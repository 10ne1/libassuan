/* assuan-handler.c - dispatch commands 
 *	Copyright (C) 2001 Free Software Foundation, Inc.
 *
 * This file is part of GnuPG.
 *
 * GnuPG is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * GnuPG is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

#include <config.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "assuan-defs.h"

#define digitp(a) ((a) >= '0' && (a) <= '9')


static int
dummy_handler (ASSUAN_CONTEXT ctx, char *line)
{
  return set_error (ctx, Server_Fault, "no handler registered");
}


static int
std_handler_nop (ASSUAN_CONTEXT ctx, char *line)
{
  return 0; /* okay */
}
  
static int
std_handler_cancel (ASSUAN_CONTEXT ctx, char *line)
{
  return set_error (ctx, Not_Implemented, NULL); 
}
  
static int
std_handler_bye (ASSUAN_CONTEXT ctx, char *line)
{
  return -1; /* pretty simple :-) */
}
  
static int
std_handler_auth (ASSUAN_CONTEXT ctx, char *line)
{
  return set_error (ctx, Not_Implemented, NULL); 
}
  
static int
std_handler_reset (ASSUAN_CONTEXT ctx, char *line)
{
  return set_error (ctx, Not_Implemented, NULL); 
}
  
static int
std_handler_end (ASSUAN_CONTEXT ctx, char *line)
{
  return set_error (ctx, Not_Implemented, NULL); 
}

static int
parse_cmd_input_output (ASSUAN_CONTEXT ctx, char *line, int *rfd)
{
  char *endp;

  if (strncmp (line, "FD=", 3))
    return set_error (ctx, Syntax_Error, "FD=<n> expected");
  line += 3;
  if (!digitp (*line))
    return set_error (ctx, Syntax_Error, "number required");
  *rfd = strtoul (line, &endp, 10);
  if (*endp)
    return set_error (ctx, Syntax_Error, "garbage found");
  if (*rfd == ctx->inbound.fd)
    return set_error (ctx, Parameter_Conflict, "fd same as inbound fd");
  if (*rfd == ctx->outbound.fd)
    return set_error (ctx, Parameter_Conflict, "fd same as outbound fd");
  return 0;
}

/* Format is INPUT FD=<n> */
static int
std_handler_input (ASSUAN_CONTEXT ctx, char *line)
{
  int rc, fd;

  rc = parse_cmd_input_output (ctx, line, &fd);
  if (rc)
    return rc;
  ctx->input_fd = fd;
  return 0;
}

/* Format is OUTPUT FD=<n> */
static int
std_handler_output (ASSUAN_CONTEXT ctx, char *line)
{
  int rc, fd;

  rc = parse_cmd_input_output (ctx, line, &fd);
  if (rc)
    return rc;
  ctx->output_fd = fd;
  return 0;
}



  

/* This is a table with the standard commands and handler for them.
   The table is used to initialize a new context and assuciate strings
   and handlers with cmd_ids */
static struct {
  const char *name;
  int cmd_id;
  int (*handler)(ASSUAN_CONTEXT, char *line);
  int always; /* always initializethis command */
} std_cmd_table[] = {
  { "NOP",    ASSUAN_CMD_NOP,    std_handler_nop, 1 },
  { "CANCEL", ASSUAN_CMD_CANCEL, std_handler_cancel, 1 },
  { "BYE",    ASSUAN_CMD_BYE,    std_handler_bye, 1 },
  { "AUTH",   ASSUAN_CMD_AUTH,   std_handler_auth, 1 },
  { "RESET",  ASSUAN_CMD_RESET,  std_handler_reset, 1 },
  { "END",    ASSUAN_CMD_END,    std_handler_end, 1 },

  { "INPUT",  ASSUAN_CMD_INPUT,  std_handler_input },
  { "OUTPUT", ASSUAN_CMD_OUTPUT, std_handler_output },
  { NULL }
};


/**
 * assuan_register_command:
 * @ctx: the server context
 * @cmd_id: An ID value for the command
 * @cmd_name: A string with the command name
 * @handler: The handler function to be called
 * 
 * Register a handler to be used for a given command.
 * 
 * The @cmd_name must be %NULL or an empty string for all @cmd_ids
 * below %ASSUAN_CMD_USER because predefined values are used.
 * 
 * Return value: 
 **/
