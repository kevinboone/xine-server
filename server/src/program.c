/*==========================================================================

  xine-server 
  program.c
  Copyright (c)2020 Kevin Boone
  Distributed under the terms of the GPL v3.0

==========================================================================*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <wchar.h>
#include <time.h>
#include "feature.h" 
#include "program_context.h" 
#include "log.h" 
#include "program.h" 
#include "string.h" 
#include "server.h" 
#include "cmdproc.h" 
#include "xine_interface.h" 
#include "notifier.h" 
#include "../../api/xine-server-api.h" 

/*==========================================================================
  program_run

  Start here
==========================================================================*/
int program_run (ProgramContext *context)
  {
  if (program_context_get_boolean (context, "list-drivers", FALSE))
    {
    XineInterface *xi = xine_interface_create (NULL, NULL);
    xine_interface_init (xi, NULL);
    const char *const *drivers = xine_interface_list_audio_drivers (xi);
    while (*drivers)
      {
      printf ("%s\n", *drivers);
      drivers++;
      }
    xine_interface_destroy (xi);
    }
  else
    {
    // We need to daemonize before initializing Xine, else
    //  Xine gets in a pickle later
    if (!program_context_get_boolean (context, "debug", FALSE))
     {
     daemon (0, 0);
     }

    Notifier *notifier = notifier_create ();

    XineInterface *xi = xine_interface_create 
       (program_context_get (context, "driver"),
          program_context_get (context, "config"));
   
    char *error = NULL;
    if (xine_interface_init (xi, &error))
      {
      CmdProc *cmdproc = cmdproc_create (xi, notifier);
      
      int port = program_context_get_integer (context, "port", 
	    XINESERVER_DEF_PORT);
      const char *host = program_context_get (context, "host");
      if (!host) host = "127.0.0.1";
      Server *server = server_create (host, port, cmdproc);
      char *error = NULL;


      notifier_notify (notifier, XSNOTIFY_CLASS_SERVER, 
	 XSNOTIFY_EVENT_STARTUP, NOTIFY_MSG_SERVER_STARTUP);
      if (server_start (server, &error))
	{
	// The server thread can be run as a real thread, if we
	//  need to do concurrent work here. This is for future
	//  expansion -- right now we just run the server thread
	//  in this thread
    /*
	BOOL stop = FALSE;
	while (!stop)
	  {
	  usleep (1000000);
	  printf ("tick\n");
	  if (!server_is_running (server)) 
	    stop = TRUE;
	  }
    */
	}
      else
	{
	log_error ("Can't initialize server: %s", error);
	free (error);
	}

      server_destroy (server);
      cmdproc_stop_playback (cmdproc);
      cmdproc_destroy (cmdproc);
      }
    else
      {
      log_error ("Can't initialize Xine: %s", error);
      free (error);
      }

    xine_interface_destroy (xi);
    notifier_notify (notifier, XSNOTIFY_CLASS_SERVER, 
        XSNOTIFY_EVENT_SHUTDOWN, NOTIFY_MSG_SERVER_SHUTDOWN);
    notifier_destroy (notifier);
    }
  return 0;
  }


