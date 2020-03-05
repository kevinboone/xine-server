/*==========================================================================

  xine-server 
  cmdproc.c
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
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <pthread.h>
#include <xine.h>
#include "feature.h" 
#include "defs.h" 
#include "log.h" 
#include "string.h" 
#include "numberformat.h" 
#include "cmdproc.h" 
#include "../../api/xine-server-api.h" 
#include "xine_interface.h" 
#include "notifier.h" 

#define OK_RESPONSE "0 OK\n"

struct _CmdProc
  {
  BOOL request_quit;
  XineInterface *xi;
  List *playlist; // List of char *
  // Position in the playlist. The first item is zero. When there is
  //   no playlist, or playback has been stopped, the position
  //   is -1
  int playlist_index;
  // We need a mutex to controll access to the playlist. Otherwise 
  //   one thread could be playing the next item in the playlist,
  //   while another is clearing it
  pthread_mutex_t playlist_mutex;
  Notifier *notifier;
  }; 

static BOOL cmdproc_play_playlist_entry (CmdProc *self, int index, 
        int *error_code, char **error); //Forward
static void cmdproc_notify_playback_finished (void *arg); // Forward
static void cmdproc_notify_progress (const char *msg, int percent, 
      void *arg); // Forward

/*==========================================================================

  cmdproc_create

==========================================================================*/
CmdProc *cmdproc_create (XineInterface *xi, Notifier *notifier)
  {
  LOG_IN
  log_debug ("%s: Creating command processor", __PRETTY_FUNCTION__); 
  CmdProc *self = malloc (sizeof (CmdProc));
  self->request_quit = FALSE;
  self->xi = xi;
  self->playlist = list_create (free);
  self->playlist_index = -1;
  self->notifier = notifier;
  pthread_mutexattr_t attr;
  pthread_mutexattr_init(&attr);
  pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
  pthread_mutex_init (&self->playlist_mutex, &attr);
  xine_interface_set_playback_finished_fn (self->xi,
                 cmdproc_notify_playback_finished, self);
  xine_interface_set_progress_fn (self->xi,
                 cmdproc_notify_progress, self);

  LOG_OUT
  return self;
  }


/*==========================================================================

  cmdproc_destroy

==========================================================================*/
void cmdproc_destroy (CmdProc *self)
  {
  LOG_IN
  log_debug ("%s: Destroying  command processor", __PRETTY_FUNCTION__); 
  if (self)
    {
    if (self->playlist)
      list_destroy (self->playlist);
    free (self);
    }
  LOG_OUT
  }


/*==========================================================================

  cmdproc_cmd_playback_stopped

  Don't change the playlist index, because this function might be
   called in response to a notification that a track has finished.
  In that case, we'll need the current playlist index, to play the
   next track.

==========================================================================*/
void cmdproc_set_playback_stopped (CmdProc *self)
  {
  }


/*==========================================================================

  cmdproc_cmd_playlist

==========================================================================*/
static void cmdproc_cmd_playlist (CmdProc *self, List *argv, char **response)
  {
  LOG_IN
  String *s_response = string_create ("0");
  pthread_mutex_lock (&self->playlist_mutex);
  int l = list_length (self->playlist);
  for (int i = 0; i < l; i++)
    {
    const char *entry = list_get (self->playlist, i);
    String *s_entry = string_create (entry);
    String *esc_entry = string_substitute_all (s_entry, "\"", "\\\"");
    string_append (s_response, " \"");
    string_append (s_response, string_cstr (esc_entry));
    string_append (s_response, "\"");
    string_destroy (s_entry);
    string_destroy (esc_entry);
    }
  pthread_mutex_unlock (&self->playlist_mutex);

  string_append (s_response, "\n");
  *response = strdup (string_cstr (s_response)); 
  string_destroy (s_response);
  LOG_OUT
  }

  
/*==========================================================================

  cmdproc_cmd_stop

==========================================================================*/
static void cmdproc_cmd_stop (CmdProc *self, List *argv, char **response)
  {
  LOG_IN
  
  // Don't invoke the notifier here -- stop_playback() will do it
  log_debug ("%s Stopping playback on client request", __PRETTY_FUNCTION__); 
  cmdproc_stop_playback (self);
  asprintf (response, OK_RESPONSE); 

  LOG_OUT
  }


