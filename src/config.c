#include "config.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>

static void trim(
    char* text
)
{
    char* start;
    char* end;

    if (text == NULL)
    {
        return;
    }

    start = text;

    while (
        *start != '\0' &&
        isspace((unsigned char)*start)
        )
    {
        ++start;
    }

    if (start != text)
    {
        memmove(
            text,
            start,
            strlen(start) + 1
        );
    }

    end =
        text + strlen(text);

    while (
        end > text &&
        isspace((unsigned char)*(end - 1))
        )
    {
        --end;
    }

    *end = '\0';
}

static int copy_value(
    char* destination,
    const char* value
)
{
    size_t length;

    if (
        destination == NULL ||
        value == NULL
        )
    {
        return 0;
    }

    length = strlen(value);

    if (
        length >=
        RICTUS_CONFIG_VALUE_MAX
        )
    {
        return 0;
    }

    memcpy(
        destination,
        value,
        length + 1
    );

    return 1;
}

static int set_value(
    rictus_config* config,
    const char* key,
    const char* value
)
{
    char* destination = NULL;

    if (
        strcmp(
            key,
            "irc_server"
        ) == 0
        )
    {
        destination =
            config->irc_server;
    }
    else if (
        strcmp(
            key,
            "irc_port"
        ) == 0
        )
    {
        destination =
            config->irc_port;
    }
    else if (
        strcmp(
            key,
            "irc_nick"
        ) == 0
        )
    {
        destination =
            config->irc_nick;
    }
    else if (
        strcmp(
            key,
            "irc_account"
        ) == 0
        )
    {
        destination =
            config->irc_account;
    }
    else if (
        strcmp(
            key,
            "irc_password"
        ) == 0
        )
    {
        destination =
            config->irc_password;
    }
    else if (
        strcmp(
            key,
            "irc_channel"
        ) == 0
        )
    {
        destination =
            config->irc_channel;
    }
    else
    {
        /*
         * Unknown configuration keys are ignored
         * for now.
         */
        return 1;
    }

    if (
        !copy_value(
            destination,
            value
        )
        )
    {
        fprintf(
            stderr,
            "Configuration value too long for key: %s\n",
            key
        );

        return 0;
    }

    return 1;
}

int config_load(
    const char* path,
    rictus_config* config
)
{
    FILE* file = NULL;

    char line[512];

    if (
        path == NULL ||
        config == NULL
        )
    {
        return 0;
    }

    memset(
        config,
        0,
        sizeof(*config)
    );

    if (
        fopen_s(
            &file,
            path,
            "r"
        ) != 0 ||
        file == NULL
        )
    {
        fprintf(
            stderr,
            "Unable to open configuration file: %s\n",
            path
        );

        return 0;
    }

    while (
        fgets(
            line,
            sizeof(line),
            file
        ) != NULL
        )
    {
        char* equals;
        char* key;
        char* value;

        trim(
            line
        );

        /*
         * Blank line.
         */
        if (
            line[0] == '\0'
            )
        {
            continue;
        }

        /*
         * Comment.
         */
        if (
            line[0] == '#'
            )
        {
            continue;
        }

        equals =
            strchr(
                line,
                '='
            );

        if (
            equals == NULL
            )
        {
            fprintf(
                stderr,
                "Ignoring malformed configuration line.\n"
            );

            continue;
        }

        *equals = '\0';

        key = line;
        value = equals + 1;

        trim(
            key
        );

        trim(
            value
        );

        if (
            !set_value(
                config,
                key,
                value
            )
            )
        {
            fclose(
                file
            );

            return 0;
        }
    }

    fclose(
        file
    );

    /*
     * Required fields.
     */
    if (
        config->irc_server[0] == '\0' ||
        config->irc_port[0] == '\0' ||
        config->irc_nick[0] == '\0' ||
        config->irc_account[0] == '\0' ||
        config->irc_password[0] == '\0' ||
        config->irc_channel[0] == '\0'
        )
    {
        fprintf(
            stderr,
            "Configuration is incomplete.\n"
        );

        return 0;
    }

    return 1;
}