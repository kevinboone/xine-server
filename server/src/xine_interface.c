/*==========================================================================

  xine-server 
  xine_interface.c
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
#include "../../api/xine-server-api.h" 
#include "xine_interface.h" 

struct _XineInterface
  {
  // Xine stuff
  xine_t *xine;
  xine_stream_t *stream;
  xine_audio_port_t *ao_port;
  xine_video_port_t *vo_port;
  xine_event_queue_t *event_queue;
  // inited == true when Xine has been initialized
  BOOL inited;
  XineInterfacePlaybackFinishedFn playbackFinishedFn;
  void *playbackFinishedData;
  XineInterfaceProgressFn progressFn;
  void *progressData;
  // playback_started is true between successful calls to
  //  play_stream() and calls to stop(). It does not indicated
  //  that sound is being generated -- playback might be paused,
  //  for example 
  BOOL playback_started;
  char *config_file; // May be NULL, if none needed
  char *driver; // May be NULL, to use default 
  BOOL buffering; // Set when receive a buffering event
  }; 


/*==========================================================================

  xine_interface_create

==========================================================================*/
XineInterface *xine_interface_create (const char *driver, 
        const char *config_file)
  {
  LOG_IN
  log_debug ("%s: Creating Xine interface", __PRETTY_FUNCTION__); 
  XineInterface *self = malloc (sizeof (XineInterface));
  self->xine = NULL;
  self->stream = NULL;
  self->ao_port = NULL;
  self->vo_port = NULL;
  self->event_queue = NULL;
  self->inited = FALSE;
  self->playbackFinishedFn = NULL;
  self->playbackFinishedData = NULL;
  self->progressFn = NULL;
  self->progressData = NULL;
  self->playback_started = FALSE;
  if (config_file)
    self->config_file = strdup (config_file);
  else
    self->config_file = NULL; 
  if (driver)
    self->driver = strdup (driver);
  else
    self->driver = NULL; 
  self->buffering = FALSE;
  LOG_OUT
  return self;
  }


/*==========================================================================

  xine_interface_destroy

==========================================================================*/
void xine_interface_destroy (XineInterface *self)
  {
  LOG_IN
  log_debug ("%s: Destroying Xine interface", __PRETTY_FUNCTION__); 
  if (self)
    {
    if (self->stream)
      {
      log_debug ("%s: Closing Xine stream", __PRETTY_FUNCTION__); 
      xine_close (self->stream);
      }
    if (self->event_queue)
      {
      log_debug ("%s: Disposing event queue", __PRETTY_FUNCTION__); 
      xine_event_dispose_queue (self->event_queue);
      }
    if (self->stream)
      {
      log_debug ("%s: Disposing Xine stream", __PRETTY_FUNCTION__); 
      xine_dispose (self->stream);
      }
    if (self->ao_port) 
      {
      log_debug ("%s: Closing audio driver", __PRETTY_FUNCTION__); 
      xine_close_audio_driver (self->xine, self->ao_port);
      }
    self->ao_port = NULL;
    if (self->vo_port) 
      {
      log_debug ("%s: Closing video driver", __PRETTY_FUNCTION__); 
      xine_close_video_driver (self->xine, self->vo_port);
      }
    if (self->xine)
      {
      log_debug ("%s: Closing Xine", __PRETTY_FUNCTION__); 
      xine_exit (self->xine); 
      }
    if (self->config_file)
      free (self->config_file);
    if (self->driver)
      free (self->driver);
    free (self);
    }
  LOG_OUT
  }


/*==========================================================================

  xine_interface_set_playback_finished_fn

==========================================================================*/
void xine_interface_set_playback_finished_fn (XineInterface *self,
                 XineInterfacePlaybackFinishedFn fn, void *data)
  {
  LOG_IN
  self->playbackFinishedFn = fn;
  self->playbackFinishedData = data;
  LOG_OUT
  }


/*==========================================================================

  xine_interface_set_progress_fn

==========================================================================*/
void xine_interface_set_progress_fn (XineInterface *self,
                 XineInterfaceProgressFn fn, void *data)
  {
  LOG_IN
  self->progressFn = fn;
  self->progressData = data;
  LOG_OUT
  }


/*==========================================================================

  xine_interface_event_listener

==========================================================================*/
static void xine_interface_event_listener (void *user_data, 
    const xine_event_t *event) 
  {
  LOG_IN
  XineInterface *self = (XineInterface *) user_data;
  log_debug ("%s: Received event %d", __PRETTY_FUNCTION__, event->type);
  switch (event->type) 
    {
    case XINE_EVENT_UI_PLAYBACK_FINISHED:
      log_debug ("%s: Received playback finished event", __PRETTY_FUNCTION__);
      self->playback_started = FALSE;
      if (self->playbackFinishedFn)
        self->playbackFinishedFn (self->playbackFinishedData);
      self->buffering = FALSE; 
      break;

    case XINE_EVENT_PROGRESS:
      {
      log_debug ("%s: Received progress event", __PRETTY_FUNCTION__);
      xine_progress_data_t *pt = event->data;
      if (self->progressFn)
        self->progressFn (pt->description, pt->percent, self->progressData);
      }
      break;

    case XINE_EVENT_NBC_STATS:
      {
      log_debug ("%s: Received NBC STATS event", __PRETTY_FUNCTION__);
      xine_nbc_stats_data_t *b = event->data;
      self->buffering = b->buffering;
      }
      break;
    }

  LOG_OUT
  }


