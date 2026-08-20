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