/*==========================================================================

  cmdproc_cmd_pause

==========================================================================*/
static void cmdproc_cmd_pause (CmdProc *self, List *argv, char **response)
  {
  LOG_IN

  log_debug ("%s Pausing playback on client request", __PRETTY_FUNCTION__); 
  xine_interface_pause (self->xi);
  notifier_notify (self->notifier, XSNOTIFY_CLASS_TRANSPORT,
       XSNOTIFY_EVENT_PLAYBACK_PAUSED, NOTIFY_MSG_PAUSED_PLAYBACK); 

  asprintf (response, OK_RESPONSE); 
  LOG_OUT
  }

/*==========================================================================

  cmdproc_cmd_prev

==========================================================================*/
static void cmdproc_cmd_prev (CmdProc *self, List *argv, char **response)
  {
  LOG_IN

  pthread_mutex_lock (&self->playlist_mutex);

  log_debug ("%s Previous item on client request", __PRETTY_FUNCTION__); 
  int index = self->playlist_index;
  if (index > 0)
    {
    index--;
    int error_code;
    char *error = NULL;
    log_debug ("%s: Moving to playlist item %d", __PRETTY_FUNCTION__, index);
    if (cmdproc_play_playlist_entry (self, index, &error_code,
            &error))
      {
      notifier_notify (self->notifier, XSNOTIFY_CLASS_TRANSPORT,
         XSNOTIFY_EVENT_CHANGED_PL_POSITION, NOTIFY_MSG_CHANGED_PL_POSITION); 
      asprintf (response, OK_RESPONSE); 
      self->playlist_index = index;
      }
    else
      {
      asprintf (response, "%d %s\n", error_code, error); 
      free (error);
      }
    }
  else
    {
    asprintf (response, "%d At start of playlist\n", 
       XINESERVER_ERR_PLAYLIST_START); 
    }

  pthread_mutex_unlock (&self->playlist_mutex);
  LOG_OUT
  }

/*==========================================================================

  cmdproc_cmd_next

==========================================================================*/
static void cmdproc_cmd_next (CmdProc *self, List *argv, char **response)
  {
  LOG_IN

  log_debug ("%s Next item on client request", __PRETTY_FUNCTION__); 
  pthread_mutex_lock (&self->playlist_mutex);
  int length = list_length (self->playlist);
  int index = self->playlist_index;
  if (index < length - 1 && length > 0)
    {
    index++;
    int error_code;
    char *error = NULL;
    log_debug ("%s: Moving to playlist item %d", __PRETTY_FUNCTION__, index);
    if (cmdproc_play_playlist_entry (self, index, &error_code,
            &error))
      {
      notifier_notify (self->notifier, XSNOTIFY_CLASS_TRANSPORT,
         XSNOTIFY_EVENT_CHANGED_PL_POSITION, NOTIFY_MSG_CHANGED_PL_POSITION); 
      asprintf (response, OK_RESPONSE); 
      self->playlist_index = index;
      }
    else
      {
      asprintf (response, "%d %s\n", error_code, error); 
      free (error);
      }
    }
  else
    {
    asprintf (response, "%d At end of playlist\n", 
       XINESERVER_ERR_PLAYLIST_END); 
    }

  pthread_mutex_unlock (&self->playlist_mutex);
  LOG_OUT
  }

/*==========================================================================

  cmdproc_cmd_clear

==========================================================================*/
static void cmdproc_cmd_clear (CmdProc *self, List *argv, char **response)
  {
  LOG_IN

  log_info ("Clearing playlist and stopping playback");
  log_debug ("%s Clearing on client request", __PRETTY_FUNCTION__); 
  notifier_notify (self->notifier, XSNOTIFY_CLASS_PLAYLIST,
     XSNOTIFY_EVENT_PL_CHANGED, NOTIFY_MSG_PL_CHANGED); 
  notifier_notify (self->notifier, XSNOTIFY_CLASS_TRANSPORT,
     XSNOTIFY_EVENT_PLAYBACK_STOPPED, NOTIFY_MSG_STOPPED_PLAYBACK); 
  pthread_mutex_lock (&self->playlist_mutex);
  xine_interface_stop (self->xi);
  list_destroy (self->playlist);
  self->playlist = list_create (free);
  self->playlist_index = -1;

  pthread_mutex_unlock (&self->playlist_mutex);
  asprintf (response, OK_RESPONSE); 
  LOG_OUT
  }

