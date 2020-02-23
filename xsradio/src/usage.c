/*==========================================================================

  xsradio 
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
#include "../../api/xine-server-api.h" 


/*==========================================================================
  usage_show
==========================================================================*/
void usage_show (FILE *fout, const char *argv0)
  {
  fprintf (fout, "Usage: %s [options]\n", argv0);
  fprintf (fout, "     --help               show this message\n");
  fprintf (fout,  
     "  -h,--host=S             server hostname (default localhost)\n");
  fprintf (fout, "  -l,--log-level=N        log level, 0-5 (default 2)\n");
  fprintf (fout, "  -p,--port=N             server port (default %d)\n",
     XINESERVER_DEF_PORT);
  fprintf (fout, "  -s,--streams=S          stream list file\n");
  }

 
