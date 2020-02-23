/*==========================================================================

  xine-server 
  server.c
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
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>
#include "feature.h" 
#include "defs.h" 
#include "log.h" 
#include "string.h" 
#include "server.h" 
#include "cmdproc.h" 

struct _Server
  {
  int sock;
  int port;
  char *host;
  struct sockaddr_in address; 
  BOOL inited;
  CmdProc *cmdproc;
  BOOL is_running;
  }; 


/*==========================================================================

  server_create

==========================================================================*/
Server *server_create (const char *host, int port, CmdProc *cmdproc)
  {
  LOG_IN
  log_debug ("%s: Creating server, port=%d", __PRETTY_FUNCTION__, port);
  Server *self = malloc (sizeof (Server));
  self->sock = 0;
  self->port = port;
  self->host = strdup (host);
  self->inited = FALSE;
  self->cmdproc = cmdproc;
  self->is_running = FALSE;
  LOG_OUT
  return self;
  }

/*==========================================================================

  server_thread 

==========================================================================*/
static void *server_thread (void *arg)
  {
  LOG_IN
  Server *self = (Server *)arg;
  int addrlen = sizeof(self->address); 
  log_debug ("%s: server thread start", __PRETTY_FUNCTION__); 
  while (!cmdproc_has_requested_shutdown (self->cmdproc))
    {
    int client_sock = accept (self->sock, (struct sockaddr *)&self->address, 
          (socklen_t*)&addrlen);
    log_debug ("%s: Accepted client connection, socket=%d", 
       __PRETTY_FUNCTION__, client_sock);

    String *buff = string_create_empty();
    BOOL got_line = FALSE; 
    int n;
    do
      {
      char buf[1];
      n = read (client_sock, &buf, 1);
      if (buf[0] == '\r')
        got_line = TRUE;
      else
        string_append_byte (buff, buf[0]); 
      } while (n > 0 && !got_line); 

    log_debug ("%s: Client said: %s", __PRETTY_FUNCTION__, 
          string_cstr(buff));
  
    char *response;
    cmdproc_do_cmd (self->cmdproc, string_cstr (buff), &response);

    write (client_sock, response, strlen (response));
    
    free (response); 
    string_destroy (buff);

    close (client_sock);
    }
  log_debug ("%s: server thread finished", __PRETTY_FUNCTION__); 
  self->is_running = FALSE;
  LOG_OUT
  return NULL;
  }

/*==========================================================================

  server_start

==========================================================================*/
BOOL server_start (Server *self, char **error)
  {
  LOG_IN
  BOOL ret = TRUE;
  log_debug ("%s: server start", __PRETTY_FUNCTION__);

  if (server_init (self, error))
    {
    log_debug ("%s: server init OK", __PRETTY_FUNCTION__);
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, 1);

    /* The server thread is currently not run as a separate thread
       but, for future expansion, it can be */

    // Need to create in detached state, else memory
    //  will never be freed because nothing calls
    //  pthread_join() on the thread
    /*
    pthread_t p;
    self->is_running = TRUE;
    pthread_create (&p, &attr, server_thread, self);
    pthread_attr_destroy(&attr);
    */
    server_thread (self);
    }
  else
    {
    log_debug ("%s: server init failed", __PRETTY_FUNCTION__);
    ret = FALSE;
    }
  
  LOG_OUT
  return ret;
  }

/*==========================================================================

  server_init

==========================================================================*/
BOOL server_init (Server *self, char **error)
  {
  LOG_IN
  BOOL ret = FALSE;
  log_debug ("%s: server_init, port=%d", __PRETTY_FUNCTION__, self->port);

  self->sock = socket (AF_INET, SOCK_STREAM, 0);  

  if (self->sock >= 0)
    {
    memset (&self->address, 0, sizeof (self->address));
    self->address.sin_family = AF_INET;
    self->address.sin_port = htons (self->port);
    self->address.sin_addr.s_addr = inet_addr(self->host);
    if (bind (self->sock, (struct sockaddr *)&self->address, 
          sizeof (self->address)) == 0)
      {
      if (listen(self->sock, 5) == 0) 
        { 
        ret = TRUE;
        self->inited = TRUE;
        } 
      else
        {
        asprintf (error, "Can't listen to socket: %s", strerror (errno)); 
        }
      }
    else
      {
      asprintf (error, "Can't bind to socket: %s", strerror (errno)); 
      }
    }
  else
    {
    asprintf (error, "Can't create socket: %s", strerror (errno)); 
    }

  LOG_OUT
  return ret;
  }


/*==========================================================================

  server_destroy

==========================================================================*/
void server_destroy (Server *self)
  {
  LOG_IN
  log_debug ("%s: destroying server", __PRETTY_FUNCTION__);
  if (self)
    {
    if (self->sock) close (self->sock);
    if (self->host) free (self->host);
    free (self);
    }
  LOG_OUT
  }


/*==========================================================================

  server_is_running

==========================================================================*/
BOOL server_is_running (const Server *self)
  {
  return self->is_running;
  }



