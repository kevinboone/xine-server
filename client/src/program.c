/*==========================================================================

  xine-client 
  program.c
  Copyright (c)2020 Kevin Boone
  Distributed under the terms of the GPL v3.0

  This file contains the main body of the program. By the time
  program_run() has been called, RC files will have been read and comand-
  line arguments parsed, so all the contextual information will be in the
  ProgramContext. Logging will have been initialized, so the log_xxx
  methods will work, and be filtered at the appopriate levels.
  The unparsed command-line arguments will be available
  in the context as nonswitch_argc and nonswitch_argv.

==========================================================================*/
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <wchar.h>
#include <time.h>

#include "program_context.h" 
#include "feature.h" 
#include "program.h" 
#include "console.h" 
#include "usage.h" 
#include "numberformat.h" 
#include "../../api/xine-server-api.h" 

/*==========================================================================

  program_cmd_playlist

==========================================================================*/
static int program_cmd_playlist (const ProgramContext *context, 
  const char *host, int port, int argc, char **argv)
  {
  LOG_IN
  int ret = 0;
 
  if (argc == 1)
    {
    int error_code = 0;
    char *error = NULL; 
    XSPlaylist *playlist;
    if (xineserver_playlist (host, port, &playlist, 
            &error_code, &error))
      {
      ret = 0;
      int n = xsplaylist_get_nentries (playlist);
      char ** const entries = xsplaylist_get_entries (playlist);
      for (int i = 0; i < n; i++)
        {
        printf ("%d %s\n", i, entries[i]);
        }
      xsplaylist_destroy (playlist);
      }
    else
      {
      fprintf (stderr, NAME " playlist: error %d: %s\n", error_code, error);
      ret = -1;
      free (error);
      }
    }
  else
    {
    fprintf (stderr, "Usage " NAME " playlist\n");
    ret = -1;
    }

  LOG_OUT
  return ret;
  }

/*==========================================================================

  program_cmd_play

==========================================================================*/
static int program_cmd_play (const ProgramContext *context, 
      const char *host, int port, int argc, char **argv)
  {
  LOG_IN
  int ret = 0;
 
  if (argc == 1)
    {
    // resume
    int error_code = 0;
    char *error = NULL;
    if (!xineserver_resume (host, port, &error_code, &error))
      {
      fprintf (stderr, NAME " play: error %d: %s\n", error_code, error);
      ret = -1;
      free (error);
      }
    }
  else if (argc == 2)
    {
    const char *arg = argv[1];
    uint64_t v;
    if (numberformat_read_integer (arg, &v, TRUE))
      {
      int error_code = 0;
      char *error = NULL;
      if (!xineserver_play (host, port, v, &error_code, &error))
        {
        fprintf (stderr, NAME " play: error %d: %s\n", error_code, error);
        ret = -1;
        free (error);
        }
      }
    else
      {
      fprintf (stderr, NAME " play: error: bad number '%s' \n",
        arg);
      ret = -1;
      }
    }
  else
    {
    fprintf (stderr, "Usage: " NAME " play-now {streams}\n");
    ret = -1;
    }

  LOG_OUT
  return ret;
  }

/*==========================================================================

  program_cmd_play_now

==========================================================================*/
static int program_cmd_play_now (const ProgramContext *context, 
      const char *host, int port, int argc, char **argv)
  {
  LOG_IN
  int ret = 0;
 
  if (argc >= 2)
    {
    char *error = NULL;
    int error_code = 0;
    if (!xineserver_clear (host, port, &error_code, &error))
      {
      fprintf (stderr, NAME " play-now: error %d: %s\n", error_code, error);
      ret = -1;
      free (error);
      }

    if (ret == 0)
      {
      int nstreams = argc - 1;
      char **streams = malloc (nstreams * sizeof (char *));
      for (int i = 1; i < argc; i++)
	{
        char *stream = strdup (argv[i]);
        streams[i-1] = stream;
        }

      if (!xineserver_add (host, port, nstreams, 
              (const char *const *)streams, &error_code, &error))
        {
        fprintf (stderr, NAME " play-now: error %d: %s\n", error_code, error);
        ret = -1;
        free (error);
        }

      for (int i = 0; i < nstreams; i++)
	{
        free (streams[i]);
        }
      free (streams);
      }
    if (ret == 0)
      {
      if (!xineserver_play (host, port, 0, &error_code, &error))
        {
        fprintf (stderr, NAME " play-now: error %d: %s\n", error_code, error);
        ret = -1;
        free (error);
        }
      }
    }
  else
    {
    fprintf (stderr, "Usage: " NAME " play-now {streams}\n");
    ret = -1;
    }

  LOG_OUT
  return ret;
  }

