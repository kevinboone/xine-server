/*==========================================================================

  xine-server-api
  xine-server-api.c
  Copyright (c)2020 Kevin Boone
  Distributed under the terms of the GPL v3.0

  Implementation of the xineserver API. See the header file
  xine-server-api.h for information about what these functions
  do and the arguments they take

==========================================================================*/
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <wchar.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include "xine-server-api.h" 

/*==========================================================================

  Data structures 

==========================================================================*/
struct _XSPlaylist
  {
  int nentries;
  char **entries;
  };

struct _XSStatus
  {
  char *stream;
  int position;
  int length;
  int playlist_index;
  int playlist_length;
  XSTransportStatus transport_status;
  };

struct _XSMetaInfo
  {
  int bitrate;
  char *composer;
  char *album;
  char *genre;
  char *artist;
  char *title;
  BOOL seekable;
  };


/*==========================================================================

  xineserver_xxx functions 

==========================================================================*/
/*==========================================================================

  xineserver_tokenize_response

  This is a function that tokenizes responses from the server. These
  responses may use double-quotes to surround items -- if they
  contain spaces, for example. Double-quotes themselves can be
  escaped, if they legitimately appear in the elements of the response.
  This tokenizer can also handle single-quotes and nested quotes, as
  well as certain types of comments, but these are not used in the
  protocol at present.

==========================================================================*/
// Dunno state -- usually start of line where nothing has been read
#define STATE_DUNNO 0
// White state -- eating whitespace
#define STATE_WHITE 1
// General state -- eating normal chars
#define STATE_GENERAL 2
// dquote state -- last read was a double quote 
#define STATE_DQUOTE 3
// dquote state -- last read was a double quote 
#define STATE_ESC 4
// Comment state -- ignore until EOL
#define STATE_COMMENT 5
// General char -- anything except escape, quotes, whitespace
#define CHAR_GENERAL 0
// White char -- space or tab
#define CHAR_WHITE 1
// Double quote char 
#define CHAR_DQUOTE 2
// Double quote char 
#define CHAR_ESC 3
// Hash comment
#define CHAR_HASH 4

// Helper to maintain a variable-length string. This really ought to
//  be optimized to allocate memory in blocks.
static char *buff_append (char *buff, char c)
  {
  int l = strlen (buff);
  char *buff2 = realloc (buff, l + 2);
  buff2[l] = c;
  buff2[l+1] = 0;
  return buff2;
  }

