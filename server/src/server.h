/*============================================================================

  xine-server
  server.h
  Copyright (c)2020 Kevin Boone, GPL v3.0

============================================================================*/

#pragma once

#include <stdint.h>
#include "defs.h"
#include "cmdproc.h"

struct _Server;
typedef struct _Server Server;

BEGIN_DECLS
Server    *server_create (const char *host, int port, CmdProc *cmdproc);
void       server_destroy (Server *self);
BOOL       server_init (Server *self, char **error);
BOOL       server_start (Server *self, char **error);
BOOL       server_is_running (const Server *self);
END_DECLS


