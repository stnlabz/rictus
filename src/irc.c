#define WIN32_LEAN_AND_MEAN

#include <winsock2.h>
#include <windows.h>

#include <stdio.h>
#include <string.h>

#include "config.h"
#include "log.h"
#include "tls_win.h"
#include "sasl.h"
#include "irc.h"
#include "command.h"


#define IRC_BUFFER_SIZE 8192
#define IRC_COMMAND_BUFFER_SIZE 1024


typedef struct
{
    rictus_tls *tls;

    SOCKET socket;

    char target[
        256
    ];

} rictus_irc_command_reply_context;


/*
 * ------------------------------------------------
 * COMMAND REPLY
 * ------------------------------------------------
 */

static int
irc_command_reply(
    void *context,
    const char *message
)
{
    rictus_irc_command_reply_context
        *reply_context;

    char command[
        IRC_COMMAND_BUFFER_SIZE
    ];

    int written;


    if (
        context == NULL ||
        message == NULL ||
        message[0] == '\0'
    )
    {
        return 0;
    }


    reply_context =
        (rictus_irc_command_reply_context *)
        context;


    if (
        reply_context->tls == NULL ||
        reply_context->socket ==
            INVALID_SOCKET ||
        reply_context->target[0] ==
            '\0'
    )
    {
        return 0;
    }


    written =
        snprintf(
            command,
            sizeof(command),
            "PRIVMSG %s :%s",
            reply_context->target,
            message
        );


    if (
        written <= 0 ||
        written >=
            (int)sizeof(command)
    )
    {
        return 0;
    }


    return
        irc_send(
            reply_context->tls,
            reply_context->socket,
            command
        );
}


/*
 * ------------------------------------------------
 * PRIVATE PRIVMSG
 * ------------------------------------------------
 */

static int
parse_private_privmsg(
    rictus_tls *tls,
    SOCKET socket,
    const char *line,
    const rictus_config *config
)
{
    char copy[
        IRC_BUFFER_SIZE
    ];

    char sender[
        256
    ];

    char *context =
        NULL;

    char *prefix;
    char *command;
    char *target;
    char *text;

    char *bang;

    size_t sender_length;

    rictus_irc_command_reply_context
        reply_context;

    rictus_command_result_t
        command_result;


    if (
        tls == NULL ||
        line == NULL ||
        config == NULL
    )
    {
        return 0;
    }


    if (
        strlen(line) >=
        sizeof(copy)
    )
    {
        return 0;
    }


    if (
        line[0] != ':'
    )
    {
        return 0;
    }


    strcpy_s(
        copy,
        sizeof(copy),
        line
    );


    prefix =
        strtok_s(
            copy,
            " ",
            &context
        );


    command =
        strtok_s(
            NULL,
            " ",
            &context
        );


    target =
        strtok_s(
            NULL,
            " ",
            &context
        );


    if (
        prefix == NULL ||
        command == NULL ||
        target == NULL
    )
    {
        return 0;
    }


    if (
        strcmp(
            command,
            "PRIVMSG"
        ) != 0
    )
    {
        return 0;
    }


    /*
     * Direct PM only.
     */

    if (
        strcmp(
            target,
            config->irc_nick
        ) != 0
    )
    {
        return 0;
    }


    text =
        context;


    if (
        text == NULL
    )
    {
        return 1;
    }


    while (
        *text == ' '
    )
    {
        ++text;
    }


    if (
        *text == ':'
    )
    {
        ++text;
    }


    if (
        *text == '\0'
    )
    {
        return 1;
    }


    /*
     * Ordinary PM text is not a command.
     */

    if (
        text[0] != '!'
    )
    {
        return 1;
    }


    if (
        prefix[0] == ':'
    )
    {
        ++prefix;
    }


    bang =
        strchr(
            prefix,
            '!'
        );


    if (
        bang != NULL
    )
    {
        sender_length =
            (size_t)
            (bang - prefix);
    }
    else
    {
        sender_length =
            strlen(prefix);
    }


    if (
        sender_length == 0 ||
        sender_length >=
        sizeof(sender)
    )
    {
        rictus_log_write(
            "WARN",
            "IRC_COMMAND_SENDER_INVALID",
            ""
        );


        return 1;
    }


    memcpy(
        sender,
        prefix,
        sender_length
    );


    sender[
        sender_length
    ] =
        '\0';


    memset(
        &reply_context,
        0,
        sizeof(reply_context)
    );


    reply_context.tls =
        tls;


    reply_context.socket =
        socket;


    strcpy_s(
        reply_context.target,
        sizeof(reply_context.target),
        sender
    );


    rictus_log_write(
        "INFO",
        "COMMAND_RECEIVED",
        "transport=IRC sender=%s",
        sender
    );


    command_result =
        rictus_command_process(
            sender,
            "",
            text,
            irc_command_reply,
            &reply_context
        );


    rictus_log_write(
        command_result ==
            RICTUS_COMMAND_OK ||
        command_result ==
            RICTUS_COMMAND_NOT_FOUND
        ? "INFO"
        : "WARN",
        "COMMAND_RESULT",
        "transport=IRC sender=%s result=%s",
        sender,
        rictus_command_result_string(
            command_result
        )
    );


    return 1;
}