/*==========================================================================

  cmdproc_cmd_volume

==========================================================================*/
static void cmdproc_cmd_volume (CmdProc *self, List *argv, char **response)
  {
  LOG_IN

  if (list_length (argv) == 2)
    {
    log_debug ("%s Setting volume on client request", __PRETTY_FUNCTION__); 
    const char *s_vol = string_cstr (list_get (argv, 1));
    int vol = atoi (s_vol);
    log_debug ("%s: set volume: %d", __PRETTY_FUNCTION__, vol);
    notifier_notify (self->notifier, XSNOTIFY_CLASS_AUDIO,
       XSNOTIFY_EVENT_VOLUME_CHANGED, NOTIFY_MSG_VOLUME_CHANGED); 
    xine_interface_set_volume (self->xi, vol);
    asprintf (response, OK_RESPONSE);
    }
  else
    {
    int v = xine_interface_get_volume (self->xi);
    asprintf (response, "%d %d\n", 0, v); 
    }
  LOG_OUT
  }

/*==========================================================================

  cmdproc_cmd_eq

==========================================================================*/
static void cmdproc_cmd_eq (CmdProc *self, List *argv, char **response)
  {
  LOG_IN

  if (list_length (argv) == 11)
    {
    log_debug ("%s Setting eq on client request", __PRETTY_FUNCTION__); 
    int eq[10];
    for (int i = 0; i < 10; i++)
      {
      eq[i] = atoi (string_cstr(list_get (argv, i+1))); 
      }
    xine_interface_set_eq (self->xi, eq);
    asprintf (response, OK_RESPONSE);
    }
  else if (list_length (argv) == 1) 
    {
    int eq[10];
    xine_interface_get_eq (self->xi, eq);
    asprintf (response, "%d %d %d %d %d %d %d %d %d %d %d\n", 0, 
         eq[0], eq[1], eq[2], eq[3], eq[4], eq[5], 
         eq[6], eq[7], eq[8], eq[9]); 
    }
  else
    {
    asprintf (response, "%d eq command takes ten arguments or none\n", 
       XINESERVER_ERR_SYNTAX);
    }
  LOG_OUT
  }

/*==========================================================================

  cmdproc_cmd_seek

==========================================================================*/
static void cmdproc_cmd_seek (CmdProc *self, List *argv, char **response)
  {
  LOG_IN

  log_debug ("%s Seeking on client request", __PRETTY_FUNCTION__); 
  if (list_length (argv) == 2)
    {
    const char *s_msec = string_cstr (list_get (argv, 1));
    int msec = atoi (s_msec);
    log_debug ("%s: seek: %d msec", __PRETTY_FUNCTION__, msec);
    xine_interface_seek (self->xi, msec);
    notifier_notify (self->notifier, XSNOTIFY_CLASS_TRANSPORT,
       XSNOTIFY_EVENT_SEEK, NOTIFY_MSG_SEEK); 
    asprintf (response, OK_RESPONSE);
    }
  else
    {
    asprintf (response, "%d seek command takes one argument\n", 
       XINESERVER_ERR_SYNTAX);
    }
  LOG_OUT
  }

/*==========================================================================

  cmdproc_escape_quotes

==========================================================================*/
char *cmdproc_escape_quotes (const char *s)
  {
  LOG_IN
  String *s_s = string_create (s);
  String *e_s = string_substitute_all (s_s, "\"", "\\\"");
  string_destroy (s_s);
  char *esc_s = strdup (string_cstr (e_s));
  string_destroy (e_s);
  LOG_OUT
  return esc_s;
  }


