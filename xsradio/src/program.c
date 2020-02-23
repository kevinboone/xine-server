/*==========================================================================

  xsradio 
  program.c

  Copyright (c)2020 Kevin Boone
  Distributed under the terms of the GPL v3.0

==========================================================================*/
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <wchar.h>
#include <time.h>
#include <ncurses.h>
#include <errno.h>
#include <locale.h>

#include "program_context.h" 
#include "feature.h" 
#include "program.h" 
#include "console.h" 
#include "usage.h" 
#include "list.h" 
#include "file.h" 
#include "numberformat.h" 
#include "../../api/xine-server-api.h" 

// Entries in the streams file are in the form url____name,
//   one station per line
#define SEPARATOR "____"

// The top row of the select window -- used to determine where a 
//   mouse click has landed
#define SELECT_WINDOW_TOP 5

/*==========================================================================

  program_update_select_window 

  Redraw the station list according to the position of the 
    selection cursor

==========================================================================*/
static void program_update_select_window (WINDOW *select_window, 
      const List *list, int first_index_on_screen, int current_selection)
  {
  werase (select_window);
  wattron(select_window, COLOR_PAIR(1)); 
  wattron(select_window, A_BOLD); 
  box (select_window, 0, 0);
  int index = first_index_on_screen;
  int row = 0;
  int height;
  int width;
  getmaxyx (select_window, height, width);
  mvwaddstr (select_window, 1, 0, "^");
  mvwaddstr (select_window, height - 2, 0, "v");
  if (list)
    {
    int l = list_length ((List *)list);
    if (l > 0)
      {
      height -= 2; // ignore box rows
      while (index < l  && row < height)
        {
        const char *stream = list_get ((List *)list, index);
	const char *sep = strstr (stream, SEPARATOR);
        const char *name = sep + 4; 
        if (current_selection == index)
          {
          wattron (select_window, A_REVERSE); 
          mvwaddnstr (select_window, row + 1, 2, name, width - 3);
          wattroff (select_window, A_REVERSE); 
          }
        else
          {
          mvwaddnstr (select_window, row + 1, 2, name, width - 3);
          }
   
        index++;
        row++;
        }
      }
    }

  wattroff (select_window, A_BOLD); 
  wrefresh (select_window); 
  }

/*==========================================================================

  program_update_status_window 

  Update the status window with information from the server. Note
   that the server can go down while this program is running, so we
   need to be prepared to handle an error response from the xineserver
   API.

==========================================================================*/
static void program_update_status_window (const char *host, int port, 
     const char *station, WINDOW *window)
  {
  wattron(window, COLOR_PAIR(1)); 
  wattron(window, A_BOLD); 
  werase (window);
  box (window, 0, 0);

  char *error;
  int error_code = 0;
  BOOL ok = TRUE;
  XSStatus *status = NULL;
  XSMetaInfo *mi = NULL;
  if (ok)
    {
    ok = xineserver_status (host, port, &status, &error_code, &error);
    }
  if (ok)
    {
    ok = xineserver_meta_info (host, port, &mi, &error_code, &error);
    }
  
  if (ok)
    {
    const char *uri = xsstatus_get_stream (status);
    if (station)
      mvwaddnstr (window, 1, 2, station, COLS - 3);
    else
      {
      if (uri && uri[0] != '-')
        mvwaddnstr (window, 1, 2, uri, COLS - 3);
      else
        mvwaddnstr (window, 1, 2, "[no station]", COLS - 3);
      }

    const char *title = xsmetainfo_get_title (mi);
    if (strcmp (title, "-") == 0)
      title = "[not playing]";
 
    mvwaddnstr (window, 2, 2, title, COLS - 3);
    const char *ts = "?";
    switch (xsstatus_get_transport_status (status))
      {
      case XINESERVER_TRANSPORT_STOPPED: ts = "stopped"; break;
      case XINESERVER_TRANSPORT_PLAYING: ts = "playing"; break;
      case XINESERVER_TRANSPORT_PAUSED: ts = "paused"; break;
      case XINESERVER_TRANSPORT_BUFFERING: ts = "buffering"; break;
      }

    int bitrate = xsmetainfo_get_bitrate (mi);
    char *s = NULL;
    asprintf (&s, "%d kbps, %s", bitrate / 1000, ts);  
    mvwaddstr (window, 3, 2, s);
    free (s);
    }
  else
    {
    mvwaddstr (window, 1, 1, error);
    free (error); 
    }

  if (mi) xsmetainfo_destroy (mi);
  if (status) xsstatus_destroy (status);

  wattroff (window, A_BOLD); 
  wrefresh (window);
  }


