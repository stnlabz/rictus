#include <ctype.h>
#include <stddef.h>
#include <string.h>

#include "command.h"


static int
rictus_command_name_valid(
    const char *name
)
{
    size_t index;


    if (
        name == NULL ||
        name[0] == '\0'
    )
    {
        return 0;
    }


    for (
        index = 0;
        name[index] != '\0';
        ++index
    )
    {
        unsigned char c;


        c =
            (unsigned char)
            name[index];


        if (
            !isalnum(c) &&
            c != '_' &&
            c != '-'
        )
        {
            return 0;
        }
    }


    return 1;
}


static void
rictus_command_name_normalize(
    char *name
)
{
    size_t index;


    if (
        name == NULL
    )
    {
        return;
    }


    for (
        index = 0;
        name[index] != '\0';
        ++index
    )
    {
        name[index] =
            (char)
            tolower(
                (unsigned char)
                name[index]
            );
    }
}


void
rictus_command_registry_init(
    rictus_command_registry_t *registry
)
{
    if (
        registry == NULL
    )
    {
        return;
    }


    memset(
        registry,
        0,
        sizeof(*registry)
    );
}


rictus_command_result_t
rictus_command_register(
    rictus_command_registry_t *registry,
    const char *name,
    rictus_command_handler_fn handler,
    void *context
)
{
    char normalized[
        RICTUS_COMMAND_NAME_MAX
    ];

    size_t index;


    if (
        registry == NULL ||
        name == NULL ||
        handler == NULL
    )
    {
        return
            RICTUS_COMMAND_INVALID;
    }


    if (
        strlen(name) >=
        sizeof(normalized)
    )
    {
        return
            RICTUS_COMMAND_TOO_LONG;
    }


    strcpy_s(
        normalized,
        sizeof(normalized),
        name
    );


    rictus_command_name_normalize(
        normalized
    );


    if (
        !rictus_command_name_valid(
            normalized
        )
    )
    {
        return
            RICTUS_COMMAND_INVALID;
    }


    for (
        index = 0;
        index < registry->count;
        ++index
    )
    {
        if (
            strcmp(
                registry
                    ->handlers[index]
                    .name,
                normalized
            ) == 0
        )
        {
            return
                RICTUS_COMMAND_INVALID;
        }
    }


    if (
        registry->count >=
        RICTUS_COMMAND_REGISTRY_MAX
    )
    {
        return
            RICTUS_COMMAND_FAILED;
    }


    strcpy_s(
        registry
            ->handlers[
                registry->count
            ]
            .name,
        sizeof(
            registry
                ->handlers[
                    registry->count
                ]
                .name
        ),
        normalized
    );


    registry
        ->handlers[
            registry->count
        ]
        .handler =
            handler;


    registry
        ->handlers[
            registry->count
        ]
        .context =
            context;


    ++registry->count;


    return
        RICTUS_COMMAND_OK;
}


rictus_command_result_t
rictus_command_parse(
    const char *sender,
    const char *account,
    const char *text,
    rictus_command_t *command
)
{
    const char *cursor;

    const char *name_start;

    size_t name_length;

    size_t argument_length;


    if (
        sender == NULL ||
        account == NULL ||
        text == NULL ||
        command == NULL
    )
    {
        return
            RICTUS_COMMAND_INVALID;
    }


    memset(
        command,
        0,
        sizeof(*command)
    );


    if (
        text[0] != '!'
    )
    {
        return
            RICTUS_COMMAND_NOT_COMMAND;
    }


    if (
        strlen(sender) >=
            sizeof(command->sender) ||
        strlen(account) >=
            sizeof(command->account)
    )
    {
        return
            RICTUS_COMMAND_TOO_LONG;
    }


    strcpy_s(
        command->sender,
        sizeof(command->sender),
        sender
    );


    strcpy_s(
        command->account,
        sizeof(command->account),
        account
    );


    cursor =
        text + 1;


    while (
        *cursor == ' ' ||
        *cursor == '\t'
    )
    {
        ++cursor;
    }


    if (
        *cursor == '\0'
    )
    {
        return
            RICTUS_COMMAND_INVALID;
    }


    name_start =
        cursor;


    while (
        *cursor != '\0' &&
        *cursor != ' ' &&
        *cursor != '\t'
    )
    {
        ++cursor;
    }


    name_length =
        (size_t)
        (cursor - name_start);


    if (
        name_length == 0 ||
        name_length >=
            sizeof(command->name)
    )
    {
        return
            RICTUS_COMMAND_TOO_LONG;
    }


    memcpy(
        command->name,
        name_start,
        name_length
    );


    command
        ->name[
            name_length
        ] =
            '\0';


    rictus_command_name_normalize(
        command->name
    );


    if (
        !rictus_command_name_valid(
            command->name
        )
    )
    {
        return
            RICTUS_COMMAND_INVALID;
    }


    while (
        *cursor == ' ' ||
        *cursor == '\t'
    )
    {
        ++cursor;
    }


    argument_length =
        strlen(cursor);


    if (
        argument_length >=
        sizeof(command->arguments)
    )
    {
        return
            RICTUS_COMMAND_TOO_LONG;
    }


    if (
        argument_length > 0
    )
    {
        strcpy_s(
            command->arguments,
            sizeof(command->arguments),
            cursor
        );
    }


    return
        RICTUS_COMMAND_OK;
}


rictus_command_result_t
rictus_command_dispatch(
    const rictus_command_registry_t *registry,
    const rictus_command_t *command,
    rictus_command_reply_fn reply,
    void *reply_context
)
{
    size_t index;


    if (
        registry == NULL ||
        command == NULL ||
        reply == NULL
    )
    {
        return
            RICTUS_COMMAND_INVALID;
    }


    for (
        index = 0;
        index < registry->count;
        ++index
    )
    {
        const rictus_command_handler_t
            *handler;


        handler =
            &registry
                ->handlers[index];


        if (
            strcmp(
                handler->name,
                command->name
            ) != 0
        )
        {
            continue;
        }


        return
            handler->handler(
                command,
                reply,
                reply_context,
                handler->context
            );
    }


    return
        RICTUS_COMMAND_NOT_FOUND;
}


const char *
rictus_command_result_string(
    rictus_command_result_t result
)
{
    switch (
        result
    )
    {
        case RICTUS_COMMAND_OK:

            return "OK";


        case RICTUS_COMMAND_NOT_COMMAND:

            return "NOT_COMMAND";


        case RICTUS_COMMAND_INVALID:

            return "INVALID";


        case RICTUS_COMMAND_TOO_LONG:

            return "TOO_LONG";


        case RICTUS_COMMAND_NOT_FOUND:

            return "NOT_FOUND";


        case RICTUS_COMMAND_DENIED:

            return "DENIED";


        case RICTUS_COMMAND_FAILED:

            return "FAILED";


        default:

            return "UNKNOWN";
    }
}