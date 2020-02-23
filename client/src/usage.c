/*==========================================================================

  xine-client 
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
  fprintf (fout, "Usage: %s [options] {command} {arguments}\n", argv0);
  fprintf (fout, "     --help               show this message\n");
  fprintf (fout,  
     "  -h,--host=S             server hostname (default localhost)\n");
  fprintf (fout, "  -l,--log-level=N        log level, 0-5 (default 2)\n");
  fprintf (fout, "  -p,--port=N             server port (default %d)\n",
     XINESERVER_DEF_PORT);
  fprintf (fout, "  -v,--version            show version\n");
  fprintf (fout, "\nCommands:\n");
  fprintf (fout, "  add {streams}       add files or streams to playlist\n");
  fprintf (fout, "  clear               stop playback and clear playlist\n");
  fprintf (fout, "  eq [0]..[10]        set equalizer levels, 0..10\n");
  fprintf (fout, "  next                play next in playlist\n");
  fprintf (fout, "  pause               pause playback:\n");
  fprintf (fout, "  play-now {streams}  as 'add', but clear playlist first\n");
  fprintf (fout, "  playlist            print the playlist:\n");
  fprintf (fout, "  play                resume paused playback\n");
  fprintf (fout, "  play N              play playlist item N (first is 0)\n");
  fprintf (fout, "  prev                play previous in playlist\n");
  fprintf (fout, "  seek {sec}          set playback position in seconds\n");
  fprintf (fout, "  shutdown            shutdown the server\n");
  fprintf (fout, "  status              show playback status\n");
  fprintf (fout, "  stop                stop playback\n");
  fprintf (fout, "  volume [V]          set or get volume\n");
  }

 