/*==========================================================================

  xine_interface_init

==========================================================================*/
BOOL xine_interface_init (XineInterface *self, 
         char **error) 
  {
  LOG_IN
  BOOL ret = TRUE; 
  // Note -- driver arg may be NULL

  if (!self->inited)
    {
    log_debug ("%s: Initializing Xine interface", __PRETTY_FUNCTION__);
    self->xine = xine_new();

    if (self->config_file)
      {
      log_debug ("%s: loading config file %s", __PRETTY_FUNCTION__, 
        self->config_file);
      xine_config_load (self->xine, self->config_file);
      }
    xine_init (self->xine);

    log_debug ("%s: Opening audio driver", __PRETTY_FUNCTION__);
    self->ao_port = xine_open_audio_driver 
	(self->xine, self->driver, NULL);
    if (self->ao_port) 
      log_debug ("%s: Opened audio port", __PRETTY_FUNCTION__);
    else
      {
      log_error ("%s: Can't open audio port", __PRETTY_FUNCTION__);
      asprintf (error, "Can't open audio port");
      }

    log_debug ("%s: Opening video driver", __PRETTY_FUNCTION__);
    self->vo_port = xine_open_video_driver 
	(self->xine, NULL, XINE_VISUAL_TYPE_NONE, NULL);

    log_debug ("%s: Creating Xine stream", __PRETTY_FUNCTION__);
    self->stream = xine_stream_new (self->xine, self->ao_port, 
      self->vo_port);

    self->event_queue = xine_event_new_queue (self->stream);
    xine_event_create_listener_thread (self->event_queue, 
	xine_interface_event_listener, self);

    self->inited = TRUE;
    }
  else
    {
    log_debug ("%s: Xine interface already initialized",
          __PRETTY_FUNCTION__);
    }

  LOG_OUT
  return ret;
  }


/*==========================================================================

  xine_interface_get_metadata

  This is a thin wrapper on xine_get_meta_info. It takes care
  of returning a "-" when nothing is playing, or if the data 
  returned by Xine is null.

==========================================================================*/
const char *xine_interface_get_meta_info
                 (const XineInterface *self, int key)
  {
  LOG_IN
  const char *ret = "-";
  if (self->stream)
    {
    if (self->playback_started)
      {
      const char *value = xine_get_meta_info (self->stream, key); 
      if (value) 
        {
        if (strlen (value) > 0)
          ret = value;
        else
          value = "-";
        }
      else
        ret = "-";
      }
    }
  return ret; 
  LOG_OUT
  }


/*==========================================================================

  xine_interface_seek

==========================================================================*/
BOOL xine_interface_seek (XineInterface *self, int msec)
  {
  LOG_IN
  BOOL ret = TRUE;
  if (self->stream)
    {
    xine_play (self->stream, 0, msec);
    }
  return ret; 
  LOG_OUT
  }


/*==========================================================================

  xine_interface_get_stream_info

  This is a thin wrapper on xine_get_stream_info. It takes care
  of returning a 0 when nothing is playing

==========================================================================*/
uint32_t  xine_interface_get_stream_info
                 (const XineInterface *self, int key)
  {
  LOG_IN
  uint32_t ret = 0;
  if (self->stream)
    {
    if (self->playback_started)
      {
      ret = xine_get_stream_info (self->stream, key); 
      }
    }
  return ret; 
  LOG_OUT
  }


/*==========================================================================

  xine_interface_is_seekable
  
  If nothing is playing, the value is FALSE

==========================================================================*/
uint32_t  xine_interface_is_seekable
                 (const XineInterface *self, int key)
  {
  LOG_IN
  BOOL ret = FALSE;
  if (self->stream)
    {
    if (self->playback_started)
      {
      ret = xine_get_stream_info (self->stream, XINE_STREAM_INFO_SEEKABLE); 
      }
    }
  return ret; 
  LOG_OUT
  }


