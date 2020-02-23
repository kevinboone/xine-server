/*==========================================================================

  boilerplate
  main.c
  Copyright (c)2020 Kevin Boone
  Distributed under the terms of the GPL v3.0

  This is the program entry point. There will not usually be any non-
  boilerplate code in this file. It sets up the ProgramContext object
  and then calls program_run to start the real program.

==========================================================================*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>
#include <syslog.h>
#include "program_context.h" 
#include "program.h" 
#include "console.h" 

/*==========================================================================
log_handler()
==========================================================================*/
void log_handler (int level, const char *message, void *userdata)
  {
  const ProgramContext *context = (ProgramContext *)userdata;
  if (program_context_is_debug (context))
    {
    const char *s_level = "ERROR";
    switch (level)
      {
      case MYLOG_ERROR: s_level = "ERROR"; break;
      case MYLOG_WARNING: s_level = "WARN"; break;
      case MYLOG_INFO: s_level = "INFO"; break;
      case MYLOG_DEBUG: s_level = "DEBUG"; break;
      case MYLOG_TRACE: s_level = "TRACE"; break;
      }
    printf (NAME " %s: %s\n", s_level , message);
    }
  else
    {
    if (level < 3)
      {
      openlog (NAME, 0, LOG_USER);
      switch (level)

        {
        case MYLOG_ERROR:
          syslog (LOG_ERR, "%s", message);
          break;
        case MYLOG_WARNING:
          syslog (LOG_WARNING, "%s", message);
          break;
        case MYLOG_INFO:
          syslog (LOG_INFO, "%s", message);
          break;
        }
  
      closelog (); 
      }
    }
  }


/*==========================================================================
main()
==========================================================================*/
int main (int argc, char **argv)
  {
  int ret = 0; // Exit value

  ProgramContext *context = program_context_create();
  program_context_read_rc_files (context, NAME ".rc");

  if (program_context_parse_command_line (context, argc, argv))
    {
    program_context_setup_logging (context, log_handler, context);
    program_context_query_console (context);

    log_info (NAME " starting up");

    ret = program_run (context);

    log_info (NAME " shutting down");
    }

  console_reset(); // Tidy up any format changes
  program_context_destroy (context);

  return ret;
  }

