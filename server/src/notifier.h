/*============================================================================

  xine-server
  notifier.h
  Copyright (c)2020 Kevin Boone, GPL v3.0

============================================================================*/

#pragma once

#include <stdint.h>
#include "defs.h"
#include "../../api/xine-server-api.h" 

struct _Notifier;
typedef struct _Notifier Notifier;

#define NOTIFY_MSG_SERVER_STARTUP   "Server startup"
#define NOTIFY_MSG_SERVER_SHUTDOWN  "Server shutdown"
#define NOTIFY_MSG_STOPPED_PLAYBACK "Stopped playback"
#define NOTIFY_MSG_PAUSED_PLAYBACK   "Paused playback"
#define NOTIFY_MSG_CHANGED_PL_POSITION "Changed position in playlist"
#define NOTIFY_MSG_PL_CHANGED        "Playlist contents changed"
#define NOTIFY_MSG_VOLUME_CHANGED    "Volume changed"
#define NOTIFY_MSG_SEEK              "Playback position changed"
#define NOTIFY_MSG_NEW_STREAM        "Playing new stream"
#define NOTIFY_MSG_STREAM_FINISHED   "Finished stream"
#define NOTIFY_MSG_PL_FINISHED       "Finished playlist"

BEGIN_DECLS
Notifier     *notifier_create (void);
void          notifier_destroy (Notifier *self);
void          notifier_notify (Notifier *self, XSNotifyClass cls, 
                 XSNotifyEvent event, const char *fmt,...);
END_DECLS



