#ifndef RICTUS_COMMAND_H
#define RICTUS_COMMAND_H

#include <stddef.h>


#define RICTUS_COMMAND_NAME_MAX      64
#define RICTUS_COMMAND_ARGUMENTS_MAX 512
#define RICTUS_COMMAND_SENDER_MAX    128
#define RICTUS_COMMAND_ACCOUNT_MAX   128

#define RICTUS_COMMAND_REGISTRY_MAX  64


typedef enum
{
    RICTUS_COMMAND_OK = 0,

    RICTUS_COMMAND_NOT_COMMAND,

    RICTUS_COMMAND_INVALID,

    RICTUS_COMMAND_TOO_LONG,

    RICTUS_COMMAND_NOT_FOUND,

    RICTUS_COMMAND_DENIED,

    RICTUS_COMMAND_FAILED

} rictus_command_result_t;


typedef struct
{
    char sender[
        RICTUS_COMMAND_SENDER_MAX
    ];

    char account[
        RICTUS_COMMAND_ACCOUNT_MAX
    ];

    char name[
        RICTUS_COMMAND_NAME_MAX
    ];

    char arguments[
        RICTUS_COMMAND_ARGUMENTS_MAX
    ];

} rictus_command_t;


typedef int
(*rictus_command_reply_fn)(
    void *context,
    const char *message
);


typedef rictus_command_result_t
(*rictus_command_handler_fn)(
    const rictus_command_t *command,
    rictus_command_reply_fn reply,
    void *reply_context,
    void *handler_context
);


typedef struct
{
    char name[
        RICTUS_COMMAND_NAME_MAX
    ];

    rictus_command_handler_fn
        handler;

    void *
        context;

} rictus_command_handler_t;


typedef struct
{
    rictus_command_handler_t handlers[
        RICTUS_COMMAND_REGISTRY_MAX
    ];

    size_t count;

} rictus_command_registry_t;


void
rictus_command_registry_init(
    rictus_command_registry_t *registry
);


rictus_command_result_t
rictus_command_register(
    rictus_command_registry_t *registry,
    const char *name,
    rictus_command_handler_fn handler,
    void *context
);


rictus_command_result_t
rictus_command_parse(
    const char *sender,
    const char *account,
    const char *text,
    rictus_command_t *command
);


rictus_command_result_t
rictus_command_dispatch(
    const rictus_command_registry_t *registry,
    const rictus_command_t *command,
    rictus_command_reply_fn reply,
    void *reply_context
);


const char *
rictus_command_result_string(
    rictus_command_result_t result
);


#endif