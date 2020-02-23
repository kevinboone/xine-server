/*============================================================================

  xine-server
  xine_interface.h
  Copyright (c)2020 Kevin Boone, GPL v3.0

============================================================================*/

#pragma once

#include <stdint.h>
#include "defs.h"
#include "cmdproc.h"
#include "../../api/xine-server-api.h"

struct _XineInterface;
typedef struct _XineInterface XineInterface;

typedef void (*XineInterfacePlaybackFinishedFn)(void *data);
typedef void (*XineInterfaceProgressFn)(const char *message, 
         int percent, void *data);

BEGIN_DECLS

XineInterface *xine_interface_create (const char *driver,
                   const char *config_file);
BOOL           xine_interface_init (XineInterface *self,  char **error);
void           xine_interface_destroy (XineInterface *self);
void           xine_interface_set_playback_finished_fn (XineInterface *self,
                   XineInterfacePlaybackFinishedFn fn, void *data);
void           xine_interface_set_progress_fn (XineInterface *self,
                   XineInterfaceProgressFn fn, void *data);
BOOL           xine_interface_play_stream (XineInterface *self, 
                   const char *stream, char **error);
void           xine_interface_stop (XineInterface *self);
XSTransportStatus  xine_interface_get_transport_status 
                   (XineInterface *self);
void           xine_interface_resume (XineInterface *self);
void           xine_interface_pause (XineInterface *self);
void           xine_interface_set_volume (XineInterface *self, int volume);
int            xine_interface_get_volume (const XineInterface *self);
const char    *xine_interface_get_meta_info
                   (const XineInterface *self, int key);
uint32_t       xine_interface_get_stream_info
                   (const XineInterface *self, int key);
uint32_t       xine_interface_is_seekable
                   (const XineInterface *self, int key);
BOOL           xine_interface_seek (XineInterface *self, int msec);
const char *const *xine_interface_list_audio_drivers 
                   (const XineInterface *self);
void           xine_interface_get_eq (const XineInterface *self, int eq[10]);
void           xine_interface_set_eq (const XineInterface *self, int eq[10]);

// Position and length are in msec. If the stream is inbounded,
// e.g., a radio stream, length is reported as zero. If nothing
// is playing, both length and position are zero.
void           xine_interface_get_pos_len (const XineInterface *self, 
                  int *pos, int *len);

END_DECLS



