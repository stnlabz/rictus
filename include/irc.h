#ifndef RICTUS_IRC_H
#define RICTUS_IRC_H

#include <winsock2.h>

#include "config.h"
#include "tls_win.h"


typedef struct
{
    int sasl_requested;
    int sasl_started;
    int sasl_payload_sent;
    int sasl_complete;

    int registered;
    int joined;

} rictus_irc_state;


/*
 * ------------------------------------------------
 * PRIVATE COMMAND CALLBACK
 * ------------------------------------------------
 *
 * IRC owns transport parsing.
 *
 * Core owns command processing.
 *
 * A private IRC PRIVMSG addressed directly to
 * Rictus may be handed to Core through this
 * callback.
 */

typedef int
(*rictus_irc_command_callback_fn)(
    const char *sender,
    const char *text,
    void *context
);


void irc_set_command_callback(
    rictus_irc_command_callback_fn callback,
    void *context
);


int irc_send(
    rictus_tls *tls,
    SOCKET socket,
    const char *line
);


int irc_handle_line(
    rictus_tls *tls,
    SOCKET socket,
    const rictus_config *config,
    rictus_irc_state *state,
    const char *line
);


#endif