/*
 * STN-LABZ
 * Rictus Intelligence Module
 *
 * seen.c
 *
 * Persistent local duplicate-detection state.
 */

#define WIN32_LEAN_AND_MEAN

#include <windows.h>

#include <stdio.h>
#include <string.h>

#include "seen.h"


#define RICTUS_INTELLIGENCE_STATE_DIRECTORY \
    "C:\\stn-labz\\rictus\\intelligence"


static int
rictus_intelligence_seen_ensure_directory(void)
{
    DWORD attributes;


    attributes =
        GetFileAttributesA(
            RICTUS_INTELLIGENCE_STATE_DIRECTORY
        );


    if (
        attributes !=
        INVALID_FILE_ATTRIBUTES
    )
    {
        return
            (
                attributes &
                FILE_ATTRIBUTE_DIRECTORY
            )
            ? 1
            : 0;
    }


    if (
        CreateDirectoryA(
            RICTUS_INTELLIGENCE_STATE_DIRECTORY,
            NULL
        )
    )
    {
        return 1;
    }


    return
        GetLastError() ==
        ERROR_ALREADY_EXISTS;
}


void
rictus_intelligence_seen_init(
    rictus_intelligence_seen_t *seen
)
{
    if (
        seen == NULL
    )
    {
        return;
    }


    memset(
        seen,
        0,
        sizeof(*seen)
    );
}


int
rictus_intelligence_seen_contains(
    const rictus_intelligence_seen_t *seen,
    const char *fingerprint
)
{
    size_t index;


    if (
        seen == NULL ||
        fingerprint == NULL ||
        fingerprint[0] == '\0'
    )
    {
        return 0;
    }


    for (
        index = 0;
        index < seen->count;
        ++index
    )
    {
        if (
            strcmp(
                seen->fingerprints[index],
                fingerprint
            ) == 0
        )
        {
            return 1;
        }
    }


    return 0;
}


rictus_intelligence_seen_result_t
rictus_intelligence_seen_load(
    rictus_intelligence_seen_t *seen
)
{
    FILE *file =
        NULL;

    char line[
        RICTUS_INTELLIGENCE_ITEM_FINGERPRINT_MAX +
        16
    ];


    if (
        seen == NULL
    )
    {
        return
            RICTUS_INTELLIGENCE_SEEN_INVALID_ARGUMENT;
    }


    rictus_intelligence_seen_init(
        seen
    );


    if (
        !rictus_intelligence_seen_ensure_directory()
    )
    {
        return
            RICTUS_INTELLIGENCE_SEEN_OPEN_FAILED;
    }


    if (
        fopen_s(
            &file,
            RICTUS_INTELLIGENCE_SEEN_PATH,
            "r"
        ) != 0 ||
        file == NULL
    )
    {
        /*
         * First execution is valid.
         *
         * Absence means no prior items have been
         * recorded.
         */

        return
            RICTUS_INTELLIGENCE_SEEN_OK;
    }


    while (
        fgets(
            line,
            sizeof(line),
            file
        ) != NULL
    )
    {
        size_t length;


        length =
            strlen(
                line
            );


        while (
            length > 0 &&
            (
                line[length - 1] == '\r' ||
                line[length - 1] == '\n'
            )
        )
        {
            line[
                length - 1
            ] =
                '\0';

            --length;
        }


        if (
            length == 0
        )
        {
            continue;
        }


        if (
            length >=
            RICTUS_INTELLIGENCE_ITEM_FINGERPRINT_MAX
        )
        {
            fclose(
                file
            );


            return
                RICTUS_INTELLIGENCE_SEEN_OPEN_FAILED;
        }


        if (
            seen->count >=
            RICTUS_INTELLIGENCE_SEEN_MAX
        )
        {
            fclose(
                file
            );


            return
                RICTUS_INTELLIGENCE_SEEN_FULL;
        }


        if (
            rictus_intelligence_seen_contains(
                seen,
                line
            )
        )
        {
            continue;
        }


        memcpy(
            seen->fingerprints[
                seen->count
            ],
            line,
            length + 1
        );


        seen->count++;
    }


    fclose(
        file
    );


    return
        RICTUS_INTELLIGENCE_SEEN_OK;
}


rictus_intelligence_seen_result_t
rictus_intelligence_seen_add(
    rictus_intelligence_seen_t *seen,
    const char *fingerprint
)
{
    FILE *file =
        NULL;

    size_t length;


    if (
        seen == NULL ||
        fingerprint == NULL ||
        fingerprint[0] == '\0'
    )
    {
        return
            RICTUS_INTELLIGENCE_SEEN_INVALID_ARGUMENT;
    }


    if (
        rictus_intelligence_seen_contains(
            seen,
            fingerprint
        )
    )
    {
        return
            RICTUS_INTELLIGENCE_SEEN_OK;
    }


    if (
        seen->count >=
        RICTUS_INTELLIGENCE_SEEN_MAX
    )
    {
        return
            RICTUS_INTELLIGENCE_SEEN_FULL;
    }


    length =
        strlen(
            fingerprint
        );


    if (
        length >=
        RICTUS_INTELLIGENCE_ITEM_FINGERPRINT_MAX
    )
    {
        return
            RICTUS_INTELLIGENCE_SEEN_INVALID_ARGUMENT;
    }


    if (
        !rictus_intelligence_seen_ensure_directory()
    )
    {
        return
            RICTUS_INTELLIGENCE_SEEN_OPEN_FAILED;
    }


    if (
        fopen_s(
            &file,
            RICTUS_INTELLIGENCE_SEEN_PATH,
            "a"
        ) != 0 ||
        file == NULL
    )
    {
        return
            RICTUS_INTELLIGENCE_SEEN_OPEN_FAILED;
    }


    if (
        fprintf(
            file,
            "%s\n",
            fingerprint
        ) < 0
    )
    {
        fclose(
            file
        );


        return
            RICTUS_INTELLIGENCE_SEEN_WRITE_FAILED;
    }


    if (
        fflush(
            file
        ) != 0
    )
    {
        fclose(
            file
        );


        return
            RICTUS_INTELLIGENCE_SEEN_WRITE_FAILED;
    }


    fclose(
        file
    );


    memcpy(
        seen->fingerprints[
            seen->count
        ],
        fingerprint,
        length + 1
    );


    seen->count++;


    return
        RICTUS_INTELLIGENCE_SEEN_OK;
}


const char *
rictus_intelligence_seen_result_string(
    rictus_intelligence_seen_result_t result
)
{
    switch (
        result
    )
    {
        case RICTUS_INTELLIGENCE_SEEN_OK:

            return "OK";


        case RICTUS_INTELLIGENCE_SEEN_INVALID_ARGUMENT:

            return "INVALID_ARGUMENT";


        case RICTUS_INTELLIGENCE_SEEN_OPEN_FAILED:

            return "OPEN_FAILED";


        case RICTUS_INTELLIGENCE_SEEN_WRITE_FAILED:

            return "WRITE_FAILED";


        case RICTUS_INTELLIGENCE_SEEN_FULL:

            return "FULL";


        default:

            return "UNKNOWN";
    }
}