/*==========================================================================

  cmdproc_cmd_meta_info

==========================================================================*/
static void cmdproc_cmd_meta_info (CmdProc *self, List *argv, char **response)
  {
  LOG_IN

  log_debug ("%s Getting meta-info on client request", __PRETTY_FUNCTION__); 
  const char *title = xine_interface_get_meta_info 
      (self->xi, XINE_META_INFO_TITLE);
  char *esc_title = cmdproc_escape_quotes (title); 
  const char *artist = xine_interface_get_meta_info 
      (self->xi, XINE_META_INFO_ARTIST);
  char *esc_artist = cmdproc_escape_quotes (artist); 
  const char *genre = xine_interface_get_meta_info 
      (self->xi, XINE_META_INFO_GENRE);
  char *esc_genre = cmdproc_escape_quotes (genre); 
  const char *album = xine_interface_get_meta_info 
      (self->xi, XINE_META_INFO_ALBUM);
  char *esc_album = cmdproc_escape_quotes (album); 
  const char *composer = xine_interface_get_meta_info 
      (self->xi, XINE_META_INFO_COMPOSER);
  char *esc_composer = cmdproc_escape_quotes (composer); 
  uint32_t bitrate = xine_interface_get_stream_info 
      (self->xi, XINE_STREAM_INFO_AUDIO_BITRATE);
  uint32_t seekable = xine_interface_get_stream_info 
      (self->xi, XINE_STREAM_INFO_SEEKABLE);

  asprintf (response, "0 %d %d \"%s\" \"%s\" \"%s\" \"%s\" \"%s\" \n",
      bitrate, seekable, esc_title, esc_artist, esc_genre, 
        esc_album, esc_composer);

  free (esc_title);
  free (esc_artist);
  free (esc_genre);
  free (esc_album);
  free (esc_composer);
  
  LOG_OUT
  }

/*==========================================================================

  cmdproc_cmd_status

==========================================================================*/
static void cmdproc_cmd_status (CmdProc *self, List *argv, char **response)
  {
  LOG_IN

  log_debug ("%s Getting status on client request", __PRETTY_FUNCTION__); 
  int playlist_length = list_length (self->playlist);
  const char *stream = "-";
  if (self->playlist_index >= 0 && self->playlist_index < playlist_length)
    stream = list_get (self->playlist, self->playlist_index);

  int pos, len;
  xine_interface_get_pos_len (self->xi, &pos, &len);
  XSTransportStatus status = xine_interface_get_transport_status (self->xi);
  switch (status)
    {
    case XINESERVER_TRANSPORT_PLAYING:
      asprintf (response, "0 playing %d %d \"%s\" %d %d\n", pos, len, 
	stream, self->playlist_index, playlist_length); 
      break;
    case XINESERVER_TRANSPORT_STOPPED:
      asprintf (response, "0 stopped %d %d \"%s\" %d %d\n", pos, len, 
	stream, self->playlist_index, playlist_length); 
      break;
    case XINESERVER_TRANSPORT_PAUSED:
      asprintf (response, "0 paused %d %d \"%s\" %d %d\n", pos, len, 
	stream, self->playlist_index, playlist_length); 
      break;
    case XINESERVER_TRANSPORT_BUFFERING:
      asprintf (response, "0 buffering %d %d \"%s\" %d %d\n", pos, len, 
	stream, self->playlist_index, playlist_length); 
      break;
    }

  LOG_OUT
  }


/*==========================================================================

  cmdproc_cmd_add

==========================================================================*/
static void cmdproc_cmd_add (CmdProc *self, List *argv, char **response)
  {
  LOG_IN
  
  log_debug ("%s Adding to playlist on client request", __PRETTY_FUNCTION__); 
  BOOL added = FALSE;
  int argc = list_length (argv);
  BOOL error = FALSE;
  if (argc >= 2)
    {
    for (int i = 1; i < argc && !error; i++)
      { 
      const char *stream = string_cstr (list_get (argv, i));
      log_debug ("add to playlist: %s", stream);
      if ((stream[0] == '/' && (access (stream, R_OK) == 0))
	  || (stream[0] != '/'))
	{
	added = TRUE;
	list_append (self->playlist, strdup (stream));
	}
      else
	{
	log_warning ("%s: File not found: %s", __PRETTY_FUNCTION__, 
	  stream);
	asprintf (response, "%d File not found %s\n", 
	  XINESERVER_ERR_NOFILE, stream);
        error = TRUE;
	}
      }
    if (!error)
      asprintf (response, OK_RESPONSE); 

    if (added)
      notifier_notify (self->notifier, XSNOTIFY_CLASS_PLAYLIST,
         XSNOTIFY_EVENT_PL_CHANGED, NOTIFY_MSG_PL_CHANGED); 
    }
  else
    {
    asprintf (response, "%d add command takes one or more argument\n", 
       XINESERVER_ERR_SYNTAX);
    }

  LOG_OUT
  }