static void xineserver_tokenize_response (const char *s, 
         int *_ntokens, char ***_tokens)
  {
  int ntokens = 0;
  char **tokens = malloc (ntokens * sizeof (char *)); 
  int i, l = strlen (s);

  char *buff = malloc (1);
  buff[0] = 0; 

  int state = STATE_DUNNO;
  int last_state = STATE_DUNNO;

  for (i = 0; i < l; i++)
    {
    char c = s[i];
    int chartype = CHAR_GENERAL;
    switch (c)
      {
      case ' ': case '\t': 
        chartype = CHAR_WHITE;
        break; 

      case '"': 
        chartype = CHAR_DQUOTE;
        break; 

      case '\\': 
        chartype = CHAR_ESC;
        break; 

      case '#': 
        chartype = CHAR_HASH;
        break; 

      default:
        chartype = CHAR_GENERAL;
      }

    //printf ("Got char %c of type %d in state %d\n", c, chartype, state);

    // Note: the number of cases should be num_state * num_char_types
    switch (1000 * state + chartype)
      {
      // --- Dunno states ---
      case 1000 * STATE_DUNNO + CHAR_GENERAL:
        buff = buff_append (buff, c);
        state = STATE_GENERAL;
        break;

      case 1000 * STATE_DUNNO + CHAR_WHITE:
        // ws at start of line, or the first ws after
        state = STATE_WHITE;
        break;

      case 1000 * STATE_DUNNO + CHAR_DQUOTE:
        // Eat the quote and go into dqote mode
        state = STATE_DQUOTE;
        break;
 
      case 1000 * STATE_DUNNO + CHAR_ESC:
        last_state = state;
        state = STATE_ESC;
        break;

      case 1000 * STATE_DUNNO + CHAR_HASH:
        last_state = state;
        state = STATE_COMMENT;
        break;

      // --- White states ---
      case 1000 * STATE_WHITE + CHAR_GENERAL:
        // Got a char while in ws
        buff = buff_append (buff, c); 
        state = STATE_GENERAL;
        break;

      case 1000 * STATE_WHITE + CHAR_WHITE:
        // Eat ws
        break;

      case 1000 * STATE_WHITE + CHAR_DQUOTE:
        // Eat the ws and go into dqote mode
        state = STATE_DQUOTE;
        break;
 
      case 1000 * STATE_WHITE + CHAR_ESC:
        last_state = state;
        state = STATE_ESC;
        break;

      case 1000 * STATE_WHITE + CHAR_HASH:
        last_state = state;
        state = STATE_COMMENT;
        break;

      // --- General states ---
      case 1000 * STATE_GENERAL + CHAR_GENERAL:
        // Eat normal char 
        buff = buff_append (buff, c);
        break;

      case 1000 * STATE_GENERAL + CHAR_WHITE:
        //Hit ws while eating characters -- this is a token
        if (strlen (buff) > 0)
          {
          tokens = realloc (tokens, (ntokens + 1) * sizeof (char *));     
          tokens[ntokens] = buff;
          ntokens++;
          }
        buff = malloc (1);
        buff[0] = 0;
        state = STATE_WHITE;
        break;

      case 1000 * STATE_GENERAL + CHAR_DQUOTE:
        // Go into dquote mode 
        state = STATE_DQUOTE;
        break;

      case 1000 * STATE_GENERAL + CHAR_ESC:
        last_state = state;
        state = STATE_ESC;
        break;

      case 1000 * STATE_GENERAL + CHAR_HASH:
        //Hit hash while eating characters -- this is a token
        if (strlen (buff) > 0)
          {
          tokens = realloc (tokens, (ntokens + 1) * sizeof (char *));     
          tokens[ntokens] = buff;
          ntokens++;
          }
        buff = malloc (1);
        buff[0] = 0;
        state = STATE_COMMENT;
        break;

      // --- Dquote states ---
      case 1000 * STATE_DQUOTE + CHAR_GENERAL:
        // Store the char, but remain in dquote mode
        buff = buff_append (buff, c);
        break;

      case 1000 * STATE_DQUOTE + CHAR_WHITE:
        // Store the ws, and remain in dquote mode
        buff = buff_append (buff, c);
        break;

      case 1000 * STATE_DQUOTE + CHAR_DQUOTE:
        // Leave dquote mode and store token (which might be empty) 
        if (strlen (buff) > 0)
          {
          tokens = realloc (tokens, (ntokens + 1) * sizeof (char *));     
          tokens[ntokens] = buff;
          ntokens++;
          }
        buff = malloc (1);
        buff[0] = 0;
        state = STATE_DUNNO;
        break;

      case 1000 * STATE_DQUOTE + CHAR_ESC:
        last_state = state;
        state = STATE_ESC;
        //string_append_byte (buff, c);
        break;

      case 1000 * STATE_DQUOTE + CHAR_HASH:
        // Keep this hash char -- it is quoted 
        buff = buff_append (buff, c);
        break;

      // --- Esc states ---
      case 1000 * STATE_ESC + CHAR_GENERAL:
        buff = buff_append (buff, c);
        state = last_state; 
        break;

      case 1000 * STATE_ESC + CHAR_WHITE:
        buff = buff_append (buff, c);
        state = last_state; 
        break;

      case 1000 * STATE_ESC + CHAR_DQUOTE:
        buff = buff_append (buff, '"');
        state = last_state; 
        break;

      case 1000 * STATE_ESC + CHAR_ESC:
        buff = buff_append (buff, '\\');
        state = last_state; 
        break;

      case 1000 * STATE_ESC + CHAR_HASH:
        buff = buff_append (buff, '#');
        state = last_state; 
        break;

      // --- Comment states ---
      case 1000 * STATE_COMMENT + CHAR_GENERAL:
        break;

      case 1000 * STATE_COMMENT + CHAR_WHITE:
        break;

      case 1000 * STATE_COMMENT + CHAR_DQUOTE:
        break;

      case 1000 * STATE_COMMENT + CHAR_ESC:
        break;

      case 1000 * STATE_COMMENT + CHAR_HASH:
        break;

      default:
        fprintf (stderr, // This should never happen
           "Internal error: bad state %d and char type %d in tokenize()",
          state, chartype);
      }
    }

  if (strlen (buff) > 0)
    {
    tokens = realloc (tokens, (ntokens + 1) * sizeof (char *));     
    tokens[ntokens] = buff;
    ntokens++;
    }
  else
    {
    if (buff) free (buff); 
    }

  *_ntokens = ntokens;
  *_tokens = tokens;
  }