/*==========================================================================

  program_cmd_add

==========================================================================*/
static int program_cmd_add (const ProgramContext *context, const char *host,
      int port, int argc, char **argv)
  {
  LOG_IN
  int ret = 0;
 
  if (argc >= 2)
    {
    int nstreams = argc - 1;
    char **streams = malloc (nstreams * sizeof (char *));
    for (int i = 1; i < argc; i++)
      {
      char *stream = strdup (argv[i]);
      streams[i-1] = stream;
      }

    int error_code = 0;
    char *error = NULL;
    if (!xineserver_add (host, port, nstreams, 
              (const char *const *)streams, &error_code, &error))
      {
      fprintf (stderr, NAME " add: error %d: %s\n", error_code, error);
      ret = -1;
      free (error);
      }

    for (int i = 0; i < nstreams; i++)
      {
      free (streams[i]);
      }
    free (streams);
    }
  else
    {
    fprintf (stderr, "Usage " NAME " add {streams}\n");
    ret = -1;
    }

  LOG_OUT
  return ret;
  }

/*==========================================================================

  program_cmd_seek

==========================================================================*/
static int program_cmd_seek (const ProgramContext *context, const char *host,
      int port, int argc, char **argv)
  {
  LOG_IN
  int ret = 0;
 
  if (argc == 2)
    {
    int error_code = 0;
    char *error = NULL;
    const char *s_seek = argv[1];
    if (!xineserver_seek (host, port, atoi (s_seek) * 1000, 
           &error_code, &error))
      {
      fprintf (stderr, NAME " seek: error %d: %s\n", error_code, error);
      ret = -1;
      free (error);
      }
    }
  else
    {
    fprintf (stderr, "Usage " NAME " seek {msec}\n");
    ret = -1;
    }

  LOG_OUT
  return ret;
  }

/*==========================================================================

  program_cmd_eq

==========================================================================*/
static int program_cmd_eq (const ProgramContext *context, const char *host,
      int port, int argc, char **argv)
  {
  LOG_IN
  int ret = 0;
 
  if (argc == 11)
    {
    int error_code = 0;
    char *error = NULL;
    int eq[10];
    for (int i = 0; i < 10; i++)
      eq[i] = atoi (argv[i + 1]) - 100;

    if (xineserver_set_eq (host, port, eq, &error_code, &error))
      { 
      }
    else
      {
      fprintf (stderr, NAME " eq: error %d: %s\n", error_code, error);
      ret = -1;
      free (error);
      }
    }
  else if (argc == 1)
    {
    int error_code = 0;
    char *error = NULL;
    int eq[10];
    if (xineserver_get_eq (host, port, eq, &error_code, &error))
      { 
      for (int i = 0; i < 10; i++)
        printf ("%d ", eq[i] + 100);
      printf ("\n");
      }
    else
      { 
      fprintf (stderr, NAME " eq: error %d: %s\n", error_code, error);
      ret = -1;
      free (error);
      }
    }
  else
    {
    fprintf (stderr, "Usage: " NAME " takes ten arguments, or none\n");
    }

  LOG_OUT
  return ret;
  }

/*==========================================================================

  program_cmd_volume

==========================================================================*/
static int program_cmd_volume (const ProgramContext *context, const char *host,
      int port, int argc, char **argv)
  {
  LOG_IN
  int ret = 0;
 
  if (argc == 2)
    {
    int error_code = 0;
    char *error = NULL;
    const char *s_vol = argv[1];
    if (!xineserver_set_volume (host, port, atoi (s_vol), &error_code, &error))
      {
      fprintf (stderr, NAME " volume: error %d: %s\n", error_code, error);
      ret = -1;
      free (error);
      }
    }
  else
    {
    int error_code = 0;
    char *error = NULL;
    int vol = 0;
    if (xineserver_get_volume (host, port, &vol, &error_code, &error))
      { 
      printf ("%d\n", vol);
      }
    else
      {
      fprintf (stderr, NAME " volume: error %d: %s\n", error_code, error);
      ret = -1;
      free (error);
      }
    }

  LOG_OUT
  return ret;
  }

