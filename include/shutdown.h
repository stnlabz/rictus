#ifndef RICTUS_SHUTDOWN_H
#define RICTUS_SHUTDOWN_H

/*
 * STN-LABZ
 * Rictus Core
 *
 * Controlled shutdown state.
 */

typedef enum
{
    RICTUS_SHUTDOWN_NONE = 0,
    RICTUS_SHUTDOWN_OPERATOR,
    RICTUS_SHUTDOWN_SYSTEM

} rictus_shutdown_reason;


void rictus_shutdown_reset(void);

int rictus_shutdown_request(
    rictus_shutdown_reason reason
);

int rictus_shutdown_requested(void);

rictus_shutdown_reason rictus_shutdown_reason_get(void);

const char *rictus_shutdown_reason_string(
    rictus_shutdown_reason reason
);

#endif