/*==========================================================================

  xineserver_send_and_receive

==========================================================================*/
BOOL xineserver_send_and_receive (const char *host, 
       int port, const char *command, char **response, char **error)
  {
  BOOL ret = FALSE;

  int sock = socket (AF_INET, SOCK_STREAM, 0);  

  if (sock >= 0)
    {
    struct hostent *hostent = gethostbyname (host);
    if (hostent)
      {
      struct sockaddr_in sin;
      memcpy (&sin.sin_addr.s_addr, hostent->h_addr, hostent->h_length);
      sin.sin_family = AF_INET;
      sin.sin_port = htons (port);
      if (connect (sock, (struct sockaddr *)&sin, sizeof (sin)) == 0)
        {
        static char *buff = "\r\n";
        send (sock, command, strlen (command), 0);
        send (sock, buff, 2, 0);
        int n;
        BOOL got_line = FALSE;
        char *rbuff = malloc (1);
        rbuff[0] = 0;
        int i = 0;
        do
          {
          char buf[1];
          n = read (sock, &buf, 1);
          if (buf[0] == '\n')
            got_line = TRUE;
          else
            {
            rbuff = realloc (rbuff, i + 2);
            rbuff[i] = buf[0];
            rbuff[i + 1] = 0;
            //printf ("got %c\n", buf[0]); 
            }
          i++;
          } while (n > 0 && !got_line); 

        *response = strdup (rbuff);
        free (rbuff);
        ret = TRUE;
        }
      else
        {
        asprintf (error, "Can't connect: %s", strerror (errno));
        ret = FALSE;
        }
      }
    else
      {
      ret = FALSE;
      asprintf (error, "Can't resolve hostname: %s", hstrerror (h_errno));
      }
    close (sock);
    }
  else
   {
   ret = FALSE;
   asprintf (error, "Can't open socket");
   }

  return ret;
  }


/*==========================================================================

  xineserver_get_text_response

==========================================================================*/
static const char *xineserver_get_text_response (const char *response)
  {
  const char *ret;
  char *sp = strchr (response, ' ');
  if (sp)
    ret = sp + 1;
  else
    ret = "";
  return ret;
  }

/*==========================================================================

  xineserver_get_error_code_response

==========================================================================*/
static int xineserver_get_error_code_response (const char *response)
  {
  int code = 0;
  sscanf (response, "%d", &code);
  return code;
  }


/*==========================================================================

  xineserver_gen_command

==========================================================================*/
static BOOL xineserver_gen_command (const char *host, int port, 
        const char *command, int *error_code, char **error)
  {
  BOOL ret = FALSE;
  char *response = NULL;
  ret = xineserver_send_and_receive (host, port, command, &response, error);
  if (ret)
    {
    int _error_code = xineserver_get_error_code_response (response);
    if (_error_code != 0)
      {
      *error_code = _error_code;
      if (error)
        {
        *error = strdup (xineserver_get_text_response (response));
        }
      ret = FALSE;
      }
    free (response);
    }
  else
    *error_code = XINESERVER_ERR_COMM;
  return ret;
  }