/*==========================================================================

  program_set_message 

  Set the contents of the message window

==========================================================================*/
static void program_set_message (WINDOW *window, const char *message)
  {
  werase (window);
  wattron(window, COLOR_PAIR(1)); 
  wattron(window, A_BOLD); 
  box (window, 0, 0);
  mvwaddnstr (window, 1, 2, message, COLS - 2);
  wattroff(window, A_BOLD); 
  wrefresh (window);
  }


/*==========================================================================

  load_stream_file
  
  Read each line of the streams file into a list of char*

==========================================================================*/
BOOL program_load_stream_file (const char *stream_file, List *list, 
       char **error)
  {
  LOG_IN
  BOOL ret = FALSE;

  FILE *f = fopen (stream_file, "r");
  if (f)
    {
    char *line = NULL;
    while (file_readline (f, &line))
      {
      const char *sep = strstr (line, SEPARATOR);
      if (sep)
        list_append (list, line);
      }  
    fclose (f);
    ret = TRUE;
    }
  else
    {
    asprintf (error, "Can't open '%s' for reading: %s", stream_file, 
       strerror (errno));
    ret = FALSE;
    }

  if (ret && list_length (list) == 0)
    {
    asprintf (error, "File '%s' contains no streams", stream_file);
    ret = FALSE;
    }

  LOG_OUT
  return ret;
  }

/*==========================================================================

  program_play_stream

  There is no specific 'play this now' function in the xinserver API.
  To play now, we must clear the playlist, add the new item, and
    then start playback from index 0.

==========================================================================*/
static BOOL program_play_stream (const char *host, int port, 
       const char *stream, char **error)
  {
  LOG_IN
  BOOL ret = FALSE;

  int error_code = 0;
  BOOL ok = TRUE;
  if (ok)
    {
    ok = xineserver_clear (host, port, &error_code, error);
    }
  if (ok)
    {
    ok = xineserver_add_single (host, port, stream, &error_code, error);
    }
  if (ok)
    {
    ok = xineserver_play (host, port, 0, &error_code, error);
    }

  ret = ok;
  LOG_OUT
  return ret;
  }
 
/*==========================================================================

  program_stop

  Stop playback

==========================================================================*/
void program_stop (const char *host, int port)
  {
  int error_code = 0;
  char *error = NULL;
  if (!xineserver_stop (host, port, &error_code, &error))
    {
    // There isn't much we can do about an error here
    log_error (error);
    free (error);
    }
  }


/*==========================================================================

  program_create_windows

  Create the three main windows on the screen

==========================================================================*/
void program_create_windows (WINDOW *main_window, WINDOW **_status_window,
    WINDOW **_select_window, WINDOW **_message_window, int *_select_height)
  {
  LOG_IN

  WINDOW *status_window = subwin (main_window, 5, COLS, 0, 0);
  int select_height = LINES - 4 - 6;
  WINDOW *select_window = subwin (main_window, select_height + 2,  
    COLS, SELECT_WINDOW_TOP, 0); 
  WINDOW *message_window = subwin (main_window, 3, COLS, LINES - 3, 0); 

  *_select_window = select_window; 
  *_status_window = status_window;
  *_message_window = message_window;
  *_select_height = select_height;

  LOG_OUT
  }


/*==========================================================================

  program_create_windows

==========================================================================*/
void show_help (WINDOW *parent, WINDOW *status_window, 
    const char *station, const char *host, int port)
  {
  WINDOW *window = subwin (parent, 13, COLS - 16, 5, 8);

  werase (window);
  box (window, 0, 0);

  mvwaddstr (window,  2, 3, "Up/Down/PgUp/PgDown: select station from list");
  mvwaddstr (window,  3, 3, "          Space bar: play/pause");
  mvwaddstr (window,  4, 3, "                U/D: volume up/down");
  mvwaddstr (window,  5, 3, "              Enter: play selected station");
  mvwaddstr (window,  6, 3, "                  Q: exit");

  mvwaddstr (window,  10, 14, "(Any key to dimiss this message)");

  wrefresh (window);

  while (getch() == ERR)
    program_update_status_window (host, port, station, status_window);

  delwin (window);
  }


