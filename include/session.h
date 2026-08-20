#ifndef RICTUS_SESSION_H
#define RICTUS_SESSION_H

#include "config.h"
#include "irc.h"

typedef enum
{
    RICTUS_SESSION_CONTINUE = 0,
    RICTUS_SESSION_RECEIVE_FAILURE,
    RICTUS_SESSION_SILENT_LOSS

} rictus_session_result;

rictus_session_result rictus_session_classify_receive(
    int received,
    const rictus_config *config,
    const rictus_irc_state *state
);

#endif