/*==========================================================================

  xineserver_status

==========================================================================*/
BOOL xineserver_status (const char *host, int port, XSStatus **status, 
                            int *error_code, char **error)
  {
  BOOL ret = FALSE;
  char *command = NULL;
  char *response = NULL;
  asprintf (&command, "%s", XINESERVER_CMD_STATUS);
  ret = xineserver_send_and_receive (host, port, command, &response, error);
  if (ret)
    {
    int _error_code = xineserver_get_error_code_response (response);
    if (_error_code != 0)
      {
      *error_code = _error_code;
      if (error)
        {
        *error = strdup (xineserver_get_text_response (response));
        }
      ret = FALSE;
      }
    else
      {
      // Parse status out of response 
      //printf ("response = %s\n", response);
      char **tokens = NULL;
      int ntokens = 0;

      xineserver_tokenize_response 
        (xineserver_get_text_response (response), &ntokens, &tokens); 

      if (ntokens == 6)
        { 
        XSStatus *_status = malloc (sizeof (XSStatus)); 

        const char *ts = tokens[0];
        if (strcmp (ts, "playing") == 0)
           _status->transport_status = XINESERVER_TRANSPORT_PLAYING;
        else if (strcmp (ts, "paused") == 0)
           _status->transport_status = XINESERVER_TRANSPORT_PAUSED;
        else if (strcmp (ts, "buffering") == 0)
           _status->transport_status = XINESERVER_TRANSPORT_BUFFERING;
        else 
           _status->transport_status = XINESERVER_TRANSPORT_STOPPED;
        _status->position = atoi (tokens[1]);
        _status->length = atoi (tokens[2]);
        _status->stream = strdup (tokens[3]);
        _status->playlist_index = atoi (tokens[4]);
        _status->playlist_length = atoi (tokens[5]);

        *status = _status;
        ret = TRUE;
        }
      else 
        {
        *error_code = XINESERVER_ERR_RESPONSE;
        }
      for (int i = 0; i < ntokens; i++) free (tokens[i]);
      free (tokens);
      }
    free (response);
    }
  else
    *error_code = XINESERVER_ERR_COMM;
  free (command);
  return ret;
  }

/*==========================================================================

  xineserver_get_eq

==========================================================================*/
BOOL xineserver_get_eq (const char *host, int port, int eq[10],
                            int *error_code, char **error)
  {
  BOOL ret = FALSE;
  char *command = NULL;
  char *response = NULL;
  asprintf (&command, "%s", XINESERVER_CMD_EQ);
  ret = xineserver_send_and_receive (host, port, command, &response, error);
  if (ret)
    {
    int _error_code = xineserver_get_error_code_response (response);
    if (_error_code != 0)
      {
      *error_code = _error_code;
      if (error)
        {
        *error = strdup (xineserver_get_text_response (response));
        }
      ret = FALSE;
      }
    else
      {
      // Parse status out of response 
      //printf ("response = %s\n", response);
      char **tokens = NULL;
      int ntokens = 0;

      xineserver_tokenize_response 
        (xineserver_get_text_response (response), &ntokens, &tokens); 

      if (ntokens == 10)
        { 
        for (int i = 0; i < 10; i++)
          eq[i] = atoi (tokens[i]);
        ret = TRUE;
        }
      else 
        {
        *error_code = XINESERVER_ERR_RESPONSE;
        *error = strdup ("Incorrect number of tokens in response from server");
        ret = FALSE;
        }
      for (int i = 0; i < ntokens; i++) free (tokens[i]);
      free (tokens);
      }
    free (response);
    }
  else
    *error_code = XINESERVER_ERR_COMM;
  free (command);
  return ret;

  }