/*
 * ------------------------------------------------
 * CHANNEL MODE
 * ------------------------------------------------
 */

static int
parse_rictus_mode(
    const char *line,
    const rictus_config *config
)
{
    char copy[
        IRC_BUFFER_SIZE
    ];

    char *context =
        NULL;

    char *source;
    char *command;
    char *channel;
    char *mode;
    char *target;


    if (
        line == NULL ||
        config == NULL
    )
    {
        return 0;
    }


    if (
        strlen(line) >=
        sizeof(copy)
    )
    {
        return 0;
    }


    strcpy_s(
        copy,
        sizeof(copy),
        line
    );


    source =
        strtok_s(
            copy,
            " ",
            &context
        );


    command =
        strtok_s(
            NULL,
            " ",
            &context
        );


    channel =
        strtok_s(
            NULL,
            " ",
            &context
        );


    mode =
        strtok_s(
            NULL,
            " ",
            &context
        );


    target =
        strtok_s(
            NULL,
            " ",
            &context
        );


    if (
        source == NULL ||
        command == NULL ||
        channel == NULL ||
        mode == NULL ||
        target == NULL
    )
    {
        return 0;
    }


    if (
        strcmp(
            command,
            "MODE"
        ) != 0
    )
    {
        return 0;
    }


    if (
        strcmp(
            channel,
            config->irc_channel
        ) != 0
    )
    {
        return 0;
    }


    if (
        strcmp(
            target,
            config->irc_nick
        ) != 0
    )
    {
        return 0;
    }


    if (
        source[0] == ':'
    )
    {
        ++source;
    }


    if (
        strcmp(
            mode,
            "+v"
        ) == 0
    )
    {
        rictus_log_write(
            "INFO",
            "CHANNEL_MODE",
            "source=%s channel=%s mode=+v target=%s",
            source,
            channel,
            target
        );


        return 1;
    }


    if (
        strcmp(
            mode,
            "-v"
        ) == 0
    )
    {
        rictus_log_write(
            "WARN",
            "CHANNEL_MODE",
            "source=%s channel=%s mode=-v target=%s",
            source,
            channel,
            target
        );


        return 1;
    }


    if (
        strcmp(
            mode,
            "+o"
        ) == 0
    )
    {
        rictus_log_write(
            "INFO",
            "CHANNEL_MODE",
            "source=%s channel=%s mode=+o target=%s",
            source,
            channel,
            target
        );


        return 1;
    }


    if (
        strcmp(
            mode,
            "-o"
        ) == 0
    )
    {
        rictus_log_write(
            "WARN",
            "CHANNEL_MODE",
            "source=%s channel=%s mode=-o target=%s",
            source,
            channel,
            target
        );


        return 1;
    }


    rictus_log_write(
        "INFO",
        "CHANNEL_MODE",
        "source=%s channel=%s mode=%s target=%s",
        source,
        channel,
        mode,
        target
    );


    return 1;
}