/*==========================================================================

  program_cmd_stop

==========================================================================*/
static int program_cmd_stop (const ProgramContext *context, const char *host,
      int port, int argc, char **argv)
  {
  LOG_IN
  int ret = 0;
 
  char *error = NULL;
  int error_code = 0;
  if (!xineserver_stop (host, port, &error_code, &error))
    {
    fprintf (stderr, NAME " stop: error %d: %s\n", error_code, error);
    ret = -1;
    free (error);
    }

  LOG_OUT
  return ret;
  }

/*==========================================================================

  program_cmd_shutdown

==========================================================================*/
static int program_cmd_shutdown (const ProgramContext *context, 
      const char *host, int port, int argc, char **argv)
  {
  LOG_IN
  int ret = 0;
 
  char *error = NULL;
  int error_code = 0;
  if (!xineserver_shutdown (host, port, &error_code, &error))
    {
    fprintf (stderr, NAME " shutdown: error %d: %s\n", error_code, error);
    ret = -1;
    free (error);
    }

  LOG_OUT
  return ret;
  }

/*==========================================================================

  program_cmd_pause

==========================================================================*/
static int program_cmd_pause (const ProgramContext *context, const char *host,
      int port, int argc, char **argv)
  {
  LOG_IN
  int ret = 0;
 
  char *error = NULL;
  int error_code = 0;
  if (!xineserver_pause (host, port, &error_code, &error))
    {
    fprintf (stderr, NAME " pause: error %d: %s\n", error_code, error);
    ret = -1;
    free (error);
    }

  LOG_OUT
  return ret;
  }

/*==========================================================================

  program_cmd_next

==========================================================================*/
static int program_cmd_next (const ProgramContext *context, const char *host,
      int port, int argc, char **argv)
  {
  LOG_IN
  int ret = 0;
 
  char *error = NULL;
  int error_code = 0;
  if (!xineserver_next (host, port, &error_code, &error))
    {
    fprintf (stderr, NAME " next: error %d: %s\n", error_code, error);
    ret = -1;
    free (error);
    }

  LOG_OUT
  return ret;
  }

/*==========================================================================

  program_cmd_prev

==========================================================================*/
static int program_cmd_prev (const ProgramContext *context, const char *host,
      int port, int argc, char **argv)
  {
  LOG_IN
  int ret = 0;
 
  char *error = NULL;
  int error_code = 0;
  if (!xineserver_prev (host, port, &error_code, &error))
    {
    fprintf (stderr, NAME " prev: error %d: %s\n", error_code, error);
    ret = -1;
    free (error);
    }

  LOG_OUT
  return ret;
  }

/*==========================================================================

  program_cmd_clear

==========================================================================*/
static int program_cmd_clear (const ProgramContext *context, const char *host,
      int port, int argc, char **argv)
  {
  LOG_IN
  int ret = 0;
 
  char *error = NULL;
  int error_code = 0;
  if (!xineserver_clear (host, port, &error_code, &error))
    {
    fprintf (stderr, NAME " clear: error %d: %s\n", error_code, error);
    ret = -1;
    free (error);
    }

  LOG_OUT
  return ret;
  }

/*==========================================================================

  program_cmd_status

==========================================================================*/
static int program_cmd_status (const ProgramContext *context, const char *host,
      int port, int argc, char **argv)
  {
  LOG_IN
  int ret = 0;
 
  char *error = NULL;
  int error_code = 0;
  XSStatus *status;
  if (xineserver_status (host, port, &status, &error_code, &error))
    {
    printf ("stream: %s\n", xsstatus_get_stream (status));
    char buff[20];
    xsstatus_get_position_hms (status, buff, sizeof (buff));
    printf ("position: %s\n", buff); 
    xsstatus_get_length_hms (status, buff, sizeof (buff));
    printf ("length: %s\n", buff); 
    printf ("playlist index: %d\n", xsstatus_get_playlist_index (status));
    printf ("playlist length: %d\n", xsstatus_get_playlist_length (status));
    const char *ts = "";
    switch (xsstatus_get_transport_status (status))
      {
      case XINESERVER_TRANSPORT_STOPPED: ts = "stopped"; break;
      case XINESERVER_TRANSPORT_PLAYING: ts = "playing"; break;
      case XINESERVER_TRANSPORT_PAUSED: ts = "paused"; break;
      case XINESERVER_TRANSPORT_BUFFERING: ts = "buffering"; break;
      }
    printf ("transport: %s\n", ts); 
    xsstatus_destroy (status); 
    }
  else
    {
    fprintf (stderr, NAME " status: error %d: %s\n", error_code, error);
    ret = -1;
    free (error);
    }

  LOG_OUT
  return ret;
  }