/*==========================================================================

  xineserver_set_eq

==========================================================================*/
BOOL xineserver_set_eq (const char *host, int port, int eq[10],
                            int *error_code, char **error)
  {
  BOOL ret = FALSE;
  char *command = NULL;
  char *response = NULL;
  asprintf (&command, "%s %d %d %d %d %d %d %d %d %d %d", 
     XINESERVER_CMD_EQ, eq[0], eq[1], eq[2], eq[3], eq[4],
       eq[5], eq[6], eq[7], eq[8], eq[9]);
  ret = xineserver_send_and_receive (host, port, command, &response, error);
  if (ret)
    {
    int _error_code = xineserver_get_error_code_response (response);
    if (_error_code != 0)
      {
      *error_code = _error_code;
      if (error)
        {
        *error = strdup (xineserver_get_text_response (response));
        }
      ret = FALSE;
      }
    free (response);
    }
  else
    *error_code = XINESERVER_ERR_COMM;
  free (command);
  return ret;

  }


/*==========================================================================

  xineserver_meta_info

==========================================================================*/
BOOL xineserver_meta_info (const char *host, int port, XSMetaInfo **mi, 
                            int *error_code, char **error)
  {
  BOOL ret = FALSE;
  char *command = NULL;
  char *response = NULL;
  asprintf (&command, "%s", XINESERVER_CMD_META_INFO);
  ret = xineserver_send_and_receive (host, port, command, &response, error);
  if (ret)
    {
    int _error_code = xineserver_get_error_code_response (response);
    if (_error_code != 0)
      {
      *error_code = _error_code;
      if (error)
        {
        *error = strdup (xineserver_get_text_response (response));
        }
      ret = FALSE;
      }
    else
      {
      // Parse status out of response 
      //printf ("response = %s\n", response);
      char **tokens = NULL;
      int ntokens = 0;

      xineserver_tokenize_response 
        (xineserver_get_text_response (response), &ntokens, &tokens); 

      if (ntokens == 7)
        { 
        XSMetaInfo *_mi = malloc (sizeof (XSMetaInfo)); 

        _mi->bitrate = atoi (tokens[0]);
        _mi->seekable = atoi (tokens[1]);
        _mi->title = strdup (tokens[2]);
        _mi->artist = strdup (tokens[3]);
        _mi->genre = strdup (tokens[4]);
        _mi->album = strdup (tokens[5]);
        _mi->composer = strdup (tokens[6]);

        *mi = _mi;
        ret = TRUE;
        }
      else 
        {
        *error_code = XINESERVER_ERR_RESPONSE;
        *error = strdup ("Incorrect number of tokens in response from server");
        ret = FALSE;
        }
      for (int i = 0; i < ntokens; i++) free (tokens[i]);
      free (tokens);
      }
    free (response);
    }
  else
    *error_code = XINESERVER_ERR_COMM;
  free (command);
  return ret;
  }


/*==========================================================================

  xineserver_playlist

==========================================================================*/
BOOL xineserver_playlist (const char *host, int port, XSPlaylist **playlist, 
                            int *error_code, char **error)
  {
  BOOL ret = FALSE;
  char *command = NULL;
  char *response = NULL;
  asprintf (&command, "%s", XINESERVER_CMD_PLAYLIST);
  ret = xineserver_send_and_receive (host, port, command, &response, error);
  if (ret)
    {
    int _error_code = xineserver_get_error_code_response (response);
    if (_error_code != 0)
      {
      *error_code = _error_code;
      if (error)
        {
        *error = strdup (xineserver_get_text_response (response));
        }
      ret = FALSE;
      }
    else
      {
      // Parse playlist out of response 
      //printf ("response = %s\n", response);
      char **tokens = NULL;
      int ntokens = 0;
      xineserver_tokenize_response 
        (xineserver_get_text_response (response), &ntokens, &tokens); 
      XSPlaylist *pl = malloc (sizeof (XSPlaylist)); 
      pl->nentries = ntokens;
      pl->entries = tokens;
      *playlist = pl;
      ret = TRUE;
      }
    free (response);
    }
  else
    *error_code = XINESERVER_ERR_COMM;
  free (command);
  return ret;
  }