/*==========================================================================

  program_play_pause

==========================================================================*/
void program_play_pause (const char *host, int port)
  {
  int error_code = 0;
  char *error = NULL;
  XSStatus *status = NULL;
  if (xineserver_status (host, port, &status, 
                            &error_code, &error))
    {
    if (xsstatus_get_transport_status (status) 
         == XINESERVER_TRANSPORT_PLAYING)
      {
      if (!xineserver_pause (host, port, &error_code, &error))
        {
        log_error (error);
        free (error);
        }
      }
    else
      {
      if (!xineserver_resume (host, port, &error_code, &error))
        {
        log_error (error);
        free (error);
        }
      }
    xsstatus_destroy (status);
    }
  else
    {
    log_error (error);
    free (error);
    }
  }


/*==========================================================================

  program_get_volume

==========================================================================*/
int program_get_volume (const char *host, int port, WINDOW *message_window)
  {
  LOG_IN
  int ret = -1;
  int error_code = 0;
  char *error = NULL;
  int vol = -1;
  if (xineserver_get_volume (host, port, &vol, 
                            &error_code, &error))
    {
    ret = vol;
    }
  else
    {
    program_set_message (message_window, error);
    free (error);
    ret = -1;
    }
  LOG_OUT
  return ret;
  }


/*==========================================================================

  program_set_volume

==========================================================================*/
void program_set_volume (const char *host, int port, int volume, 
       WINDOW *message_window)
  {
  LOG_IN
  int error_code = 0;
  char *error = NULL;
  if (!xineserver_set_volume (host, port, volume, 
        &error_code, &error))
    {
    program_set_message (message_window, error);
    free (error);
    }
  LOG_OUT
  }


/*==========================================================================

  program_volume_up

==========================================================================*/
void program_volume_up (WINDOW *message_window, const char *host, int port)
  {
  int vol = program_get_volume (host, port, message_window);
  if (vol < XINESERVER_MAX_VOLUME)
    {
    vol += 10;
    if (vol > XINESERVER_MAX_VOLUME) 
      vol = XINESERVER_MAX_VOLUME;
    program_set_volume (host, port, vol, message_window);
    }
  }


/*==========================================================================

  program_volume_down

==========================================================================*/
void program_volume_down (WINDOW *message_window, const char *host, int port)
  {
  int vol = program_get_volume (host, port, message_window);
  if (vol >= XINESERVER_MIN_VOLUME)
    {
    vol -= 10;
    if (vol < XINESERVER_MIN_VOLUME) 
      vol = XINESERVER_MIN_VOLUME;
    program_set_volume (host, port, vol, message_window);
    }
  }


