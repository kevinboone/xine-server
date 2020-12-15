/*==========================================================================

  xine-server 
  usage.c
  Copyright (c)2020 Kevin Boone
  Distributed under the terms of the GPL v3.0

==========================================================================*/

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "feature.h" 
#include "usage.h" 


/*==========================================================================
  usage_show
==========================================================================*/
void usage_show (FILE *fout, const char *argv0)
  {
  fprintf (fout, "Usage: %s [options]\n", argv0);
  fprintf (fout, "  -?,--help               show this message\n");
  fprintf (fout, "  -c,--config=file        configuration file (none)\n");
  fprintf (fout, "     --debug              debug mode\n");
  fprintf (fout, "  -h,--host=IP            host IP to bind to (127.0.0.1)\n");
  fprintf (fout, "  -l,--log-level=N        log level, 0-5 (default 2)\n");
  fprintf (fout, "  --list-drivers          list audio drivers\n");
  fprintf (fout, "  -v,--version            show version\n");
  fprintf (fout, "  -p,--port=N             listen port (default 30001)\n");
  fprintf (fout, "  -d,-=driver=D           audio driver (default auto)\n");
  }

 