/*==========================================================================

  cmdproc_play_playlist_entry

==========================================================================*/
static BOOL cmdproc_play_playlist_entry (CmdProc *self, int index, 
        int *error_code, char **error)
  {
  LOG_IN
  BOOL ret = FALSE;
  log_debug ("%s Play item %d on client request", __PRETTY_FUNCTION__,
        index); 
  pthread_mutex_lock (&self->playlist_mutex);
  if (list_length (self->playlist) > 0) 
    {
    log_debug ("%s: Playlist is not empty", __PRETTY_FUNCTION__);
    if (index >= 0 && index < list_length (self->playlist))
      {
      log_debug ("playlist index %d is valid and in range", index);
      const char *stream = list_get (self->playlist, index);
      log_debug ("stream to play is %s", stream);
      self->playlist_index = index;

      ret = cmdproc_play_stream (self, stream, error_code, error);
      if (ret)
        {
        *error_code = 0;
        ret = TRUE;
        }
      else
        {
        // error codes have been set by cmdproc_play_stream
        ret = FALSE;
        }
      }
    else
      {
      log_error ("%s: Playlist index %d is out of range", __PRETTY_FUNCTION__, 
        index);
      *error_code = XINESERVER_ERR_PLAYLIST_INDEX;
      asprintf (error , "Playlist index %d out of range (0-%d)", 
         index, list_length (self->playlist) - 1);
      }
    }
  else
    {
    log_error ("%s: Can't play because playlist is empty", 
        __PRETTY_FUNCTION__);
    *error_code = XINESERVER_ERR_PLAYLIST_EMPTY;
    asprintf (error, "Playlist empty"); 
    }
  pthread_mutex_unlock (&self->playlist_mutex);
  LOG_OUT
  return ret;
  }


/*==========================================================================

  cmdproc_cmd_play

==========================================================================*/
static void cmdproc_cmd_play (CmdProc *self, int argc, List * argv, 
       char **response)
  {
  LOG_IN

  if (argc == 1)
    {
    log_debug ("%s: no arguments", __PRETTY_FUNCTION__);
    xine_interface_resume (self->xi);
    asprintf (response, OK_RESPONSE); 
    }
  else
    {
    log_debug ("%s: one arguments", __PRETTY_FUNCTION__);
    const char *arg = string_cstr (list_get (argv, 1));
    uint64_t v;
    BOOL ok = numberformat_read_integer (arg, &v, TRUE);
    if (ok)
      {
      int index = v;
      int error_code = 0;
      char *error = NULL;
      if (cmdproc_play_playlist_entry (self, index, &error_code,
            &error))
        {
        asprintf (response, OK_RESPONSE); 
        }
      else
        {
        asprintf (response, "%d %s\n", error_code, error); 
        free (error);
        }
      }
    else
      {
      log_error ("%s: Playlist index %s is not a number", 
           __PRETTY_FUNCTION__, arg);
      asprintf (response, "%d Bad number %s\n", XINESERVER_ERR_BADARG, 
	   arg); 
      }
    }
  LOG_OUT
  }