/*==========================================================================

  program_handle_key

==========================================================================*/
void program_handle_key (const char *host, int port, List *streams, 
       int key, WINDOW *main_window, WINDOW *status_window,
       WINDOW *select_window, WINDOW *message_window, char **station,
       int *message_showed_for, int select_height, int *selection_index,
       int *first_index_on_screen)
  {
  switch (key)
    {
    int l;
    case 'h':
    case 'H':
	    show_help (main_window, status_window, *station, host, port); 
	    program_update_select_window (select_window, streams, 
              *first_index_on_screen, *selection_index);
	    break;
    case 'u':
    case 'U':
            program_volume_up (message_window, host, port);
	    break;
    case 'd':
    case 'D':
	    program_volume_down (message_window, host, port);
	    break;
    case ' ':
	    program_play_pause (host, port);
            program_update_status_window (host, port, *station, status_window);
	    break;
    case KEY_DOWN:
	    l = list_length (streams);
	    if (*selection_index < l - 1) (*selection_index)++;
	    if (*selection_index - *first_index_on_screen >= select_height)
	      *first_index_on_screen = *selection_index - select_height + 1; 
	    program_update_select_window (select_window, streams, 
              *first_index_on_screen, *selection_index);
	    break;
    case KEY_NPAGE:
	    l = list_length (streams);
	    *selection_index += select_height;
	    if (*selection_index >= l) *selection_index = l - 1;
	    if (*selection_index - *first_index_on_screen >= select_height)
	      *first_index_on_screen = *selection_index - select_height + 1; 
	    program_update_select_window (select_window, streams, 
              *first_index_on_screen, *selection_index);
	    break;
    case KEY_UP:
	    l = list_length (streams);
	    if (*selection_index > 0) (*selection_index)--;
	    if (*selection_index - *first_index_on_screen < 0)
	      *first_index_on_screen = *selection_index; 
	    program_update_select_window (select_window, streams, 
              *first_index_on_screen, *selection_index);
	    break;
    case KEY_PPAGE:
	    l = list_length (streams);
	    if (*selection_index > 0) *selection_index -= select_height;
	    if (*selection_index < 0) *selection_index = 0;
	    if (*selection_index - *first_index_on_screen < 0)
	      *first_index_on_screen = *selection_index; 
	    program_update_select_window (select_window, streams, 
              *first_index_on_screen, *selection_index);
	    break;
    case 10: // Enter
	    {
	    const char *_selected = list_get (streams, *selection_index);
	    char *selected = strdup (_selected);
	    char *sep = strstr (selected, SEPARATOR);
	    if (sep)
	      {
	      *sep = 0;
              char *error = NULL;
	      program_stop (host, port);
              *station = NULL;

              program_update_status_window (host, port, *station, 
                 status_window); 
              
              program_set_message (message_window, "Requesting...");
	      if (program_play_stream (host, port, selected, &error))
                {
	        char *sep = strstr (_selected, SEPARATOR);
                *station = sep + 4;
                program_set_message (message_window, "OK");
                *message_showed_for = 1;
                }
              else
                {
                program_set_message (message_window, error);
                *message_showed_for = 1;
                free (error);
                }
	      }
	    free (selected);
	    }
	    break;
    default:;
    }

  }


/*==========================================================================

  program_main_loop

==========================================================================*/
void program_main_loop (const char *host, int port, List *streams,
       int col_code)
  {
  WINDOW *main_window = initscr();
  halfdelay (10);  // 1 sec
  noecho();
  keypad (stdscr, TRUE);
  curs_set (0);
  start_color ();
  mouseinterval (0);
  init_pair (1, col_code, COLOR_BLACK);
   mousemask(ALL_MOUSE_EVENTS, NULL);
  int message_showed_for = 0;
    

  int select_height = 0;
  WINDOW *status_window = NULL;
  WINDOW *message_window = NULL;
  WINDOW *select_window = NULL;
  char *station = NULL;

  program_create_windows (main_window, &status_window, &select_window,
         &message_window, &select_height);

  program_update_status_window (host, port, station, status_window); 
  program_set_message (message_window, 
         NAME " version " VERSION " -- press 'h' for help"); 
  message_showed_for = 1;

  int selection_index = 0;
  int first_index_on_screen = 0;
  program_update_select_window (select_window, streams, 
	selection_index, first_index_on_screen);
  int ch = 0;
  while ((ch = getch ()) != 27 /* esc */ && ch != 'q' && ch != 'Q')
    {
    switch (ch)
      {
      MEVENT event;

      case KEY_MOUSE:
        if (getmouse(&event) == OK)
          {
          if (event.bstate & (BUTTON1_CLICKED || BUTTON1_PRESSED))
            {
            BOOL handled = FALSE;
            int mouse_row_offset = event.y - SELECT_WINDOW_TOP - 1;
            if (!handled)
              {
              if (event.x == 0 || event.x == 1)
                {
                if (mouse_row_offset == 0)
                  { 
                  program_handle_key (host, port, streams, KEY_PPAGE, 
                    main_window,  
                    status_window, select_window, message_window, &station,
                    &message_showed_for, select_height, &selection_index,
                    &first_index_on_screen);
                  }
                else if (mouse_row_offset == select_height - 1)
                  { 
                  program_handle_key (host, port, streams, KEY_NPAGE, 
                    main_window,  
                    status_window, select_window, message_window, &station,
                    &message_showed_for, select_height, &selection_index,
                    &first_index_on_screen);
                  }
                }
              }
            if (!handled)
              {
              // See if the mouse is in the select window
              if (event.x > 1 && 
                   mouse_row_offset >= 0 && mouse_row_offset < select_height)
                { 
                int index_hit = mouse_row_offset + first_index_on_screen;
                selection_index = index_hit;
	        program_update_select_window (select_window, streams, 
                        first_index_on_screen, selection_index);
                program_handle_key (host, port, streams, 10, main_window,  
                  status_window, select_window, message_window, &station,
                  &message_showed_for, select_height, &selection_index,
                  &first_index_on_screen);
                handled = TRUE;
                }
              }
            }
          }
      break;

      case KEY_RESIZE:
        delwin (status_window);
        delwin (select_window);
        delwin (status_window);
        program_create_windows (main_window, &status_window, &select_window,
               &message_window, &select_height);
        program_update_status_window (host, port, station, status_window);
	program_update_select_window (select_window, streams, 
          first_index_on_screen, selection_index);
        program_set_message (message_window, "");
      break;
      case ERR: // No input
        program_update_status_window (host, port, station, status_window); 
        if (message_showed_for > 0)
          {
          message_showed_for++;
          if (message_showed_for == 5) 
            {
	    program_set_message (message_window, ""); // Clear old errors
            message_showed_for = 0;
            }
          }
      break;
      default:
        program_handle_key (host, port, streams, ch, main_window,  
          status_window, select_window, message_window, &station,
          &message_showed_for, select_height, &selection_index,
          &first_index_on_screen);
      }
    }

  delwin (status_window);
  delwin (select_window);
  delwin (message_window);
  delwin (main_window);
  endwin();	
  }