/*==========================================================================

  xineserver_add_single 

==========================================================================*/
BOOL xineserver_add (const char *host, int port, int nstreams, 
                            const char *const *streams, 
                            int *error_code, char **error)
  {
  BOOL ret = FALSE;
  char *command = strdup (XINESERVER_CMD_ADD); 
  for (int i = 0; i < nstreams; i++)
    {
    const char *stream = streams[i]; // TODO -- escape

    command = realloc (command, strlen (command) + strlen(stream) + 10);   

    strcat (command, " \"");
    strcat (command, stream);
    strcat (command, "\"");
    }

  ret = xineserver_gen_command (host, port, command, error_code, error);
  free (command);
  return ret;
  }

/*==========================================================================

  xineserver_add_single 

==========================================================================*/
BOOL xineserver_add_single (const char *host, int port, const char *stream, 
        int *error_code, char **error)
  {
  BOOL ret = FALSE;
  char *command = NULL;
  // TODO -- escape quotes in stream
  asprintf (&command, "%s \"%s\"", XINESERVER_CMD_ADD, stream);
  ret = xineserver_gen_command (host, port, command, error_code, error);
  free (command);
  return ret;
  }

/*==========================================================================

  xineserver_shutdown

==========================================================================*/
BOOL xineserver_shutdown (const char *host, int port, 
        int *error_code, char **error)
  {
  BOOL ret = FALSE;
  char *command = NULL;
  asprintf (&command, "%s", XINESERVER_CMD_SHUTDOWN);
  ret = xineserver_gen_command (host, port, command, error_code, error);
  free (command);
  return ret;
  }

/*==========================================================================

  xineserver_stop

==========================================================================*/
BOOL xineserver_stop (const char *host, int port, 
        int *error_code, char **error)
  {
  BOOL ret = FALSE;
  char *command = NULL;
  asprintf (&command, "%s", XINESERVER_CMD_STOP);
  ret = xineserver_gen_command (host, port, command, error_code, error);
  free (command);
  return ret;
  }

/*==========================================================================

  xineserver_pause

==========================================================================*/
BOOL xineserver_pause (const char *host, int port, 
        int *error_code, char **error)
  {
  BOOL ret = FALSE;
  char *command = NULL;
  asprintf (&command, "%s", XINESERVER_CMD_PAUSE);
  ret = xineserver_gen_command (host, port, command, error_code, error);
  free (command);
  return ret;
  }


/*==========================================================================

  xineserver_next

==========================================================================*/
BOOL xineserver_next (const char *host, int port, 
        int *error_code, char **error)
  {
  BOOL ret = FALSE;
  char *command = NULL;
  asprintf (&command, "%s", XINESERVER_CMD_NEXT);
  ret = xineserver_gen_command (host, port, command, error_code, error);
  free (command);
  return ret;
  }


/*==========================================================================

  xineserver_prev

==========================================================================*/
BOOL xineserver_prev (const char *host, int port, 
        int *error_code, char **error)
  {
  BOOL ret = FALSE;
  char *command = NULL;
  asprintf (&command, "%s", XINESERVER_CMD_PREV);
  ret = xineserver_gen_command (host, port, command, error_code, error);
  free (command);
  return ret;
  }


/*==========================================================================

  xineserver_set_volume

==========================================================================*/
BOOL xineserver_set_volume (const char *host, int port, int volume,
        int *error_code, char **error)
  {
  BOOL ret = FALSE;
  char *command = NULL;
  asprintf (&command, "%s %d", XINESERVER_CMD_VOLUME, volume);
  ret = xineserver_gen_command (host, port, command, error_code, error);
  free (command);
  return ret;
  }


/*==========================================================================

  xineserver_set_volume

==========================================================================*/
BOOL xineserver_get_volume (const char *host, int port, int *volume,
        int *error_code, char **error)
  {
  BOOL ret = FALSE;
  char *command = NULL;
  asprintf (&command, "%s", XINESERVER_CMD_VOLUME);

  char *response = NULL;
  ret = xineserver_send_and_receive (host, port, command, &response, error);
  if (ret)
    {
    int _error_code = xineserver_get_error_code_response (response);
    if (_error_code != 0)
      {
      *error_code = _error_code;
      if (error)
        {
        *error = strdup (xineserver_get_text_response (response));
        }
      ret = FALSE;
      }
    else
      {
      *volume = atoi (xineserver_get_text_response (response)); 
      ret = TRUE;
      }
    free (response);
    }
  else
    *error_code = XINESERVER_ERR_COMM;
  free (command);
  return ret;
  }