/*==========================================================================

  cmdproc_do_cmd

==========================================================================*/
void cmdproc_do_cmd (CmdProc *self, const char *cmd, char **response)
  {
  LOG_IN
  log_debug ("%s: command=%s", __PRETTY_FUNCTION__, cmd);
  if (strlen (cmd) > 0)
    {
    String *s_cmd = string_create (cmd);
    List *argv = string_tokenize (s_cmd);
    int argc = list_length (argv);
    if (argc > 0)
      {
      const char *cmd = string_cstr (list_get (argv, 0)); 
      log_debug ("%s: Got cmd %s", __PRETTY_FUNCTION__, cmd);
      if (strcmp (cmd, XINESERVER_CMD_SHUTDOWN) == 0)
        {
        log_debug ("%s, Requesting shutdown", __PRETTY_FUNCTION__);
        self->request_quit = TRUE;
        asprintf (response, OK_RESPONSE);
        }
      else if (strcmp (cmd, XINESERVER_CMD_PLAY) == 0)
        {
        log_debug ("%s: Got play command", __PRETTY_FUNCTION__);
        cmdproc_cmd_play (self, argc, argv, response);
        }
      else if (strcmp (cmd, XINESERVER_CMD_ADD) == 0)
        {
        log_debug ("%s: Got add command", __PRETTY_FUNCTION__);
        cmdproc_cmd_add (self, argv, response); 
        }
      else if (strcmp (cmd, XINESERVER_CMD_STOP) == 0)
        {
        log_debug ("%s: Got stop command", __PRETTY_FUNCTION__);
        cmdproc_cmd_stop (self, argv, response); 
        }
      else if (strcmp (cmd, XINESERVER_CMD_STATUS) == 0)
        {
        log_debug ("%s: Got status command", __PRETTY_FUNCTION__);
        cmdproc_cmd_status (self, argv, response); 
        }
      else if (strcmp (cmd, XINESERVER_CMD_PAUSE) == 0)
        {
        log_debug ("%s: Got pause command", __PRETTY_FUNCTION__);
        cmdproc_cmd_pause (self, argv, response); 
        }
      else if (strcmp (cmd, XINESERVER_CMD_PLAYLIST) == 0)
        {
        log_debug ("%s: Got playlist command", __PRETTY_FUNCTION__);
        cmdproc_cmd_playlist (self, argv, response); 
        }
      else if (strcmp (cmd, XINESERVER_CMD_CLEAR) == 0)
        {
        log_debug ("%s: Got clear command", __PRETTY_FUNCTION__);
        cmdproc_cmd_clear (self, argv, response); 
        }
      else if (strcmp (cmd, XINESERVER_CMD_NEXT) == 0)
        {
        log_debug ("%s: Got next command", __PRETTY_FUNCTION__);
        cmdproc_cmd_next (self, argv, response); 
        }
      else if (strcmp (cmd, XINESERVER_CMD_PREV) == 0)
        {
        log_debug ("%s: Got prev command", __PRETTY_FUNCTION__);
        cmdproc_cmd_prev (self, argv, response); 
        }
      else if (strcmp (cmd, XINESERVER_CMD_VOLUME) == 0)
        {
        log_debug ("%s: Got volume command", __PRETTY_FUNCTION__);
        cmdproc_cmd_volume (self, argv, response); 
        }
      else if (strcmp (cmd, XINESERVER_CMD_META_INFO) == 0)
        {
        log_debug ("%s: Got meta-info command", __PRETTY_FUNCTION__);
        cmdproc_cmd_meta_info (self, argv, response); 
        }
      else if (strcmp (cmd, XINESERVER_CMD_SEEK) == 0)
        {
        log_debug ("%s: Got seek command", __PRETTY_FUNCTION__);
        cmdproc_cmd_seek (self, argv, response); 
        }
      else if (strcmp (cmd, XINESERVER_CMD_EQ) == 0)
        {
        log_debug ("%s: Got eq command", __PRETTY_FUNCTION__);
        cmdproc_cmd_eq (self, argv, response); 
        }
      else if (strcmp (cmd, XINESERVER_CMD_VERSION) == 0)
        {
        log_debug ("%s: Got version command", __PRETTY_FUNCTION__);
        asprintf (response, "0 %s\n", VERSION);
	}
      else
        {
        asprintf (response, "%d Unknown command %s\n", 
            XINESERVER_ERR_BADCOMMAND, cmd);
        }
      }
    else
      {
      asprintf (response, "%d Empty command\n", XINESERVER_ERR_SYNTAX);
      }
    list_destroy (argv);
    string_destroy (s_cmd);
    } 
  else
    {
    asprintf (response, "%d Empty command\n", XINESERVER_ERR_SYNTAX);
    }

  LOG_OUT
  }

/*==========================================================================

  cmdproc_has_requested_shutdown

==========================================================================*/
BOOL cmdproc_has_requested_shutdown (const CmdProc *self)
  {
  return self->request_quit;
  }

/*==========================================================================

  cmdproc_notify_progress

==========================================================================*/
static void cmdproc_notify_progress (const char *msg, int percent, 
      void *arg)
  {
  LOG_IN

  char *s = NULL;

  log_debug ("%s: Xine notified progress: %s %d%%", __PRETTY_FUNCTION__,
     msg, percent);

  CmdProc *self = (CmdProc *)arg;
  if (percent > 0)
    asprintf (&s, "%s [%d%%]", msg, percent);
  else
    asprintf (&s, "%s", msg);
  notifier_notify (self->notifier, XSNOTIFY_CLASS_SERVER,
     XSNOTIFY_EVENT_PROGRESS, s);

  free (s);

  LOG_OUT
  }


