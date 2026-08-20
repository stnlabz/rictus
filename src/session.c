/*
 * STN-LABZ
 * Rictus Core
 *
 * session.c
 *
 * Session receive-boundary classification.
 */

#include "session.h"
#include "log.h"


rictus_session_result rictus_session_classify_receive(
    int received,
    const rictus_config *config,
    const rictus_irc_state *state
)
{
    if (
        config == NULL ||
        state == NULL
    )
    {
        return RICTUS_SESSION_RECEIVE_FAILURE;
    }

    if (
        received > 0
    )
    {
        return RICTUS_SESSION_CONTINUE;
    }

    if (
        received < 0
    )
    {
        if (
            !rictus_log_write(
                "ERROR",
                "IRC_RECEIVE_FAILED",
                "registered=%d joined=%d channel=%s",
                state->registered,
                state->joined,
                config->irc_channel
            )
        )
        {
            return RICTUS_SESSION_RECEIVE_FAILURE;
        }

        return RICTUS_SESSION_RECEIVE_FAILURE;
    }

    /*
     * received == 0
     *
     * A transport that disappears without an IRC
     * QUIT/ERROR is not a controlled shutdown.
     *
     * Preserve the last confirmed operational state
     * and classify the event as silent session loss.
     */

    if (
        !rictus_log_write(
            "ERROR",
            "SILENT_SESSION_LOSS",
            "registered=%d sasl=%d joined=%d channel=%s",
            state->registered,
            state->sasl_complete,
            state->joined,
            config->irc_channel
        )
    )
    {
        return RICTUS_SESSION_RECEIVE_FAILURE;
    }

    return RICTUS_SESSION_SILENT_LOSS;
}