/*
 * ------------------------------------------------
 * IRC SEND
 * ------------------------------------------------
 */

int
irc_send(
    rictus_tls *tls,
    SOCKET socket,
    const char *line
)
{
    char buffer[
        2048
    ];

    int length;


    if (
        tls == NULL ||
        line == NULL
    )
    {
        return 0;
    }


    length =
        snprintf(
            buffer,
            sizeof(buffer),
            "%s\r\n",
            line
        );


    if (
        length <= 0 ||
        length >=
            (int)sizeof(buffer)
    )
    {
        return 0;
    }


    if (
        strncmp(
            line,
            "AUTHENTICATE ",
            13
        ) == 0 &&
        strcmp(
            line,
            "AUTHENTICATE PLAIN"
        ) != 0
    )
    {
        printf(
            ">> AUTHENTICATE <redacted>\n"
        );


        rictus_log_write(
            "INFO",
            "IRC_TX",
            "AUTHENTICATE <redacted>"
        );
    }
    else
    {
        printf(
            ">> %s\n",
            line
        );


        rictus_log_write(
            "INFO",
            "IRC_TX",
            "%s",
            line
        );
    }


    return
        tls_send(
            tls,
            socket,
            buffer,
            length
        );
}


/*
 * ------------------------------------------------
 * IRC RECEIVE / SEMANTIC HANDLING
 * ------------------------------------------------
 */

