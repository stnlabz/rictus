/*
 * STN-LABZ
 * Rictus Core
 *
 * shutdown.c
 *
 * Controlled shutdown state.
 *
 * The first valid shutdown request wins.
 * Later requests do not overwrite the original reason.
 */

#define WIN32_LEAN_AND_MEAN

#include <windows.h>

#include "shutdown.h"


static volatile LONG g_shutdown_requested =
    0;

static volatile LONG g_shutdown_reason =
    RICTUS_SHUTDOWN_NONE;


void rictus_shutdown_reset(void)
{
    InterlockedExchange(
        &g_shutdown_reason,
        RICTUS_SHUTDOWN_NONE
    );

    InterlockedExchange(
        &g_shutdown_requested,
        0
    );
}


int rictus_shutdown_request(
    rictus_shutdown_reason reason
)
{
    LONG previous;

    if (
        reason != RICTUS_SHUTDOWN_OPERATOR &&
        reason != RICTUS_SHUTDOWN_SYSTEM
    )
    {
        return 0;
    }

    previous =
        InterlockedCompareExchange(
            &g_shutdown_requested,
            1,
            0
        );

    if (
        previous != 0
    )
    {
        return 1;
    }

    InterlockedExchange(
        &g_shutdown_reason,
        (LONG)reason
    );

    return 1;
}


int rictus_shutdown_requested(void)
{
    return
        InterlockedCompareExchange(
            &g_shutdown_requested,
            0,
            0
        ) != 0;
}


rictus_shutdown_reason rictus_shutdown_reason_get(void)
{
    return
        (rictus_shutdown_reason)
        InterlockedCompareExchange(
            &g_shutdown_reason,
            0,
            0
        );
}


const char *rictus_shutdown_reason_string(
    rictus_shutdown_reason reason
)
{
    switch (
        reason
    )
    {
        case RICTUS_SHUTDOWN_OPERATOR:

            return "operator";

        case RICTUS_SHUTDOWN_SYSTEM:

            return "system";

        default:

            return "none";
    }
}