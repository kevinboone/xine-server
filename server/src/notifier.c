/*==========================================================================

  xine-server 
  notifier.c
  Copyright (c)2020 Kevin Boone
  Distributed under the terms of the GPL v3.0

==========================================================================*/
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <wchar.h>
#include <time.h>
#include <stdarg.h>

#ifdef NOT_YET_IMPLEMENTED
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#include "feature.h" 
#include "program_context.h" 
#include "log.h" 
#include "program.h" 
#include "string.h" 
#include "server.h" 
#include "cmdproc.h" 
#include "notifier.h" 
#include "../../api/xine-server-api.h" 

struct _Notifier
  {
  }; 


/*==========================================================================

  notifier_create

==========================================================================*/
Notifier *notifier_create (void)
  {
  LOG_IN
  log_debug ("%s: Creating notifier", __PRETTY_FUNCTION__);
  Notifier *self = malloc (sizeof (Notifier));
  LOG_OUT
  return self;
  }

/*==========================================================================

  notifier_destroy

==========================================================================*/
void notifier_destroy (Notifier *self)
  {
  LOG_IN
  log_debug ("%s: Destroying notifier", __PRETTY_FUNCTION__);
  if (self)
    {
    free (self);
    }
  LOG_OUT
  }

/*==========================================================================

  notifier_notify

==========================================================================*/
void notifier_notify (Notifier *self, XSNotifyClass group, 
       XSNotifyEvent event, const char *fmt,...)
  {
#ifdef NOT_YET_IMPLEMENTED
  char *s = NULL;
  if (fmt)
    {
    va_list ap;
    va_start (ap, fmt);
    vasprintf (&s, fmt, ap);
    va_end (ap);
    }
  else
    {
    // Auto-message -- not yet implemented
    s = strdup ("No message");
    }
  char *msg = NULL;
  asprintf (&msg, "%d %d %s", group, event, s);
  free (s);

  /*
  // Example code to multicast the notifications
  int port = 1900;
  const char *group_address = "224.0.0.5";
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd > 0)
      { 
      struct sockaddr_in addr;
      memset(&addr, 0, sizeof(addr));
      addr.sin_family = AF_INET;
      addr.sin_addr.s_addr = inet_addr(group_address);
      addr.sin_port = htons (port);

      sendto (fd, msg, strlen (msg), 0, (struct sockaddr*) &addr,
            sizeof (addr));
      }
  */

  printf ("NOTIFY %s\n", msg);

  free (msg);
#endif
  }