/*==========================================================================

  program_cmd_meta_info

==========================================================================*/
static int program_cmd_meta_info (const ProgramContext *context, 
      const char *host, int port, int argc, char **argv)
  {
  LOG_IN
  int ret = 0;
 
  char *error = NULL;
  int error_code = 0;
  XSMetaInfo *mi = NULL;
  if (xineserver_meta_info (host, port, &mi, &error_code, &error))
    {
    printf ("bitrate: %d kbps\n", xsmetainfo_get_bitrate (mi) / 1000); 
    printf ("seekable: %d\n", xsmetainfo_is_seekable (mi)); 
    printf ("title: %s\n", xsmetainfo_get_title (mi)); 
    printf ("artist: %s\n", xsmetainfo_get_artist (mi)); 
    printf ("genre: %s\n", xsmetainfo_get_genre (mi)); 
    printf ("album: %s\n", xsmetainfo_get_album (mi)); 
    printf ("composer: %s\n", xsmetainfo_get_composer (mi)); 
    xsmetainfo_destroy (mi); 
    }
  else
    {
    fprintf (stderr, NAME " status: error %d: %s\n", error_code, error);
    ret = -1;
    free (error);
    }

  LOG_OUT
  return ret;
  }


/*==========================================================================

  program_do_cmd

==========================================================================*/
static int program_do_cmd (const ProgramContext *context, const char *host, 
      int port, int argc, char **argv)
  {
  LOG_IN
  int ret = 0;

  const char *cmd = argv[0];
  if (strcmp (cmd, "add") == 0)
    ret = program_cmd_add (context, host, port, argc, argv);
  else if (strcmp (cmd, "playlist") == 0)
    ret = program_cmd_playlist (context, host, port, argc, argv);
  else if (strcmp (cmd, "stop") == 0)
    ret = program_cmd_stop (context, host, port, argc, argv);
  else if (strcmp (cmd, "shutdown") == 0)
    ret = program_cmd_shutdown (context, host, port, argc, argv);
  else if (strcmp (cmd, "pause") == 0)
    ret = program_cmd_pause (context, host, port, argc, argv);
  else if (strcmp (cmd, "clear") == 0)
    ret = program_cmd_clear (context, host, port, argc, argv);
  else if (strcmp (cmd, "play-now") == 0)
    ret = program_cmd_play_now (context, host, port, argc, argv);
  else if (strcmp (cmd, "play") == 0)
    ret = program_cmd_play(context, host, port, argc, argv);
  else if (strcmp (cmd, "status") == 0)
    ret = program_cmd_status (context, host, port, argc, argv);
  else if (strcmp (cmd, "next") == 0)
    ret = program_cmd_next (context, host, port, argc, argv);
  else if (strcmp (cmd, "prev") == 0)
    ret = program_cmd_prev (context, host, port, argc, argv);
  else if (strcmp (cmd, "volume") == 0)
    ret = program_cmd_volume (context, host, port, argc, argv);
  else if (strcmp (cmd, "meta-info") == 0)
    ret = program_cmd_meta_info (context, host, port, argc, argv);
  else if (strcmp (cmd, "seek") == 0)
    ret = program_cmd_seek (context, host, port, argc, argv);
  else if (strcmp (cmd, "eq") == 0)
    ret = program_cmd_eq (context, host, port, argc, argv);
  else
    {
    log_error ("Unknown command '%s'", cmd);
    ret = -1;
    }

  LOG_OUT
  return ret;
  }


/*==========================================================================
  program_run

  The return value will eventually become the exit value from the program.
==========================================================================*/
int program_run (ProgramContext *context)
  {
  LOG_IN
  char ** const argv = program_context_get_nonswitch_argv (context);
  int argc = program_context_get_nonswitch_argc (context);
  int port = program_context_get_integer (context, "port",
      XINESERVER_DEF_PORT);
  const char *host = program_context_get (context, "host");
  if (!host) host = "localhost";
  if (argc >= 2)
    {
    char **_argv = malloc (argc * sizeof (char *));
    for (int i = 1; i < argc; i++)
      _argv[i - 1] = strdup (argv[i]);
    program_do_cmd (context, host, port, argc - 1, _argv);
    for (int i = 1; i < argc; i++)
      free (_argv[i - 1]);
    free (_argv); 
    }
  else
    {
    usage_show (stderr, argv[0]);
    }
  LOG_OUT
  return 0;
  }