/*==========================================================================

  xineserver_version

==========================================================================*/
BOOL xineserver_version (const char *host, int port, int *major, 
                            int *minor, int *error_code, char **error)
  {
  BOOL ret = FALSE;
  char *command = NULL;
  asprintf (&command, "%s", XINESERVER_CMD_VERSION);

  char *response = NULL;
  ret = xineserver_send_and_receive (host, port, command, &response, error);
  if (ret)
    {
    int _error_code = xineserver_get_error_code_response (response);
    if (_error_code != 0)
      {
      *error_code = _error_code;
      if (error)
        {
        *error = strdup (xineserver_get_text_response (response));
        }
      ret = FALSE;
      }
    else
      {
      sscanf (xineserver_get_text_response (response), "%d.%d", major, minor); 
      ret = TRUE;
      }
    free (response);
    }
  else
    *error_code = XINESERVER_ERR_COMM;
  free (command);
  return ret;
  }

/*==========================================================================

  xineserver_resume

==========================================================================*/
BOOL xineserver_resume (const char *host, int port, 
                            int *error_code, char **error)
  {
  BOOL ret = FALSE;
  char *command = NULL;
  asprintf (&command, "%s", XINESERVER_CMD_PLAY);
  ret = xineserver_gen_command (host, port, command, error_code, error);
  free (command);
  return ret;
  }

/*==========================================================================

  xineserver_play

==========================================================================*/
BOOL xineserver_play (const char *host, int port, int index,
                            int *error_code, char **error)
  {
  BOOL ret = FALSE;
  char *command = NULL;
  asprintf (&command, "%s %d", XINESERVER_CMD_PLAY, index);
  ret = xineserver_gen_command (host, port, command, error_code, error);
  free (command);
  return ret;
  }

/*==========================================================================

  xineserver_seek

==========================================================================*/
BOOL xineserver_seek (const char *host, int port, int msec,
                            int *error_code, char **error)
  {
  BOOL ret = FALSE;
  char *command = NULL;
  asprintf (&command, "%s %d", XINESERVER_CMD_SEEK, msec);
  ret = xineserver_gen_command (host, port, command, error_code, error);
  free (command);
  return ret;
  }

/*==========================================================================

  xineserver_clear

==========================================================================*/
BOOL xineserver_clear (const char *host, int port, 
        int *error_code, char **error)
  {
  BOOL ret = FALSE;
  char *command = NULL;
  asprintf (&command, "%s", XINESERVER_CMD_CLEAR);
  ret = xineserver_gen_command (host, port, command, error_code, error);
  free (command);
  return ret;
  }

/*==========================================================================

  xsplaylist_xxx functions 

==========================================================================*/
/*==========================================================================

  xsplaylist_destroy

==========================================================================*/
void xsplaylist_destroy  (XSPlaylist *self)
  {
  if (self)
    {
    if (self->entries)
      {
      for (int i = 0; i < self->nentries; i++)
        free (self->entries[i]);
      free (self->entries);
      }
    free (self);
    }
  }


/*==========================================================================

  xsplaylist_get_nentries

==========================================================================*/
int xsplaylist_get_nentries (const XSPlaylist *self)
  {
  return self->nentries;
  }


/*==========================================================================

  xsplaylist_get_entries

==========================================================================*/
char** const xsplaylist_get_entries (const XSPlaylist *self)
  {
  return self->entries;
  }


/*==========================================================================

  xsstatys_xxx functions 

==========================================================================*/
/*==========================================================================

  xsstatus_destroy

==========================================================================*/
void xsstatus_destroy (XSStatus *self)
  {
  if (self)
    {
    if (self->stream) free (self->stream);
    free (self);
    }
  }

