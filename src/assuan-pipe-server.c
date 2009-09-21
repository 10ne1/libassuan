/* assuan-pipe-server.c - Assuan server working over a pipe 
   Copyright (C) 2001, 2002, 2009 Free Software Foundation, Inc.

   This file is part of Assuan.

   Assuan is free software; you can redistribute it and/or modify it
   under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation; either version 2.1 of
   the License, or (at your option) any later version.

   Assuan is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef HAVE_W32_SYSTEM
#include <windows.h>
#include <fcntl.h>
#endif

#include "assuan-defs.h"


static void
deinit_pipe_server (assuan_context_t ctx)
{
  /* nothing to do for this simple server */
}

static gpg_error_t
accept_connection (assuan_context_t ctx)
{
  /* This is a NOP for a pipe server */
  return 0;
}

static void
finish_connection (assuan_context_t ctx)
{
  /* This is a NOP for a pipe server */
}


/* Returns true if atoi(S) denotes a valid socket. */
#ifndef HAVE_W32_SYSTEM
static int
is_valid_socket (const char *s)
{
  struct stat buf;

  if ( fstat (atoi (s), &buf ) )
    return 0;
  return S_ISSOCK (buf.st_mode);
}
#endif /*!HAVE_W32_SYSTEM*/


gpg_error_t
assuan_init_pipe_server (assuan_context_t ctx, int filedes[2])
{
  const char *s;
  unsigned long ul;
  gpg_error_t rc;
  assuan_fd_t infd = ASSUAN_INVALID_FD;
  assuan_fd_t outfd = ASSUAN_INVALID_FD;
  int is_usd = 0;
  static struct assuan_io io = { _assuan_simple_read, _assuan_simple_write,
				 0, 0 };

  rc = _assuan_register_std_commands (ctx);
  if (rc)
    return rc;

#ifdef HAVE_W32_SYSTEM
  /* MS Windows has so many different types of handle that one needs
     to tranlsate them at many place forth and back.  Also make sure
     that the file descriptors are in binary mode.  */
  setmode (filedes[0], O_BINARY);
  setmode (filedes[1], O_BINARY);
  infd  = (void*)_get_osfhandle (filedes[0]);
  outfd = (void*)_get_osfhandle (filedes[1]);
#else
  s = getenv ("_assuan_connection_fd");
  if (s && *s && is_valid_socket (s))
    {
      /* Well, we are called with an bi-directional file descriptor.
	 Prepare for using sendmsg/recvmsg.  In this case we ignore
	 the passed file descriptors. */
      infd = atoi (s);
      outfd = atoi (s);
      is_usd = 1;

    }
  else if (filedes && filedes[0] != ASSUAN_INVALID_FD 
	   && filedes[1] != ASSUAN_INVALID_FD )
    {
      /* Standard pipe server. */
      infd = filedes[0];
      outfd = filedes[1];
    }
  else
    return _assuan_error (ctx, GPG_ERR_ASS_SERVER_START);
#endif

  ctx->is_server = 1;
  ctx->engine.release = deinit_pipe_server;
  ctx->pipe_mode = 1;

  s = getenv ("_assuan_pipe_connect_pid");
  if (s && (ul=strtoul (s, NULL, 10)) && ul)
    ctx->pid = (pid_t)ul;
  else
    ctx->pid = (pid_t)-1;
  ctx->accept_handler = accept_connection;
  ctx->finish_handler = finish_connection;
  ctx->deinit_handler = deinit_pipe_server;
  ctx->inbound.fd = infd;
  ctx->outbound.fd = outfd;

  if (is_usd)
    {
      _assuan_init_uds_io (ctx);
      ctx->deinit_handler = _assuan_uds_deinit;
    }
  else
    ctx->io = &io;

  return 0;
}


void
_assuan_deinit_server (assuan_context_t ctx)
{
  /* We use this function pointer to avoid linking other server when
     not needed but still allow for a generic deinit function.  */
  ctx->deinit_handler (ctx);
  ctx->deinit_handler = NULL;
 
  _assuan_inquire_release (ctx);
  _assuan_free (ctx, ctx->hello_line);
  ctx->hello_line = NULL;
  _assuan_free (ctx, ctx->okay_line);
  ctx->okay_line = NULL;
  _assuan_free (ctx, ctx->cmdtbl);
  ctx->cmdtbl = NULL;
}