/*==========================================================================

  cmdproc_notify_playback_finished

==========================================================================*/
static void cmdproc_notify_playback_finished (void *arg)
  {
  LOG_IN

  CmdProc *self = (CmdProc *)arg;
  cmdproc_set_playback_stopped (self);

  log_debug ("%s: Xine notified playback finished", __PRETTY_FUNCTION__);

  pthread_mutex_lock (&self->playlist_mutex);

  int index = self->playlist_index;

  const char *old_stream = list_get (self->playlist, index);
  log_info ("Playback finished for stream '%s'", old_stream);
  notifier_notify (self->notifier, XSNOTIFY_CLASS_TRANSPORT,
     XSNOTIFY_EVENT_STREAM_FINISHED, "%s %s", NOTIFY_MSG_STREAM_FINISHED, 
       old_stream); 

  int length = list_length (self->playlist); 
  index++;
  if (index < length)
    {
    int error_code = 0;
    char *error = NULL;
    log_debug ("%s: Moving to playlist item %d", __PRETTY_FUNCTION__, index);
    const char *stream = list_get (self->playlist, index);
    log_debug ("%s: next item is %s", __PRETTY_FUNCTION__, stream);
    BOOL ok = cmdproc_play_stream (self, stream, &error_code, &error);
    if (ok)
      {
      self->playlist_index = index;
      }
    else
      { 
      log_error ("%s: %s", __PRETTY_FUNCTION__, error); 
      free (error);
      // Try to move onto the next item. This is a recursive call,
      //  and may be troublesome where there is a long playlist
      //  with many broken items, because of stack usage
      self->playlist_index = index; 
      cmdproc_notify_playback_finished (self);
      }
    }
  else
    {
    log_info (NOTIFY_MSG_PL_FINISHED);
    log_debug ("%s: At end of playlist", __PRETTY_FUNCTION__);
    index = -1;
    self->playlist_index = -1;
    notifier_notify (self->notifier, XSNOTIFY_CLASS_TRANSPORT,
       XSNOTIFY_EVENT_PL_FINISHED, NOTIFY_MSG_PL_FINISHED); 
    }

  pthread_mutex_unlock (&self->playlist_mutex);
  LOG_OUT
  }

/*==========================================================================

  cmdproc_init_xine

==========================================================================*/
BOOL cmdproc_init_xine (CmdProc *self)
  {
  return TRUE;
  }


/*==========================================================================

  cmdproc_play_stream

==========================================================================*/
BOOL cmdproc_play_stream (CmdProc *self, 
        const char *stream, int *error_code, char **error)
  {
  LOG_IN
  BOOL ret = FALSE;
  *error_code = 0;
  log_debug ("%s: Play stream: %s", __PRETTY_FUNCTION__, stream);
  
  // If stream begins / it is a local file, and we can check
  //  it before passing it to Xine
  if ((stream[0] == '/' && (access (stream, R_OK) == 0))
        || (stream[0] != '/'))
    {
    if (xine_interface_play_stream (self->xi, stream, error))
      {
      log_info ("%s %s", NOTIFY_MSG_NEW_STREAM, stream);
      ret = TRUE;
      }
    else
      {
      *error_code = XINESERVER_ERR_PLAYBACK;
      ret = FALSE;
      }
    }
  else
    {
    *error_code = XINESERVER_ERR_PLAYBACK;
    asprintf (error, "File not found: %s", stream);
    ret = FALSE;
    }
  if (ret)
    notifier_notify (self->notifier, XSNOTIFY_CLASS_TRANSPORT,
        XSNOTIFY_EVENT_NEW_STREAM, "%s %s", NOTIFY_MSG_NEW_STREAM, stream); 
  LOG_OUT
  return ret;
  }

/*==========================================================================

  cmdproc_stop_playback

==========================================================================*/
void cmdproc_stop_playback (CmdProc *self)
  {
  LOG_IN

  log_debug ("%s: Stopping playback", __PRETTY_FUNCTION__); 

  self->playlist_index = -1;
  xine_interface_stop (self->xi);
  notifier_notify (self->notifier, XSNOTIFY_CLASS_TRANSPORT,
       XSNOTIFY_EVENT_PLAYBACK_STOPPED, NOTIFY_MSG_STOPPED_PLAYBACK); 
  log_info (NOTIFY_MSG_STOPPED_PLAYBACK);

  cmdproc_set_playback_stopped (self);

  LOG_OUT
  }

