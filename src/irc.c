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


#define IRC_BUFFER_SIZE 8192


/*
 * ------------------------------------------------
 * COMMAND CALLBACK
 * ------------------------------------------------
 */

static rictus_irc_command_callback_fn
g_command_callback =
    NULL;


static void *
g_command_callback_context =
    NULL;


void irc_set_command_callback(
    rictus_irc_command_callback_fn callback,
    void *context
)
{
    g_command_callback =
        callback;

    g_command_callback_context =
        context;
}


/*
 * ------------------------------------------------
 * PRIVATE PRIVMSG
 * ------------------------------------------------
 */

static int parse_private_privmsg(
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


    /*
     * IRC user messages use a source prefix:
     *
     * :nick!user@host PRIVMSG target :message
     */

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
     * Only direct messages addressed to Rictus
     * enter the command interface.
     *
     * Channel PRIVMSG traffic remains ordinary
     * IRC traffic.
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


    /*
     * The remainder of the line is the message.
     */

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
     * Only command text enters the command
     * callback.
     */

    if (
        text[0] != '!'
    )
    {
        return 1;
    }


    /*
     * Extract sender nick from:
     *
     * :nick!user@host
     */

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


    rictus_log_write(
        "INFO",
        "IRC_COMMAND_RECEIVED",
        "sender=%s",
        sender
    );


    /*
     * Command parsing and dispatch belong to Core.
     */

    if (
        g_command_callback == NULL
    )
    {
        rictus_log_write(
            "WARN",
            "IRC_COMMAND_NO_HANDLER",
            "sender=%s",
            sender
        );

        return 1;
    }


    if (
        !g_command_callback(
            sender,
            text,
            g_command_callback_context
        )
    )
    {
        rictus_log_write(
            "ERROR",
            "IRC_COMMAND_CALLBACK_FAILED",
            "sender=%s",
            sender
        );

        return 1;
    }


    rictus_log_write(
        "INFO",
        "IRC_COMMAND_DELIVERED",
        "sender=%s",
        sender
    );


    return 1;
}


/*
 * ------------------------------------------------
 * CHANNEL MODE
 * ------------------------------------------------
 */

static int parse_rictus_mode(
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

int irc_send(
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


    /*
     * Never expose SASL credential payloads.
     */

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

int irc_handle_line(
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


    /*
     * Preserve raw IRC traffic.
     */

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
     *
     * NOTE:
     *
     * This logic is intentionally preserved exactly
     * from the current Core implementation.
     *
     * Qualification will determine whether these
     * matching rules are sufficiently strict.
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