/*==========================================================================

  xsstatus_get_position

==========================================================================*/
int xsstatus_get_position (const XSStatus *self)
  {
  return self->position;
  }

/*==========================================================================

  xsstatus_get_length

==========================================================================*/
int xsstatus_get_length (const XSStatus *self)
  {
  return self->length;
  }

/*==========================================================================

  xsstatus_get_playlist_length

==========================================================================*/
int xsstatus_get_playlist_length (const XSStatus *self)
  {
  return self->playlist_length;
  }

/*==========================================================================

  xsstatus_get_playlist_index

==========================================================================*/
int xsstatus_get_playlist_index (const XSStatus *self)
  {
  return self->playlist_index;
  }

/*==========================================================================

  xsstatus_get_stream

==========================================================================*/
const char *xsstatus_get_stream (const XSStatus *self)
  {
  return self->stream;
  }

/*==========================================================================

  xsstatus_get_transport_status

==========================================================================*/
XSTransportStatus xsstatus_get_transport_status (const XSStatus *self)
  {
  return self->transport_status;
  }

/*==========================================================================

  xsstatus_get_position_hms

==========================================================================*/
void xineserver_msec_to_hms (int msec, int *h, int *m, int *s)
  {
  int sec = msec / 1000;
  *h = sec / 3600;
  int rem = sec - *h * 3600;
  *m  = rem / 60;
  *s = sec - *m * 60 - *h * 3600;
  } 

/*==========================================================================

  xsstatus_get_position_hms

==========================================================================*/
void xsstatus_get_position_hms (const XSStatus *self, char *buff,
    int bufflen)
  {
  int h, m, s;
  xineserver_msec_to_hms (self->position, &h, &m, &s);
  snprintf (buff, bufflen, "%02d:%02d:%02d", h, m, s);
  }


/*==========================================================================

  xsstatus_get_length_hms

==========================================================================*/
void xsstatus_get_length_hms (const XSStatus *self, char *buff,
     int bufflen)
  {
  int h, m, s;
  xineserver_msec_to_hms (self->length, &h, &m, &s);
  snprintf (buff, bufflen, "%02d:%02d:%02d", h, m, s);
  }

/*==========================================================================

  XSMetaInfo methods 

==========================================================================*/
/*==========================================================================

  xsmetainfo_destroy

==========================================================================*/
void xsmetainfo_destroy (XSMetaInfo *self)
  {
  if (self)
    {
    if (self->composer) free (self->composer);
    if (self->album) free (self->album);
    if (self->genre) free (self->genre);
    if (self->artist) free (self->artist);
    if (self->title) free (self->title);
    free (self);
    }
  }

/*==========================================================================

  xsmetainfo_get_bitrate

==========================================================================*/
int xsmetainfo_get_bitrate (const XSMetaInfo *self)
  {
  return self->bitrate;
  }

/*==========================================================================

  xsmetainfo_get_composer

==========================================================================*/
const char *xsmetainfo_get_composer (const XSMetaInfo *self)
  {
  return self->composer;
  }

/*==========================================================================

  xsmetainfo_get_album

==========================================================================*/
const char *xsmetainfo_get_album (const XSMetaInfo *self)
  {
  return self->album;
  }

/*==========================================================================

  xsmetainfo_get_genre

==========================================================================*/
const char *xsmetainfo_get_genre (const XSMetaInfo *self)
  {
  return self->genre;
  }

/*==========================================================================

  xsmetainfo_get_artist

==========================================================================*/
const char *xsmetainfo_get_artist (const XSMetaInfo *self)
  {
  return self->artist;
  }

/*==========================================================================

  xsmetainfo_get_title

==========================================================================*/
const char *xsmetainfo_get_title (const XSMetaInfo *self)
  {
  return self->title;
  }

/*==========================================================================

  xsmetainfo_is_seekable

==========================================================================*/
BOOL xsmetainfo_is_seekable (const XSMetaInfo *self)
  {
  return self->seekable;
  }