int
assuan_register_command (ASSUAN_CONTEXT ctx,
                         int cmd_id, const char *cmd_name,
                         int (*handler)(ASSUAN_CONTEXT, char *))
{
  int i;

  if (cmd_name && !*cmd_name)
    cmd_name = NULL;

  if (cmd_id < ASSUAN_CMD_USER)
    { 
      if (cmd_name)
        return ASSUAN_Invalid_Value; /* must be NULL for these values*/

      for (i=0; std_cmd_table[i].name; i++)
        {
          if (std_cmd_table[i].cmd_id == cmd_id)
            {
              cmd_name = std_cmd_table[i].name;
              if (!handler)
                handler = std_cmd_table[i].handler;
              break;
            }
        }
      if (!std_cmd_table[i].name)
        return ASSUAN_Invalid_Value; /* not a pre-registered one */
    }
  
  if (!handler)
    handler = dummy_handler;

  if (!cmd_name)
    return ASSUAN_Invalid_Value;

  fprintf (stderr, "DBG-assuan: registering %d as `%s'\n", cmd_id, cmd_name);

  if (!ctx->cmdtbl)
    {
      ctx->cmdtbl_size = 10;
      ctx->cmdtbl = xtrycalloc ( ctx->cmdtbl_size, sizeof *ctx->cmdtbl);
      if (!ctx->cmdtbl)
        return ASSUAN_Out_Of_Core;
      ctx->cmdtbl_used = 0;
    }
  else if (ctx->cmdtbl_used >= ctx->cmdtbl_size)
    {
      struct cmdtbl_s *x;

      fprintf (stderr, "DBG-assuan: enlarging cmdtbl\n");
      
      x = xtryrealloc ( ctx->cmdtbl, (ctx->cmdtbl_size+10) * sizeof *x);
      if (!x)
        return ASSUAN_Out_Of_Core;
      ctx->cmdtbl = x;
      ctx->cmdtbl_size += 10;
    }

  ctx->cmdtbl[ctx->cmdtbl_used].name = cmd_name;
  ctx->cmdtbl[ctx->cmdtbl_used].cmd_id = cmd_id;
  ctx->cmdtbl[ctx->cmdtbl_used].handler = handler;
  ctx->cmdtbl_used++;
  return 0;
}

/* Helper to register the standards commands */
int
_assuan_register_std_commands (ASSUAN_CONTEXT ctx)
{
  int i, rc;

  for (i=0; std_cmd_table[i].name; i++)
    {
      if (std_cmd_table[i].always)
        {
          rc = assuan_register_command (ctx, std_cmd_table[i].cmd_id,
                                        NULL, NULL);
          if (rc)
            return rc;
        }
    } 
  return 0;
}



/* Process the special data lines.  The "D " has already been removed
   from the line.  As all handlers this function may modify the line.  */
static int
handle_data_line (ASSUAN_CONTEXT ctx, char *line, int linelen)
{
  return set_error (ctx, Not_Implemented, NULL);
}


/* Parse the line, break out the command, find it in the command
   table, remove leading and white spaces from the arguments, all the
   handler with the argument line and return the error */
static int 
dispatch_command (ASSUAN_CONTEXT ctx, char *line, int linelen)
{
  char *p;
  const char *s;
  int shift, i;

  if (*line == 'D' && line[1] == ' ') /* divert to special handler */
    return handle_data_line (ctx, line+2, linelen-2);

  for (p=line; *p && *p != ' ' && *p != '\t'; p++)
    ;
  if (p==line)
    return set_error (ctx, Invalid_Command, "leading white-space"); 
  if (*p) 
    { /* Skip over leading WS after the keyword */
      *p++ = 0;
      while ( *p == ' ' || *p == '\t')
        p++;
    }
  shift = p - line;

  for (i=0; (s=ctx->cmdtbl[i].name); i++)
    if (!strcmp (line, s))
      break;
  if (!s)
    return set_error (ctx, Unknown_Command, NULL);
  line += shift;
  linelen -= shift;

  fprintf (stderr, "DBG-assuan: processing %s `%s'\n", s, line);
  return ctx->cmdtbl[i].handler (ctx, line);
}




/**
 * assuan_process:
 * @ctx: assuan context
 * 
 * This fucntion is used to handle the assuan protocol after a
 * connection has been established using assuan_accept().  This is the
 * main protocol handler.
 * 
 * Return value: 0 on success or an error code if the assuan operation
 * failed.  Note, that no error is returned for operational errors.
 **/
int
assuan_process (ASSUAN_CONTEXT ctx)
{
  int rc;

  do {
    /* Read the line but skip comments */
    do
      {
        rc = _assuan_read_line (ctx);
        if (rc)
          return rc;
      
        fprintf (stderr, "DBG-assuan: got %d bytes `%s'\n",
                 ctx->inbound.linelen, ctx->inbound.line);
      }
    while ( *ctx->inbound.line == '#' || !ctx->inbound.linelen);
  
    /* dispatch comamnd and return reply */
    rc = dispatch_command (ctx, ctx->inbound.line, ctx->inbound.linelen);
    if (!rc)
      rc = _assuan_write_line (ctx, "OK");
    else if (rc == -1)
      { /* No error checking because the peer may have already disconnect */ 
        _assuan_write_line (ctx, "OK  Bye, bye - hope to meet you again");
      }
    else 
      {
        char errline[256];

        if (rc < 100)
          sprintf (errline, "ERR %d server fault (%.50s)",
                   ASSUAN_Server_Fault, assuan_strerror (rc));
        else
          {
            const char *text = ctx->err_no == rc? ctx->err_str:NULL;

            sprintf (errline, "ERR %d %.50s%s%.100s",
                     rc, assuan_strerror (rc), text? " - ":"", text?text:"");
          }
        rc = _assuan_write_line (ctx, errline);
      }
  } while (!rc);

  if (rc == -1)
    rc = 0;

  return rc;
}




