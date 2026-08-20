#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "command.h"


static rictus_command_registry_t
g_command_registry;


static int
g_command_registry_initialized =
    0;


/*
 * ------------------------------------------------
 * COMMAND NAME VALIDATION
 * ------------------------------------------------
 */

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


/*
 * ------------------------------------------------
 * COMMAND NAME NORMALIZATION
 * ------------------------------------------------
 */

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


/*
 * ------------------------------------------------
 * GLOBAL REGISTRY INITIALIZATION
 * ------------------------------------------------
 */

static int
rictus_command_initialize(
    void
)
{
    if (
        g_command_registry_initialized
    )
    {
        return 1;
    }


    rictus_command_registry_init(
        &g_command_registry
    );


    g_command_registry_initialized =
        1;


    return 1;
}


/*
 * ------------------------------------------------
 * REGISTRY INIT
 * ------------------------------------------------
 */

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


/*
 * ------------------------------------------------
 * REGISTER CORE COMMAND
 * ------------------------------------------------
 */

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

    rictus_command_handler_t
        *entry;


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


    entry =
        &registry
            ->handlers[
                registry->count
            ];


    memset(
        entry,
        0,
        sizeof(*entry)
    );


    strcpy_s(
        entry->name,
        sizeof(entry->name),
        normalized
    );


    entry->handler =
        handler;


    entry->context =
        context;


    entry->module_owned =
        0;


    ++registry->count;


    return
        RICTUS_COMMAND_OK;
}


/*
 * ------------------------------------------------
 * REGISTER MODULE COMMAND
 * ------------------------------------------------
 */

int
rictus_command_register_module(
    const char *name,
    rictus_module_command_handler_fn handler,
    void *handler_context
)
{
    char normalized[
        RICTUS_COMMAND_NAME_MAX
    ];

    size_t index;

    rictus_command_handler_t
        *entry;


    if (
        name == NULL ||
        handler == NULL
    )
    {
        return 0;
    }


    if (
        !rictus_command_initialize()
    )
    {
        return 0;
    }


    if (
        strlen(name) >=
        sizeof(normalized)
    )
    {
        return 0;
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
        return 0;
    }


    /*
     * A command name has one owner.
     */

    for (
        index = 0;
        index < g_command_registry.count;
        ++index
    )
    {
        if (
            strcmp(
                g_command_registry
                    .handlers[index]
                    .name,
                normalized
            ) == 0
        )
        {
            return 0;
        }
    }


    if (
        g_command_registry.count >=
        RICTUS_COMMAND_REGISTRY_MAX
    )
    {
        return 0;
    }


    entry =
        &g_command_registry
            .handlers[
                g_command_registry.count
            ];


    memset(
        entry,
        0,
        sizeof(*entry)
    );


    strcpy_s(
        entry->name,
        sizeof(entry->name),
        normalized
    );


    entry->module_owned =
        1;


    entry->module_handler =
        handler;


    entry->module_context =
        handler_context;


    ++g_command_registry.count;


    return 1;
}


/*
 * ------------------------------------------------
 * UNREGISTER MODULE COMMAND
 * ------------------------------------------------
 */

int
rictus_command_unregister_module(
    const char *name,
    void *handler_context
)
{
    char normalized[
        RICTUS_COMMAND_NAME_MAX
    ];

    size_t index;


    if (
        name == NULL
    )
    {
        return 0;
    }


    if (
        !rictus_command_initialize()
    )
    {
        return 0;
    }


    if (
        strlen(name) >=
        sizeof(normalized)
    )
    {
        return 0;
    }


    strcpy_s(
        normalized,
        sizeof(normalized),
        name
    );


    rictus_command_name_normalize(
        normalized
    );


    for (
        index = 0;
        index < g_command_registry.count;
        ++index
    )
    {
        rictus_command_handler_t
            *entry;


        entry =
            &g_command_registry
                .handlers[index];


        if (
            !entry->module_owned
        )
        {
            continue;
        }


        if (
            strcmp(
                entry->name,
                normalized
            ) != 0
        )
        {
            continue;
        }


        if (
            entry->module_context !=
            handler_context
        )
        {
            continue;
        }


        /*
         * Compact the registry so no stale module
         * function pointer remains after unload.
         */

        if (
            index + 1 <
            g_command_registry.count
        )
        {
            memmove(
                &g_command_registry
                    .handlers[index],
                &g_command_registry
                    .handlers[index + 1],
                (
                    g_command_registry.count -
                    index -
                    1
                ) *
                sizeof(
                    g_command_registry
                        .handlers[0]
                )
            );
        }


        --g_command_registry.count;


        memset(
            &g_command_registry
                .handlers[
                    g_command_registry.count
                ],
            0,
            sizeof(
                g_command_registry
                    .handlers[0]
            )
        );


        return 1;
    }


    return 0;
}