/*==========================================================================

  program_parse_colour

==========================================================================*/
BOOL program_parse_colour (const char *colour, int *result)
  {
  LOG_IN
  BOOL ret = TRUE;
  if (strcmp (colour, "red") == 0) 
    *result = COLOR_RED;
  else if (strcmp (colour, "green") == 0)
    *result = COLOR_GREEN;
  else if (strcmp (colour, "yellow") == 0)
    *result = COLOR_YELLOW;
  else if (strcmp (colour, "blue") == 0)
    *result = COLOR_BLUE;
  else if (strcmp (colour, "magenta") == 0)
    *result = COLOR_MAGENTA;
  else if (strcmp (colour, "cyan") == 0)
    *result = COLOR_CYAN;
  else if (strcmp (colour, "white") == 0)
    *result = COLOR_WHITE;
  else 
    ret = FALSE;
  LOG_OUT
  return ret;
  }


/*==========================================================================

  program_run

==========================================================================*/
int program_run (ProgramContext *context)
  {
  LOG_IN
  //char ** const argv = program_context_get_nonswitch_argv (context);
  //int argc = program_context_get_nonswitch_argc (context);
  int port = program_context_get_integer (context, "port",
      XINESERVER_DEF_PORT);
  const char *host = program_context_get (context, "host");
  if (!host) host = "localhost";
  const char *stream_file = program_context_get (context, "streams");
  if (!stream_file) stream_file = SHARE "/streams.list";
  const char *colour = program_context_get (context, "colour");
  if (!colour) colour = "green"; 

  int error_code = 0;
  char *error = NULL;
  XSStatus *status = NULL;
  if (xineserver_status (host, port, &status, 
                            &error_code, &error))
    {
    xsstatus_destroy (status);
    setlocale (LC_ALL, "");

    int col_code;
    if (program_parse_colour (colour, &col_code))
      {
      List *streams = list_create (free);
      char *error = NULL;
      if (program_load_stream_file (stream_file, streams, &error))
        {
        program_main_loop (host, port, streams, col_code);
        }
      else
        {
        log_error (error);
        }
      list_destroy (streams);
      }
    else
      {
      log_error ("Can't parse colour '%s'. Acceptable colours are:\n"
        "red, green, blue, cyan, yellow, magenta, white", colour);
      }
    }
  else
    {
    log_error ("Can't connect to xine-server -- is it running?");
    free (error);
    }


  LOG_OUT
  return 0;
  }


