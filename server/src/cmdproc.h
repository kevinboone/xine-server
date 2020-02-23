/*============================================================================

  xine-server
  cmdproc.h
  Copyright (c)2020 Kevin Boone, GPL v3.0

============================================================================*/

#pragma once

#include <stdint.h>
#include "defs.h"
#include "notifier.h"
#include "xine_interface.h"

struct _CmdProc;
typedef struct _CmdProc CmdProc;

struct _XineInterface;

BEGIN_DECLS
CmdProc    *cmdproc_create (struct _XineInterface *xi, Notifier *notifier); 
void        cmdproc_destroy (CmdProc *self);
void        cmdproc_do_cmd (CmdProc *self, const char *cmd, char **response);
BOOL        cmdproc_has_requested_shutdown (const CmdProc *self);
void        cmdproc_stop_playback (CmdProc *self);
BOOL        cmdproc_play_stream (CmdProc *self, 
                const char *stream, int *error_code, char **error);
END_DECLS