/*==========================================================================

  xine_interface_get_transport_status

==========================================================================*/
XSTransportStatus  xine_interface_get_transport_status (XineInterface *self)
  {
  LOG_IN
  BOOL ret = FALSE;
  if (self->stream)
    {
    if (self->playback_started)
      {
      if (self->buffering)
        ret = XINESERVER_TRANSPORT_BUFFERING;
      else
        {
        int speed = xine_get_param (self->stream, XINE_PARAM_SPEED);
        log_debug ("XINE_PARAM_SPEED is %d", speed);
        if (speed == 0)
          ret = XINESERVER_TRANSPORT_PAUSED; 
        else 
          ret = XINESERVER_TRANSPORT_PLAYING; 
        }
      }
    else
      ret = XINESERVER_TRANSPORT_STOPPED;
    }
  else
    ret = XINESERVER_TRANSPORT_STOPPED;
  return ret; 
  LOG_OUT
  }


/*==========================================================================

  xine_interface_pause

==========================================================================*/
void xine_interface_pause (XineInterface *self)
  {
  LOG_IN
  if (self->stream)
    {
    xine_set_param (self->stream, XINE_PARAM_SPEED, 0);
    }

  LOG_OUT
  }


/*==========================================================================

  xine_interface_resume

==========================================================================*/
void xine_interface_resume (XineInterface *self)
  {
  LOG_IN
  if (self->stream)
    {
    xine_set_param (self->stream, XINE_PARAM_SPEED, XINE_SPEED_NORMAL);
    }

  LOG_OUT
  }


/*==========================================================================

  xine_interface_get_pos_len

==========================================================================*/
void xine_interface_get_pos_len (const XineInterface *self, 
    int *pos, int *len)
  {
  *pos = 0;
  *len = 0;
  if (self->stream)
    {
    int dummy;
    xine_get_pos_length (self->stream, &dummy, pos, len);
    }
  }



/*==========================================================================

  xine_interface_stop

==========================================================================*/
void xine_interface_stop (XineInterface *self)
  {
  LOG_IN
  if (self->stream)
    xine_stop (self->stream);
  self->playback_started = FALSE;
  self->buffering = FALSE;
  LOG_OUT
  }


/*==========================================================================

  xine_interface_get_volume

==========================================================================*/
int xine_interface_get_volume (const XineInterface *self)
  {
  LOG_IN
  int ret;
  if (self->stream)
    ret = xine_get_param (self->stream, XINE_PARAM_AUDIO_VOLUME);
  else 
    ret = -1;
  return ret;
  LOG_OUT
  }


/*==========================================================================

  xine_interface_get_eq

==========================================================================*/
void xine_interface_get_eq (const XineInterface *self, int eq[10])
  {
  LOG_IN
  if (self->stream)
    {
    for (int i = 0; i < 10; i++) 
      eq[i] = xine_get_param (self->stream, XINE_PARAM_EQ_30HZ + i);
    }
  else
    {
    for (int i = 0; i < 10; i++) eq[i] = 0;
    }
  LOG_OUT
  }

/*==========================================================================

  xine_interface_set_eq

==========================================================================*/
void xine_interface_set_eq (const XineInterface *self, int eq[10])
  {
  LOG_IN
  if (self->stream)
    {
    for (int i = 0; i < 10; i++) 
      {
      xine_set_param (self->stream, XINE_PARAM_EQ_30HZ + i, eq[i]);
      }
    }
  LOG_OUT
  }

/*==========================================================================

  xine_interface_set_volume

==========================================================================*/
void xine_interface_set_volume (XineInterface *self, int volume)
  {
  if (self->stream)
    {
    xine_set_param (self->stream, XINE_PARAM_AUDIO_VOLUME, volume);
    }
  }

/*==========================================================================

  xine_interface_play_stream

==========================================================================*/
BOOL xine_interface_play_stream (XineInterface *self,  
       const char *stream, char **error)
  {
  LOG_IN
  int ret = TRUE;

  if (self->stream)
    {
    xine_close (self->stream);
    }

  if (xine_open (self->stream, stream))
      log_debug ("%s: Xine stream opened for %s", __PRETTY_FUNCTION__,
         stream);
  else
    {
    log_error ("%s: xine_open() failed for %s", __PRETTY_FUNCTION__, 
       stream);
    asprintf (error, "xine_open() failed for %s", stream);
    ret = FALSE;
    }

  if (xine_play (self->stream, 0, 0))
    log_debug ("%s: Playing: %s", __PRETTY_FUNCTION__, stream);
  else
    {
    log_error ("%s: xine_play() failed for %s", __PRETTY_FUNCTION__,
            stream);
    asprintf (error, "xine_play() failed for %s", stream);
    ret = FALSE;
    }
  if (ret) 
    {
    xine_interface_resume (self); // Cancel a previous pause
    log_debug ("Setting status to show playback started");
    self->playback_started = TRUE;
    } 

  LOG_OUT
  return ret;
  }


/*==========================================================================

  xine_interface_list_audio_drivers

==========================================================================*/
const char *const *xine_interface_list_audio_drivers 
                 (const XineInterface *self)
  {
  return xine_list_audio_output_plugins (self->xine);
  }