int
irc_handle_line(
    rictus_tls *tls,
    SOCKET socket,
    const rictus_config *config,
    rictus_irc_state *state,
    const char *line
)
{
    if (
        tls == NULL ||
        config == NULL ||
        state == NULL ||
        line == NULL
    )
    {
        return 0;
    }


    printf(
        "<< %s\n",
        line
    );


    rictus_log_write(
        "INFO",
        "IRC_RX",
        "%s",
        line
    );


    /*
     * ------------------------------------------------
     * PING / PONG
     * ------------------------------------------------
     */

    if (
        strncmp(
            line,
            "PING ",
            5
        ) == 0
    )
    {
        char pong[
            1024
        ];

        int length;


        rictus_log_write(
            "INFO",
            "PING",
            "%s",
            line + 5
        );


        length =
            snprintf(
                pong,
                sizeof(pong),
                "PONG %s",
                line + 5
            );


        if (
            length <= 0 ||
            length >=
                (int)sizeof(pong)
        )
        {
            rictus_log_write(
                "ERROR",
                "PONG_BUILD_FAILED",
                ""
            );


            return 0;
        }


        if (
            !irc_send(
                tls,
                socket,
                pong
            )
        )
        {
            rictus_log_write(
                "ERROR",
                "PONG_SEND_FAILED",
                ""
            );


            return 0;
        }


        rictus_log_write(
            "INFO",
            "PONG",
            "%s",
            line + 5
        );


        return 1;
    }


    /*
     * ------------------------------------------------
     * CAP / SASL
     * ------------------------------------------------
     */

    if (
        strstr(
            line,
            " CAP "
        ) != NULL &&
        strstr(
            line,
            " LS "
        ) != NULL &&
        strstr(
            line,
            "sasl"
        ) != NULL &&
        !state->sasl_requested
    )
    {
        rictus_log_write(
            "INFO",
            "SASL_AVAILABLE",
            "server advertises SASL"
        );


        if (
            !irc_send(
                tls,
                socket,
                "CAP REQ :sasl"
            )
        )
        {
            rictus_log_write(
                "ERROR",
                "SASL_CAP_REQUEST_FAILED",
                ""
            );


            return 0;
        }


        state->sasl_requested =
            1;


        return 1;
    }


    if (
        strstr(
            line,
            " CAP "
        ) != NULL &&
        strstr(
            line,
            " ACK "
        ) != NULL &&
        strstr(
            line,
            "sasl"
        ) != NULL &&
        !state->sasl_started
    )
    {
        rictus_log_write(
            "INFO",
            "SASL_ACK",
            "server accepted SASL capability"
        );


        if (
            !irc_send(
                tls,
                socket,
                "AUTHENTICATE PLAIN"
            )
        )
        {
            rictus_log_write(
                "ERROR",
                "SASL_START_FAILED",
                ""
            );


            return 0;
        }


        state->sasl_started =
            1;


        return 1;
    }


    if (
        strcmp(
            line,
            "AUTHENTICATE +"
        ) == 0 &&
        !state->sasl_payload_sent
    )
    {
        char payload[
            1024
        ];

        char authenticate_command[
            1200
        ];

        int length;


        memset(
            payload,
            0,
            sizeof(payload)
        );


        memset(
            authenticate_command,
            0,
            sizeof(authenticate_command)
        );


        if (
            !sasl_build_plain(
                config,
                payload,
                sizeof(payload)
            )
        )
        {
            rictus_log_write(
                "ERROR",
                "SASL_PAYLOAD_BUILD_FAILED",
                ""
            );


            SecureZeroMemory(
                payload,
                sizeof(payload)
            );


            return 0;
        }


        length =
            snprintf(
                authenticate_command,
                sizeof(authenticate_command),
                "AUTHENTICATE %s",
                payload
            );


        if (
            length <= 0 ||
            length >=
                (int)sizeof(authenticate_command)
        )
        {
            rictus_log_write(
                "ERROR",
                "SASL_COMMAND_BUILD_FAILED",
                ""
            );


            SecureZeroMemory(
                authenticate_command,
                sizeof(authenticate_command)
            );


            SecureZeroMemory(
                payload,
                sizeof(payload)
            );


            return 0;
        }


        if (
            !irc_send(
                tls,
                socket,
                authenticate_command
            )
        )
        {
            rictus_log_write(
                "ERROR",
                "SASL_PAYLOAD_SEND_FAILED",
                ""
            );


            SecureZeroMemory(
                authenticate_command,
                sizeof(authenticate_command)
            );


            SecureZeroMemory(
                payload,
                sizeof(payload)
            );


            return 0;
        }


        SecureZeroMemory(
            authenticate_command,
            sizeof(authenticate_command)
        );


        SecureZeroMemory(
            payload,
            sizeof(payload)
        );


        state->sasl_payload_sent =
            1;


        return 1;
    }


    if (
        strstr(
            line,
            " 903 "
        ) != NULL &&
        !state->sasl_complete
    )
    {
        state->sasl_complete =
            1;


        printf(
            "SASL authentication successful.\n"
        );


        rictus_log_write(
            "INFO",
            "SASL_SUCCESS",
            "account=%s",
            config->irc_account
        );


        if (
            !irc_send(
                tls,
                socket,
                "CAP END"
            )
        )
        {
            rictus_log_write(
                "ERROR",
                "CAP_END_FAILED",
                ""
            );


            return 0;
        }


        return 1;
    }


    if (
        strstr(
            line,
            " 904 "
        ) != NULL ||
        strstr(
            line,
            " 905 "
        ) != NULL ||
        strstr(
            line,
            " 906 "
        ) != NULL ||
        strstr(
            line,
            " 907 "
        ) != NULL
    )
    {
        rictus_log_write(
            "ERROR",
            "SASL_FAILURE",
            "server rejected authentication"
        );


        fprintf(
            stderr,
            "SASL authentication failed.\n"
        );


        return 0;
    }


    /*
     * ------------------------------------------------
     * IRC REGISTRATION
     * ------------------------------------------------
     */

    if (
        strstr(
            line,
            " 001 "
        ) != NULL &&
        !state->registered
    )
    {
        state->registered =
            1;


        printf(
            "IRC registration complete.\n"
        );


        rictus_log_write(
            "INFO",
            "IRC_REGISTERED",
            "nick=%s",
            config->irc_nick
        );


        if (
            state->sasl_complete &&
            !state->joined
        )
        {
            char join_command[
                512
            ];

            int length;


            length =
                snprintf(
                    join_command,
                    sizeof(join_command),
                    "JOIN %s",
                    config->irc_channel
                );


            if (
                length <= 0 ||
                length >=
                    (int)sizeof(join_command)
            )
            {
                rictus_log_write(
                    "ERROR",
                    "JOIN_BUILD_FAILED",
                    "channel=%s",
                    config->irc_channel
                );


                return 0;
            }


            if (
                !irc_send(
                    tls,
                    socket,
                    join_command
                )
            )
            {
                rictus_log_write(
                    "ERROR",
                    "JOIN_SEND_FAILED",
                    "channel=%s",
                    config->irc_channel
                );


                return 0;
            }


            printf(
                "Channel join requested: %s\n",
                config->irc_channel
            );


            rictus_log_write(
                "INFO",
                "JOIN_REQUEST",
                "channel=%s",
                config->irc_channel
            );
        }


        return 1;
    }


    /*
     * ------------------------------------------------
     * PRIVATE COMMAND INTAKE
     * ------------------------------------------------
     */

    if (
        strstr(
            line,
            " PRIVMSG "
        ) != NULL
    )
    {
        if (
            parse_private_privmsg(
                tls,
                socket,
                line,
                config
            )
        )
        {
            return 1;
        }
    }


    /*
     * ------------------------------------------------
     * CHANNEL STATE
     * ------------------------------------------------
     */

    if (
        strstr(
            line,
            " JOIN "
        ) != NULL &&
        strstr(
            line,
            config->irc_channel
        ) != NULL &&
        strstr(
            line,
            config->irc_nick
        ) != NULL
    )
    {
        if (
            !state->joined
        )
        {
            state->joined =
                1;


            printf(
                "Rictus joined %s.\n",
                config->irc_channel
            );


            rictus_log_write(
                "INFO",
                "CHANNEL_STATE",
                "state=JOINED channel=%s",
                config->irc_channel
            );
        }


        return 1;
    }


    if (
        strstr(
            line,
            " PART "
        ) != NULL &&
        strstr(
            line,
            config->irc_channel
        ) != NULL &&
        strstr(
            line,
            config->irc_nick
        ) != NULL
    )
    {
        state->joined =
            0;


        rictus_log_write(
            "WARN",
            "CHANNEL_STATE",
            "state=PARTED channel=%s raw=%s",
            config->irc_channel,
            line
        );


        return 1;
    }


    if (
        strstr(
            line,
            " KICK "
        ) != NULL &&
        strstr(
            line,
            config->irc_channel
        ) != NULL &&
        strstr(
            line,
            config->irc_nick
        ) != NULL
    )
    {
        state->joined =
            0;


        rictus_log_write(
            "WARN",
            "CHANNEL_STATE",
            "state=KICKED channel=%s raw=%s",
            config->irc_channel,
            line
        );


        return 1;
    }


    /*
     * ------------------------------------------------
     * CHANNEL PRIVILEGE STATE
     * ------------------------------------------------
     */

    if (
        strstr(
            line,
            " MODE "
        ) != NULL
    )
    {
        parse_rictus_mode(
            line,
            config
        );


        return 1;
    }


    /*
     * ------------------------------------------------
     * SERVER ERROR
     * ------------------------------------------------
     */

    if (
        strncmp(
            line,
            "ERROR ",
            6
        ) == 0 ||
        strncmp(
            line,
            "ERROR :",
            7
        ) == 0
    )
    {
        rictus_log_write(
            "ERROR",
            "IRC_SERVER_ERROR",
            "%s",
            line
        );


        return 1;
    }


    return 1;
}