/*
 * ------------------------------------------------
 * PARSE COMMAND
 * ------------------------------------------------
 */

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


/*
 * ------------------------------------------------
 * MODULE DISPATCH ADAPTER
 * ------------------------------------------------
 */

static rictus_command_result_t
rictus_command_dispatch_module(
    const rictus_command_handler_t *handler,
    const rictus_command_t *command,
    rictus_command_reply_fn reply,
    void *reply_context
)
{
    rictus_module_command_t
        module_command;

    rictus_module_result_t
        module_result;


    if (
        handler == NULL ||
        command == NULL ||
        reply == NULL ||
        handler->module_handler == NULL
    )
    {
        return
            RICTUS_COMMAND_INVALID;
    }


    memset(
        &module_command,
        0,
        sizeof(module_command)
    );


    strcpy_s(
        module_command.sender,
        sizeof(module_command.sender),
        command->sender
    );


    strcpy_s(
        module_command.account,
        sizeof(module_command.account),
        command->account
    );


    strcpy_s(
        module_command.name,
        sizeof(module_command.name),
        command->name
    );


    strcpy_s(
        module_command.arguments,
        sizeof(module_command.arguments),
        command->arguments
    );


    module_result =
        handler->module_handler(
            &module_command,
            reply,
            reply_context,
            handler->module_context
        );


    switch (
        module_result
    )
    {
        case RICTUS_MODULE_OK:

            return
                RICTUS_COMMAND_OK;


        case RICTUS_MODULE_ERR_NOT_FOUND:

            return
                RICTUS_COMMAND_NOT_FOUND;


        case RICTUS_MODULE_ERR_NOT_AUTHORIZED:

            return
                RICTUS_COMMAND_DENIED;


        default:

            return
                RICTUS_COMMAND_FAILED;
    }
}


/*
 * ------------------------------------------------
 * DISPATCH COMMAND
 * ------------------------------------------------
 */

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


        if (
            handler->module_owned
        )
        {
            return
                rictus_command_dispatch_module(
                    handler,
                    command,
                    reply,
                    reply_context
                );
        }


        if (
            handler->handler == NULL
        )
        {
            return
                RICTUS_COMMAND_FAILED;
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


/*
 * ------------------------------------------------
 * PROCESS COMMAND
 * ------------------------------------------------
 */

rictus_command_result_t
rictus_command_process(
    const char *sender,
    const char *account,
    const char *text,
    rictus_command_reply_fn reply,
    void *reply_context
)
{
    rictus_command_t
        command;

    rictus_command_result_t
        result;


    if (
        sender == NULL ||
        account == NULL ||
        text == NULL ||
        reply == NULL
    )
    {
        return
            RICTUS_COMMAND_INVALID;
    }


    if (
        !rictus_command_initialize()
    )
    {
        return
            RICTUS_COMMAND_FAILED;
    }


    memset(
        &command,
        0,
        sizeof(command)
    );


    result =
        rictus_command_parse(
            sender,
            account,
            text,
            &command
        );


    if (
        result !=
        RICTUS_COMMAND_OK
    )
    {
        return
            result;
    }


    result =
        rictus_command_dispatch(
            &g_command_registry,
            &command,
            reply,
            reply_context
        );


    if (
        result ==
        RICTUS_COMMAND_NOT_FOUND
    )
    {
        if (
            !reply(
                reply_context,
                "Unknown command."
            )
        )
        {
            return
                RICTUS_COMMAND_FAILED;
        }
    }


    return
        result;
}


/*
 * ------------------------------------------------
 * RESULT STRING
 * ------------------------------------------------
 */

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