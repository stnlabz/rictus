/*
 * STN-LABZ
 * Rictus Intelligence Module
 *
 * intelligence.c
 *
 * Intelligence module revision 0.9.1.
 *
 * Responsibilities:
 *
 * - module identity
 * - Core API compatibility
 * - source policy
 * - qualification
 * - lifecycle
 * - worker management
 * - source collection
 * - response normalization
 * - persistent duplicate detection
 * - publication of new source evidence through Core
 *
 * This revision does not approve intelligence,
 * publish controlled knowledge, or write to the
 * corpus.
 */

#define WIN32_LEAN_AND_MEAN

#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "intelligence.h"
#include "collector.h"
#include "parser.h"
#include "seen.h"
#include "record.h"
#include "srt.h"
#include "sources.h"


#define RICTUS_INTELLIGENCE_COLLECTION_INTERVAL_MS \
    (15UL * 60UL * 1000UL)

#define RICTUS_INTELLIGENCE_IRC_MESSAGE_MAX \
    400

#define RICTUS_INTELLIGENCE_CHAIN_INDEX_PATH \
    "C:\\stn-labz\\policies\\policy.index.json"

#define RICTUS_INTELLIGENCE_CHAIN_OUTPUT_MAX \
    8192

#define RICTUS_INTELLIGENCE_RAG_INPUT_DIRECTORY \
    "C:\\stn-labz\\rag\\input"

#define RICTUS_INTELLIGENCE_RAG_LINE_MAX \
    512


static HANDLE
g_intelligence_thread =
NULL;


static HANDLE
g_intelligence_stop_event =
NULL;


static volatile LONG
g_intelligence_running =
0;


static rictus_intelligence_seen_t
g_intelligence_seen;


static rictus_intelligence_record_store_t
g_intelligence_records;


static const char*
g_intelligence_record_path =
"intelligence.records";


static rictus_intelligence_srt_store_t
g_intelligence_srt_requests;


/*
 * Core-owned host services.
 *
 * The module does not own this structure.
 * Core guarantees that it remains valid while
 * the module is ACTIVE.
 */
static const rictus_module_host_t*
g_intelligence_host =
NULL;


/*
 * ------------------------------------------------
 * SOURCE POLICY
 * ------------------------------------------------
 */

rictus_intelligence_source_t
rictus_intelligence_source_from_name(
    const char* name
)
{
    if (
        name == NULL ||
        name[0] == '\0'
        )
    {
        return
            RICTUS_INTELLIGENCE_SOURCE_NONE;
    }


    if (
        strcmp(
            name,
            "NASA"
        ) == 0
        )
    {
        return
            RICTUS_INTELLIGENCE_SOURCE_NASA;
    }


    if (
        strcmp(
            name,
            "SpaceX"
        ) == 0
        )
    {
        return
            RICTUS_INTELLIGENCE_SOURCE_SPACEX;
    }


    return
        RICTUS_INTELLIGENCE_SOURCE_OTHER;
}


rictus_intelligence_source_class_t
rictus_intelligence_source_class(
    rictus_intelligence_source_t source
)
{
    switch (
        source
        )
    {
    case RICTUS_INTELLIGENCE_SOURCE_NASA:
    case RICTUS_INTELLIGENCE_SOURCE_SPACEX:

        return
            RICTUS_INTELLIGENCE_SOURCE_PRIMARY;


    case RICTUS_INTELLIGENCE_SOURCE_OTHER:

        return
            RICTUS_INTELLIGENCE_SOURCE_SECONDARY;


    case RICTUS_INTELLIGENCE_SOURCE_NONE:
    default:

        return
            RICTUS_INTELLIGENCE_SOURCE_INVALID;
    }
}


const char*
rictus_intelligence_source_string(
    rictus_intelligence_source_t source
)
{
    switch (
        source
        )
    {
    case RICTUS_INTELLIGENCE_SOURCE_NASA:

        return "NASA";


    case RICTUS_INTELLIGENCE_SOURCE_SPACEX:

        return "SpaceX";


    case RICTUS_INTELLIGENCE_SOURCE_OTHER:

        return "OTHER";


    default:

        return "NONE";
    }
}


const char*
rictus_intelligence_source_class_string(
    rictus_intelligence_source_class_t source_class
)
{
    switch (
        source_class
        )
    {
    case RICTUS_INTELLIGENCE_SOURCE_PRIMARY:

        return "PRIMARY";


    case RICTUS_INTELLIGENCE_SOURCE_SECONDARY:

        return "SECONDARY";


    default:

        return "INVALID";
    }
}


/*
 * ------------------------------------------------
 * COMMAND: SHOW
 * ------------------------------------------------
 *
 * Resolves one persistent INT record and returns
 * its primary operator-facing fields.
 *
 * Parameters:
 * - command:       Parsed Core-owned command.
 * - reply:         Core-owned reply callback.
 * - reply_context: Core-owned callback context.
 * - handler_context: Unused for this command.
 *
 * Returns a module result describing command
 * handling success or callback failure.
 */
static rictus_module_result_t
rictus_intelligence_command_show(
    const rictus_module_command_t* command,
    rictus_module_command_reply_fn reply,
    void* reply_context,
    void* handler_context
)
{
    const rictus_intelligence_record_t* record;
    char response[1536];

    (void)handler_context;

    if (command == NULL || reply == NULL)
    {
        return RICTUS_MODULE_ERR_INVALID_ARGUMENT;
    }

    if (command->arguments[0] == '\0')
    {
        if (!reply(reply_context, "Usage: !show INT-XXXXXXXX"))
        {
            return RICTUS_MODULE_ERR_START_FAILED;
        }

        return RICTUS_MODULE_OK;
    }

    record =
        rictus_intelligence_record_store_find(
            &g_intelligence_records,
            command->arguments
        );

    if (record == NULL)
    {
        snprintf(
            response,
            sizeof(response),
            "%s: NOT FOUND",
            command->arguments
        );

        if (!reply(reply_context, response))
        {
            return RICTUS_MODULE_ERR_START_FAILED;
        }

        return RICTUS_MODULE_OK;
    }

    snprintf(
        response,
        sizeof(response),
        "%s | %s | %s",
        record->id,
        record->item.source,
        record->item.title
    );

    if (!reply(reply_context, response))
    {
        return RICTUS_MODULE_ERR_START_FAILED;
    }

    if (record->item.published[0] != '\0')
    {
        snprintf(
            response,
            sizeof(response),
            "Published: %s",
            record->item.published
        );

        if (!reply(reply_context, response))
        {
            return RICTUS_MODULE_ERR_START_FAILED;
        }
    }

    if (record->item.url[0] != '\0')
    {
        snprintf(
            response,
            sizeof(response),
            "URL: %s",
            record->item.url
        );

        if (!reply(reply_context, response))
        {
            return RICTUS_MODULE_ERR_START_FAILED;
        }
    }

    return RICTUS_MODULE_OK;
}


/*
 * ------------------------------------------------
 * COMMAND: SRT
 * ------------------------------------------------
 *
 * Operator-directed handoff request.
 *
 * This does not create the final SRT identity.
 * The INT identifier remains the source handle
 * until the Security Research workflow accepts
 * and identifies the research target.
 */
static rictus_module_result_t
rictus_intelligence_command_srt(
    const rictus_module_command_t* command,
    rictus_module_command_reply_fn reply,
    void* reply_context,
    void* handler_context
)
{
    const rictus_intelligence_record_t*
        record;

    const rictus_intelligence_srt_request_t*
        existing;

    char response[
        1200
    ];

    char report_path[
        RICTUS_INTELLIGENCE_SRT_PATH_MAX
    ];


    (void)handler_context;


    if (
        command == NULL ||
        reply == NULL
        )
    {
        return
            RICTUS_MODULE_ERR_INVALID_ARGUMENT;
    }


    if (
        command->arguments[0] == '\0'
        )
    {
        if (
            !reply(
                reply_context,
                "Usage: !srt INT-XXXXXXXX"
            )
            )
        {
            return
                RICTUS_MODULE_ERR_START_FAILED;
        }


        return
            RICTUS_MODULE_OK;
    }


    record =
        rictus_intelligence_record_store_find(
            &g_intelligence_records,
            command->arguments
        );


    if (
        record == NULL
        )
    {
        snprintf(
            response,
            sizeof(response),
            "%s: NOT FOUND",
            command->arguments
        );


        if (
            !reply(
                reply_context,
                response
            )
            )
        {
            return
                RICTUS_MODULE_ERR_START_FAILED;
        }


        return
            RICTUS_MODULE_OK;
    }


    existing =
        rictus_intelligence_srt_store_find(
            &g_intelligence_srt_requests,
            record->id
        );


    if (
        existing != NULL
        )
    {
        snprintf(
            response,
            sizeof(response),
            "SRT ALREADY REQUESTED | %s",
            record->id
        );


        if (
            !reply(
                reply_context,
                response
            )
            )
        {
            return
                RICTUS_MODULE_ERR_START_FAILED;
        }


        return
            RICTUS_MODULE_OK;
    }


    memset(
        report_path,
        0,
        sizeof(report_path)
    );


    if (
        !rictus_intelligence_srt_generate_report(
            RICTUS_INTELLIGENCE_SRT_DIRECTORY,
            record,
            command->sender,
            report_path,
            sizeof(report_path)
        )
        )
    {
        printf(
            "[INTELLIGENCE] SRT report generation failed "
            "id=%s\n",
            record->id
        );


        if (
            !reply(
                reply_context,
                "SRT REQUEST FAILED | REPORT GENERATION FAILED"
            )
            )
        {
            return
                RICTUS_MODULE_ERR_START_FAILED;
        }


        return
            RICTUS_MODULE_OK;
    }


    if (
        !rictus_intelligence_srt_store_append(
            &g_intelligence_srt_requests,
            RICTUS_INTELLIGENCE_SRT_REQUEST_PATH,
            record->id,
            "REQUESTED"
        )
        )
    {
        DeleteFileA(
            report_path
        );


        printf(
            "[INTELLIGENCE] SRT request persistence failed "
            "id=%s\n",
            record->id
        );


        if (
            !reply(
                reply_context,
                "SRT REQUEST FAILED | REQUEST PERSISTENCE FAILED"
            )
            )
        {
            return
                RICTUS_MODULE_ERR_START_FAILED;
        }


        return
            RICTUS_MODULE_OK;
    }


    snprintf(
        response,
        sizeof(response),
        "SRT REQUESTED | %s | HUMAN REVIEW REQUIRED",
        record->id
    );


    if (
        !reply(
            reply_context,
            response
        )
        )
    {
        return
            RICTUS_MODULE_ERR_START_FAILED;
    }


    snprintf(
        response,
        sizeof(response),
        "Report: %s",
        report_path
    );


    if (
        !reply(
            reply_context,
            response
        )
        )
    {
        return
            RICTUS_MODULE_ERR_START_FAILED;
    }


    printf(
        "[INTELLIGENCE] SRT requested "
        "id=%s source=%s title=%s report=%s\n",
        record->id,
        record->item.source,
        record->item.title,
        report_path
    );


    return
        RICTUS_MODULE_OK;
}


/*
 * ------------------------------------------------
 * COMMAND: APPROVE
 * ------------------------------------------------
 *
 * Human-controlled transition from a Pending
 * INT-backed SRT candidate to an Approved SRT.
 *
 * Approval assigns the permanent SRT identity.
 * It does not invoke chain or rag_builder.
 */

static const char *
rictus_intelligence_reviewer_office(
    const char *sender
)
{
    if (
        sender != NULL &&
        _stricmp(sender, "STN_Boss") == 0
        )
    {
        return "CEO / STN Boss";
    }

    return "UNRESOLVED";
}


static rictus_module_result_t
rictus_intelligence_command_approve(
    const rictus_module_command_t *command,
    rictus_module_command_reply_fn reply,
    void *reply_context,
    void *handler_context
)
{
    const rictus_intelligence_srt_request_t *existing;
    const char *office;
    char srt_id[RICTUS_INTELLIGENCE_SRT_ID_MAX];
    char report_path[RICTUS_INTELLIGENCE_SRT_PATH_MAX];
    char response[1200];

    (void)handler_context;

    if (
        command == NULL ||
        reply == NULL
        )
    {
        return RICTUS_MODULE_ERR_INVALID_ARGUMENT;
    }

    if (
        command->arguments[0] == '\0'
        )
    {
        if (
            !reply(
                reply_context,
                "Usage: !approve INT-XXXXXXXX"
            )
            )
        {
            return RICTUS_MODULE_ERR_START_FAILED;
        }

        return RICTUS_MODULE_OK;
    }

    existing =
        rictus_intelligence_srt_store_find(
            &g_intelligence_srt_requests,
            command->arguments
        );

    if (existing == NULL)
    {
        snprintf(
            response,
            sizeof(response),
            "%s: SRT CANDIDATE NOT FOUND",
            command->arguments
        );

        if (!reply(reply_context, response))
        {
            return RICTUS_MODULE_ERR_START_FAILED;
        }

        return RICTUS_MODULE_OK;
    }

    if (
        _stricmp(existing->status, "APPROVED") == 0 &&
        existing->srt_id[0] != '\0'
        )
    {
        snprintf(
            response,
            sizeof(response),
            "SRT ALREADY APPROVED | %s -> %s",
            existing->intelligence_id,
            existing->srt_id
        );

        if (!reply(reply_context, response))
        {
            return RICTUS_MODULE_ERR_START_FAILED;
        }

        return RICTUS_MODULE_OK;
    }

    if (
        _stricmp(existing->status, "REQUESTED") != 0
        )
    {
        snprintf(
            response,
            sizeof(response),
            "SRT APPROVAL REFUSED | %s | STATUS=%s",
            existing->intelligence_id,
            existing->status
        );

        if (!reply(reply_context, response))
        {
            return RICTUS_MODULE_ERR_START_FAILED;
        }

        return RICTUS_MODULE_OK;
    }

    office =
        rictus_intelligence_reviewer_office(
            command->sender
        );

    if (
        _stricmp(office, "UNRESOLVED") == 0
        )
    {
        if (
            !reply(
                reply_context,
                "SRT APPROVAL REFUSED | REVIEWER OFFICE UNRESOLVED"
            )
            )
        {
            return RICTUS_MODULE_ERR_START_FAILED;
        }

        return RICTUS_MODULE_OK;
    }

    memset(srt_id, 0, sizeof(srt_id));
    memset(report_path, 0, sizeof(report_path));

    if (
        !rictus_intelligence_srt_approve(
            &g_intelligence_srt_requests,
            RICTUS_INTELLIGENCE_SRT_REQUEST_PATH,
            RICTUS_INTELLIGENCE_SRT_DIRECTORY,
            existing->intelligence_id,
            command->sender,
            office,
            srt_id,
            sizeof(srt_id),
            report_path,
            sizeof(report_path)
        )
        )
    {
        if (
            !reply(
                reply_context,
                "SRT APPROVAL FAILED"
            )
            )
        {
            return RICTUS_MODULE_ERR_START_FAILED;
        }

        return RICTUS_MODULE_OK;
    }

    snprintf(
        response,
        sizeof(response),
        "SRT APPROVED | %s -> %s",
        command->arguments,
        srt_id
    );

    if (!reply(reply_context, response))
    {
        return RICTUS_MODULE_ERR_START_FAILED;
    }

    snprintf(
        response,
        sizeof(response),
        "Report: %s",
        report_path
    );

    if (!reply(reply_context, response))
    {
        return RICTUS_MODULE_ERR_START_FAILED;
    }

    printf(
        "[INTELLIGENCE] SRT approved "
        "intelligence_id=%s srt_id=%s reviewer=%s office=%s report=%s\n",
        command->arguments,
        srt_id,
        command->sender,
        office,
        report_path
    );

    return RICTUS_MODULE_OK;
}


/*
 * ------------------------------------------------
 * COMMAND: CHAIN
 * ------------------------------------------------
 *
 * Human-controlled Trust Chain handoff.
 *
 * Preconditions:
 * - argument is an SRT-* identifier
 * - approved SRT report exists
 * - report contains **Status:** Approved
 *
 * This command invokes Chain only. It does not
 * invoke rag_builder and does not copy the report
 * into the RAG input directory.
 */

static int
rictus_intelligence_chain_read_status(
    const char *report_path,
    char *status,
    size_t status_size
)
{
    FILE *fp;
    char line[512];

    if (
        report_path == NULL ||
        status == NULL ||
        status_size == 0
        )
    {
        return 0;
    }

    status[0] = '\0';

    if (
        fopen_s(
            &fp,
            report_path,
            "rb"
        ) != 0 ||
        fp == NULL
        )
    {
        return 0;
    }

    while (
        fgets(
            line,
            sizeof(line),
            fp
        ) != NULL
        )
    {
        const char *prefix =
            "**Status:**";

        char *value;
        char *end;

        if (
            strncmp(
                line,
                prefix,
                strlen(prefix)
            ) != 0
            )
        {
            continue;
        }

        value =
            line +
            strlen(prefix);

        while (
            *value == ' ' ||
            *value == '\t'
            )
        {
            ++value;
        }

        end =
            value +
            strlen(value);

        while (
            end > value &&
            (
                end[-1] == '\r' ||
                end[-1] == '\n' ||
                end[-1] == ' ' ||
                end[-1] == '\t'
            )
            )
        {
            --end;
        }

        *end =
            '\0';

        if (
            value[0] == '\0' ||
            strlen(value) >= status_size
            )
        {
            fclose(fp);

            return 0;
        }

        strcpy_s(
            status,
            status_size,
            value
        );

        fclose(fp);

        return 1;
    }

    fclose(fp);

    return 0;
}


static int
rictus_intelligence_chain_read_sha256(
    const char *report_path,
    char sha256[65]
)
{
    FILE *fp;
    char line[512];

    if (
        report_path == NULL ||
        sha256 == NULL
        )
    {
        return 0;
    }

    sha256[0] = '\0';

    if (
        fopen_s(
            &fp,
            report_path,
            "rb"
        ) != 0 ||
        fp == NULL
        )
    {
        return 0;
    }

    while (
        fgets(
            line,
            sizeof(line),
            fp
        ) != NULL
        )
    {
        const char *prefix =
            "sha256:";

        char *value;
        size_t i;

        if (
            strncmp(
                line,
                prefix,
                strlen(prefix)
            ) != 0
            )
        {
            continue;
        }

        value =
            line +
            strlen(prefix);

        while (
            *value == ' ' ||
            *value == '\t'
            )
        {
            ++value;
        }

        if (
            strlen(value) < 64
            )
        {
            fclose(fp);

            return 0;
        }

        for (
            i = 0;
            i < 64;
            ++i
            )
        {
            char c =
                value[i];

            if (
                !(
                    (c >= '0' && c <= '9') ||
                    (c >= 'a' && c <= 'f') ||
                    (c >= 'A' && c <= 'F')
                )
                )
            {
                fclose(fp);

                return 0;
            }

            sha256[i] =
                c;
        }

        sha256[64] =
            '\0';

        fclose(fp);

        return 1;
    }

    fclose(fp);

    return 0;
}


/*
 * Launches the system-installed Chain utility,
 * captures stdout/stderr into a caller buffer,
 * waits for process termination, and returns the
 * child exit code through exit_code.
 */
static int
rictus_intelligence_chain_execute(
    const char *report_path,
    char *output,
    size_t output_size,
    DWORD *exit_code
)
{
    SECURITY_ATTRIBUTES security_attributes;
    STARTUPINFOA startup_info;
    PROCESS_INFORMATION process_info;

    HANDLE read_pipe =
        NULL;

    HANDLE write_pipe =
        NULL;

    char command_line[
        RICTUS_INTELLIGENCE_SRT_PATH_MAX +
        1024
    ];

    DWORD bytes_read;
    size_t used =
        0;

    int written;
    BOOL process_created;

    if (
        report_path == NULL ||
        output == NULL ||
        output_size < 2 ||
        exit_code == NULL
        )
    {
        return 0;
    }

    output[0] =
        '\0';

    *exit_code =
        (DWORD)-1;

    memset(
        &security_attributes,
        0,
        sizeof(security_attributes)
    );

    security_attributes.nLength =
        sizeof(security_attributes);

    security_attributes.bInheritHandle =
        TRUE;

    if (
        !CreatePipe(
            &read_pipe,
            &write_pipe,
            &security_attributes,
            0
        )
        )
    {
        return 0;
    }

    if (
        !SetHandleInformation(
            read_pipe,
            HANDLE_FLAG_INHERIT,
            0
        )
        )
    {
        CloseHandle(read_pipe);
        CloseHandle(write_pipe);

        return 0;
    }

    written =
        snprintf(
            command_line,
            sizeof(command_line),
            "chain \"%s\" \"%s\"",
            report_path,
            RICTUS_INTELLIGENCE_CHAIN_INDEX_PATH
        );

    if (
        written <= 0 ||
        written >= (int)sizeof(command_line)
        )
    {
        CloseHandle(read_pipe);
        CloseHandle(write_pipe);

        return 0;
    }

    memset(
        &startup_info,
        0,
        sizeof(startup_info)
    );

    startup_info.cb =
        sizeof(startup_info);

    startup_info.dwFlags =
        STARTF_USESTDHANDLES;

    startup_info.hStdOutput =
        write_pipe;

    startup_info.hStdError =
        write_pipe;

    startup_info.hStdInput =
        GetStdHandle(
            STD_INPUT_HANDLE
        );

    memset(
        &process_info,
        0,
        sizeof(process_info)
    );

    process_created =
        CreateProcessA(
            NULL,
            command_line,
            NULL,
            NULL,
            TRUE,
            CREATE_NO_WINDOW,
            NULL,
            NULL,
            &startup_info,
            &process_info
        );

    CloseHandle(
        write_pipe
    );

    write_pipe =
        NULL;

    if (
        !process_created
        )
    {
        CloseHandle(
            read_pipe
        );

        return 0;
    }

    for (;;)
    {
        char buffer[512];

        if (
            !ReadFile(
                read_pipe,
                buffer,
                sizeof(buffer),
                &bytes_read,
                NULL
            ) ||
            bytes_read == 0
            )
        {
            break;
        }

        if (
            used <
            output_size - 1
            )
        {
            size_t available =
                output_size -
                1 -
                used;

            size_t copy_size =
                bytes_read;

            if (
                copy_size >
                available
                )
            {
                copy_size =
                    available;
            }

            memcpy(
                output + used,
                buffer,
                copy_size
            );

            used +=
                copy_size;

            output[used] =
                '\0';
        }
    }

    CloseHandle(
        read_pipe
    );

    WaitForSingleObject(
        process_info.hProcess,
        INFINITE
    );

    if (
        !GetExitCodeProcess(
            process_info.hProcess,
            exit_code
        )
        )
    {
        *exit_code =
            (DWORD)-1;
    }

    CloseHandle(
        process_info.hThread
    );

    CloseHandle(
        process_info.hProcess
    );

    return 1;
}


static rictus_module_result_t
rictus_intelligence_command_chain(
    const rictus_module_command_t *command,
    rictus_module_command_reply_fn reply,
    void *reply_context,
    void *handler_context
)
{
    char report_path[
        RICTUS_INTELLIGENCE_SRT_PATH_MAX
    ];

    char status[64];

    char sha256[65];

    char chain_output[
        RICTUS_INTELLIGENCE_CHAIN_OUTPUT_MAX
    ];

    char response[1200];

    DWORD exit_code;

    int written;

    (void)handler_context;

    if (
        command == NULL ||
        reply == NULL
        )
    {
        return
            RICTUS_MODULE_ERR_INVALID_ARGUMENT;
    }

    if (
        command->arguments[0] == '\0'
        )
    {
        if (
            !reply(
                reply_context,
                "Usage: !chain SRT-YYYYMMDD-NNN"
            )
            )
        {
            return
                RICTUS_MODULE_ERR_START_FAILED;
        }

        return
            RICTUS_MODULE_OK;
    }

    if (
        _strnicmp(
            command->arguments,
            "SRT-",
            4
        ) != 0 ||
        strchr(
            command->arguments,
            '\\'
        ) != NULL ||
        strchr(
            command->arguments,
            '/'
        ) != NULL ||
        strstr(
            command->arguments,
            ".."
        ) != NULL
        )
    {
        if (
            !reply(
                reply_context,
                "CHAIN REFUSED | INVALID SRT ID"
            )
            )
        {
            return
                RICTUS_MODULE_ERR_START_FAILED;
        }

        return
            RICTUS_MODULE_OK;
    }

    written =
        snprintf(
            report_path,
            sizeof(report_path),
            "%s\\%s.srt.md",
            RICTUS_INTELLIGENCE_SRT_DIRECTORY,
            command->arguments
        );

    if (
        written <= 0 ||
        written >= (int)sizeof(report_path)
        )
    {
        if (
            !reply(
                reply_context,
                "CHAIN REFUSED | REPORT PATH INVALID"
            )
            )
        {
            return
                RICTUS_MODULE_ERR_START_FAILED;
        }

        return
            RICTUS_MODULE_OK;
    }

    if (
        GetFileAttributesA(
            report_path
        ) ==
        INVALID_FILE_ATTRIBUTES
        )
    {
        snprintf(
            response,
            sizeof(response),
            "CHAIN REFUSED | %s | REPORT NOT FOUND",
            command->arguments
        );

        if (
            !reply(
                reply_context,
                response
            )
            )
        {
            return
                RICTUS_MODULE_ERR_START_FAILED;
        }

        return
            RICTUS_MODULE_OK;
    }

    if (
        !rictus_intelligence_chain_read_status(
            report_path,
            status,
            sizeof(status)
        )
        )
    {
        if (
            !reply(
                reply_context,
                "CHAIN REFUSED | STATUS MISSING OR INVALID"
            )
            )
        {
            return
                RICTUS_MODULE_ERR_START_FAILED;
        }

        return
            RICTUS_MODULE_OK;
    }

    if (
        _stricmp(
            status,
            "Approved"
        ) != 0
        )
    {
        snprintf(
            response,
            sizeof(response),
            "CHAIN REFUSED | %s | STATUS=%s",
            command->arguments,
            status
        );

        if (
            !reply(
                reply_context,
                response
            )
            )
        {
            return
                RICTUS_MODULE_ERR_START_FAILED;
        }

        return
            RICTUS_MODULE_OK;
    }

    printf(
        "[INTELLIGENCE] Chain requested "
        "srt_id=%s report=%s index=%s\n",
        command->arguments,
        report_path,
        RICTUS_INTELLIGENCE_CHAIN_INDEX_PATH
    );

    if (
        !rictus_intelligence_chain_execute(
            report_path,
            chain_output,
            sizeof(chain_output),
            &exit_code
        )
        )
    {
        if (
            !reply(
                reply_context,
                "CHAIN FAILED | PROCESS LAUNCH FAILED"
            )
            )
        {
            return
                RICTUS_MODULE_ERR_START_FAILED;
        }

        return
            RICTUS_MODULE_OK;
    }

    if (
        exit_code != 0 ||
        strstr(
            chain_output,
            "CHAIN:         PASS"
        ) == NULL
        )
    {
        const char *reason =
            "CHAIN REPORTED FAILURE";

        if (
            strstr(
                chain_output,
                "FAIL_"
            ) != NULL
            )
        {
            char *failure =
                strstr(
                    chain_output,
                    "FAIL_"
                );

            char *end =
                failure;

            while (
                *end != '\0' &&
                *end != '\r' &&
                *end != '\n'
                )
            {
                ++end;
            }

            if (
                (size_t)(end - failure) <
                sizeof(response) - 64
                )
            {
                char failure_text[512];
                size_t failure_length =
                    (size_t)(end - failure);

                memcpy(
                    failure_text,
                    failure,
                    failure_length
                );

                failure_text[failure_length] =
                    '\0';

                snprintf(
                    response,
                    sizeof(response),
                    "CHAIN FAIL | %s | %s",
                    command->arguments,
                    failure_text
                );

                if (
                    !reply(
                        reply_context,
                        response
                    )
                    )
                {
                    return
                        RICTUS_MODULE_ERR_START_FAILED;
                }

                return
                    RICTUS_MODULE_OK;
            }
        }

        snprintf(
            response,
            sizeof(response),
            "CHAIN FAIL | %s | %s | EXIT=%lu",
            command->arguments,
            reason,
            (unsigned long)exit_code
        );

        if (
            !reply(
                reply_context,
                response
            )
            )
        {
            return
                RICTUS_MODULE_ERR_START_FAILED;
        }

        return
            RICTUS_MODULE_OK;
    }

    if (
        !rictus_intelligence_chain_read_sha256(
            report_path,
            sha256
        )
        )
    {
        if (
            !reply(
                reply_context,
                "CHAIN FAIL | PASS REPORTED BUT STAMPED sha256 NOT FOUND"
            )
            )
        {
            return
                RICTUS_MODULE_ERR_START_FAILED;
        }

        return
            RICTUS_MODULE_OK;
    }

    snprintf(
        response,
        sizeof(response),
        "CHAIN PASS | %s",
        command->arguments
    );

    if (
        !reply(
            reply_context,
            response
        )
        )
    {
        return
            RICTUS_MODULE_ERR_START_FAILED;
    }

    snprintf(
        response,
        sizeof(response),
        "sha256: %s",
        sha256
    );

    if (
        !reply(
            reply_context,
            response
        )
        )
    {
        return
            RICTUS_MODULE_ERR_START_FAILED;
    }

    printf(
        "[INTELLIGENCE] Chain PASS "
        "srt_id=%s sha256=%s\n",
        command->arguments,
        sha256
    );

    return
        RICTUS_MODULE_OK;
}


/*
 * ------------------------------------------------
 * RAG OUTPUT RELAY
 * ------------------------------------------------
 *
 * Relays one completed rag_builder stdout/stderr
 * line through the existing command reply path.
 *
 * Empty lines are ignored.
 */
static int
rictus_intelligence_rag_reply_line(
    rictus_module_command_reply_fn reply,
    void *reply_context,
    const char *line
)
{
    char response[
        RICTUS_INTELLIGENCE_IRC_MESSAGE_MAX
    ];

    int written;


    if (
        reply == NULL ||
        line == NULL
        )
    {
        return 0;
    }


    if (
        line[0] == '\0'
        )
    {
        return 1;
    }


    written =
        snprintf(
            response,
            sizeof(response),
            "[RAG] %s",
            line
        );


    if (
        written <= 0 ||
        written >= (int)sizeof(response)
        )
    {
        return 0;
    }


    return
        reply(
            reply_context,
            response
        );
}


/*
 * ------------------------------------------------
 * RAG BUILDER EXECUTION
 * ------------------------------------------------
 *
 * Launches the system-installed rag_builder command
 * with no command-line arguments.
 *
 * rag_builder remains responsible for its own
 * preconfigured C:\stn-labz\rag input/output paths.
 *
 * stdout and stderr are redirected into one pipe
 * and relayed to the operator line-by-line while
 * rag_builder is running.
 *
 * exit_code receives the child process exit code.
 */
static int
rictus_intelligence_rag_execute(
    rictus_module_command_reply_fn reply,
    void *reply_context,
    DWORD *exit_code
)
{
    SECURITY_ATTRIBUTES
        security_attributes;

    STARTUPINFOA
        startup_info;

    PROCESS_INFORMATION
        process_info;

    HANDLE read_pipe =
        NULL;

    HANDLE write_pipe =
        NULL;

    char command_line[] =
        "rag_builder";

    char line[
        RICTUS_INTELLIGENCE_RAG_LINE_MAX
    ];

    size_t line_length =
        0;

    BOOL process_created;

    DWORD bytes_read;


    if (
        reply == NULL ||
        exit_code == NULL
        )
    {
        return 0;
    }


    *exit_code =
        (DWORD)-1;


    memset(
        &security_attributes,
        0,
        sizeof(security_attributes)
    );


    security_attributes.nLength =
        sizeof(security_attributes);

    security_attributes.bInheritHandle =
        TRUE;


    if (
        !CreatePipe(
            &read_pipe,
            &write_pipe,
            &security_attributes,
            0
        )
        )
    {
        return 0;
    }


    if (
        !SetHandleInformation(
            read_pipe,
            HANDLE_FLAG_INHERIT,
            0
        )
        )
    {
        CloseHandle(
            read_pipe
        );

        CloseHandle(
            write_pipe
        );

        return 0;
    }


    memset(
        &startup_info,
        0,
        sizeof(startup_info)
    );


    startup_info.cb =
        sizeof(startup_info);

    startup_info.dwFlags =
        STARTF_USESTDHANDLES;

    startup_info.hStdOutput =
        write_pipe;

    startup_info.hStdError =
        write_pipe;

    startup_info.hStdInput =
        GetStdHandle(
            STD_INPUT_HANDLE
        );


    memset(
        &process_info,
        0,
        sizeof(process_info)
    );


    process_created =
        CreateProcessA(
            NULL,
            command_line,
            NULL,
            NULL,
            TRUE,
            CREATE_NO_WINDOW,
            NULL,
            NULL,
            &startup_info,
            &process_info
        );


    CloseHandle(
        write_pipe
    );

    write_pipe =
        NULL;


    if (
        !process_created
        )
    {
        CloseHandle(
            read_pipe
        );

        return 0;
    }


    for (;;)
    {
        char buffer[256];
        DWORD index;


        if (
            !ReadFile(
                read_pipe,
                buffer,
                sizeof(buffer),
                &bytes_read,
                NULL
            ) ||
            bytes_read == 0
            )
        {
            break;
        }


        for (
            index = 0;
            index < bytes_read;
            ++index
            )
        {
            char c =
                buffer[index];


            if (
                c == '\r'
                )
            {
                continue;
            }


            if (
                c == '\n'
                )
            {
                line[
                    line_length
                ] =
                    '\0';


                if (
                    !rictus_intelligence_rag_reply_line(
                        reply,
                        reply_context,
                        line
                    )
                    )
                {
                    CloseHandle(
                        read_pipe
                    );

                    TerminateProcess(
                        process_info.hProcess,
                        1
                    );

                    WaitForSingleObject(
                        process_info.hProcess,
                        INFINITE
                    );

                    CloseHandle(
                        process_info.hThread
                    );

                    CloseHandle(
                        process_info.hProcess
                    );

                    return 0;
                }


                line_length =
                    0;

                continue;
            }


            if (
                line_length >=
                sizeof(line) - 1
                )
            {
                line[
                    line_length
                ] =
                    '\0';


                if (
                    !rictus_intelligence_rag_reply_line(
                        reply,
                        reply_context,
                        line
                    )
                    )
                {
                    CloseHandle(
                        read_pipe
                    );

                    TerminateProcess(
                        process_info.hProcess,
                        1
                    );

                    WaitForSingleObject(
                        process_info.hProcess,
                        INFINITE
                    );

                    CloseHandle(
                        process_info.hThread
                    );

                    CloseHandle(
                        process_info.hProcess
                    );

                    return 0;
                }


                line_length =
                    0;
            }


            line[
                line_length++
            ] =
                c;
        }
    }


    CloseHandle(
        read_pipe
    );


    if (
        line_length > 0
        )
    {
        line[
            line_length
        ] =
            '\0';


        if (
            !rictus_intelligence_rag_reply_line(
                reply,
                reply_context,
                line
            )
            )
        {
            TerminateProcess(
                process_info.hProcess,
                1
            );

            WaitForSingleObject(
                process_info.hProcess,
                INFINITE
            );

            CloseHandle(
                process_info.hThread
            );

            CloseHandle(
                process_info.hProcess
            );

            return 0;
        }
    }


    WaitForSingleObject(
        process_info.hProcess,
        INFINITE
    );


    if (
        !GetExitCodeProcess(
            process_info.hProcess,
            exit_code
        )
        )
    {
        *exit_code =
            (DWORD)-1;
    }


    CloseHandle(
        process_info.hThread
    );

    CloseHandle(
        process_info.hProcess
    );


    return 1;
}


/*
 * ------------------------------------------------
 * COMMAND: RAG
 * ------------------------------------------------
 *
 * Operator-directed rag_builder handoff.
 *
 * Operation:
 * - accepts one SRT identifier;
 * - resolves the approved SRT Markdown report;
 * - copies it to C:\stn-labz\rag\input;
 * - invokes the system-installed rag_builder;
 * - relays rag_builder stdout/stderr in real time;
 * - reports PASS or FAIL from the process exit code.
 *
 * This command does not invoke Chain, change
 * rag_builder configuration, or copy corpus output
 * into Digit's corpus directory.
 */
static rictus_module_result_t
rictus_intelligence_command_rag(
    const rictus_module_command_t *command,
    rictus_module_command_reply_fn reply,
    void *reply_context,
    void *handler_context
)
{
    char source_path[
        RICTUS_INTELLIGENCE_SRT_PATH_MAX
    ];

    char destination_path[
        RICTUS_INTELLIGENCE_SRT_PATH_MAX
    ];

    char response[
        1200
    ];

    DWORD attributes;
    DWORD exit_code;
    int written;


    (void)handler_context;


    if (
        command == NULL ||
        reply == NULL
        )
    {
        return
            RICTUS_MODULE_ERR_INVALID_ARGUMENT;
    }


    if (
        command->arguments[0] == '\0'
        )
    {
        if (
            !reply(
                reply_context,
                "Usage: !rag SRT-YYYYMMDD-NNN"
            )
            )
        {
            return
                RICTUS_MODULE_ERR_START_FAILED;
        }


        return
            RICTUS_MODULE_OK;
    }


    if (
        _strnicmp(
            command->arguments,
            "SRT-",
            4
        ) != 0 ||
        strchr(
            command->arguments,
            '\\'
        ) != NULL ||
        strchr(
            command->arguments,
            '/'
        ) != NULL ||
        strstr(
            command->arguments,
            ".."
        ) != NULL
        )
    {
        if (
            !reply(
                reply_context,
                "RAG REFUSED | INVALID SRT ID"
            )
            )
        {
            return
                RICTUS_MODULE_ERR_START_FAILED;
        }


        return
            RICTUS_MODULE_OK;
    }


    written =
        snprintf(
            source_path,
            sizeof(source_path),
            "%s\\%s.srt.md",
            RICTUS_INTELLIGENCE_SRT_DIRECTORY,
            command->arguments
        );


    if (
        written <= 0 ||
        written >= (int)sizeof(source_path)
        )
    {
        if (
            !reply(
                reply_context,
                "RAG REFUSED | SOURCE PATH INVALID"
            )
            )
        {
            return
                RICTUS_MODULE_ERR_START_FAILED;
        }


        return
            RICTUS_MODULE_OK;
    }


    attributes =
        GetFileAttributesA(
            source_path
        );


    if (
        attributes ==
            INVALID_FILE_ATTRIBUTES ||
        (
            attributes &
            FILE_ATTRIBUTE_DIRECTORY
        ) != 0
        )
    {
        snprintf(
            response,
            sizeof(response),
            "RAG REFUSED | %s | SRT NOT FOUND",
            command->arguments
        );


        if (
            !reply(
                reply_context,
                response
            )
            )
        {
            return
                RICTUS_MODULE_ERR_START_FAILED;
        }


        return
            RICTUS_MODULE_OK;
    }


    attributes =
        GetFileAttributesA(
            RICTUS_INTELLIGENCE_RAG_INPUT_DIRECTORY
        );


    if (
        attributes ==
            INVALID_FILE_ATTRIBUTES ||
        (
            attributes &
            FILE_ATTRIBUTE_DIRECTORY
        ) == 0
        )
    {
        if (
            !reply(
                reply_context,
                "RAG FAILED | INPUT DIRECTORY NOT FOUND"
            )
            )
        {
            return
                RICTUS_MODULE_ERR_START_FAILED;
        }


        return
            RICTUS_MODULE_OK;
    }


    written =
        snprintf(
            destination_path,
            sizeof(destination_path),
            "%s\\%s.srt.md",
            RICTUS_INTELLIGENCE_RAG_INPUT_DIRECTORY,
            command->arguments
        );


    if (
        written <= 0 ||
        written >=
            (int)sizeof(destination_path)
        )
    {
        if (
            !reply(
                reply_context,
                "RAG FAILED | DESTINATION PATH INVALID"
            )
            )
        {
            return
                RICTUS_MODULE_ERR_START_FAILED;
        }


        return
            RICTUS_MODULE_OK;
    }


    if (
        !CopyFileA(
            source_path,
            destination_path,
            FALSE
        )
        )
    {
        snprintf(
            response,
            sizeof(response),
            "RAG FAILED | %s | COPY FAILED | WIN32=%lu",
            command->arguments,
            (unsigned long)GetLastError()
        );


        if (
            !reply(
                reply_context,
                response
            )
            )
        {
            return
                RICTUS_MODULE_ERR_START_FAILED;
        }


        return
            RICTUS_MODULE_OK;
    }


    snprintf(
        response,
        sizeof(response),
        "RAG STAGED | %s",
        command->arguments
    );


    if (
        !reply(
            reply_context,
            response
        )
        )
    {
        return
            RICTUS_MODULE_ERR_START_FAILED;
    }


    printf(
        "[INTELLIGENCE] RAG requested "
        "srt_id=%s source=%s destination=%s\n",
        command->arguments,
        source_path,
        destination_path
    );


    if (
        !rictus_intelligence_rag_execute(
            reply,
            reply_context,
            &exit_code
        )
        )
    {
        if (
            !reply(
                reply_context,
                "RAG FAILED | PROCESS EXECUTION FAILED"
            )
            )
        {
            return
                RICTUS_MODULE_ERR_START_FAILED;
        }


        return
            RICTUS_MODULE_OK;
    }


    if (
        exit_code != 0
        )
    {
        snprintf(
            response,
            sizeof(response),
            "RAG FAIL | %s | EXIT=%lu",
            command->arguments,
            (unsigned long)exit_code
        );


        if (
            !reply(
                reply_context,
                response
            )
            )
        {
            return
                RICTUS_MODULE_ERR_START_FAILED;
        }


        printf(
            "[INTELLIGENCE] RAG FAIL "
            "srt_id=%s exit=%lu\n",
            command->arguments,
            (unsigned long)exit_code
        );


        return
            RICTUS_MODULE_OK;
    }


    snprintf(
        response,
        sizeof(response),
        "RAG PASS | %s",
        command->arguments
    );


    if (
        !reply(
            reply_context,
            response
        )
        )
    {
        return
            RICTUS_MODULE_ERR_START_FAILED;
    }


    printf(
        "[INTELLIGENCE] RAG PASS "
        "srt_id=%s\n",
        command->arguments
    );


    return
        RICTUS_MODULE_OK;
}


/*
 * ------------------------------------------------
 * PUBLISH NEW SOURCE EVIDENCE
 * ------------------------------------------------
 */

static void
rictus_intelligence_publish_item(
    const rictus_intelligence_item_t* item
)
{
    char message[
        RICTUS_INTELLIGENCE_IRC_MESSAGE_MAX
    ];

    char id[
        RICTUS_INTELLIGENCE_RECORD_ID_MAX
    ];

    int length;

    if (
        item == NULL ||
        g_intelligence_host == NULL ||
        g_intelligence_host->send_message == NULL
        )
    {
        return;
    }

    if (
        !rictus_intelligence_record_store_append(
            &g_intelligence_records,
            g_intelligence_record_path,
            item,
            id,
            sizeof(id)
        )
        )
    {
        printf(
            "[INTELLIGENCE] Record persistence FAILED "
            "fingerprint=%s\n",
            item->fingerprint
        );

        return;
    }

    length =
        snprintf(
            message,
            sizeof(message),
            "[Rictus Intel] Hey, I found one thing I think you should inspect. %s",
            id
        );

    if (
        length <= 0 ||
        length >= (int)sizeof(message)
        )
    {
        return;
    }

    if (!g_intelligence_host->send_message(message))
    {
        printf(
            "[INTELLIGENCE] IRC publication FAILED id=%s\n",
            id
        );

        return;
    }

    printf(
        "[INTELLIGENCE] IRC publication PASS id=%s fingerprint=%s\n",
        id,
        item->fingerprint
    );
}


/*
 * ------------------------------------------------
 * PROCESS NORMALIZED ITEM
 * ------------------------------------------------
 */

static void
rictus_intelligence_process_item(
    const rictus_intelligence_item_t* item
)
{
    rictus_intelligence_seen_result_t
        seen_result;


    if (
        item == NULL
        )
    {
        return;
    }


    if (
        rictus_intelligence_seen_contains(
            &g_intelligence_seen,
            item->fingerprint
        )
        )
    {
        printf(
            "[INTELLIGENCE] Known item "
            "source=%s fingerprint=%s\n",
            item->source,
            item->fingerprint
        );


        return;
    }


    printf(
        "[INTELLIGENCE] NEW ITEM "
        "source=%s "
        "fingerprint=%s\n",
        item->source,
        item->fingerprint
    );


    printf(
        "[INTELLIGENCE] TITLE: %s\n",
        item->title
    );


    if (
        item->published[0] != '\0'
        )
    {
        printf(
            "[INTELLIGENCE] PUBLISHED: %s\n",
            item->published
        );
    }


    if (
        item->url[0] != '\0'
        )
    {
        printf(
            "[INTELLIGENCE] URL: %s\n",
            item->url
        );
    }


    seen_result =
        rictus_intelligence_seen_add(
            &g_intelligence_seen,
            item->fingerprint
        );


    if (
        seen_result !=
        RICTUS_INTELLIGENCE_SEEN_OK
        )
    {
        printf(
            "[INTELLIGENCE] Seen-state failure "
            "fingerprint=%s result=%s\n",
            item->fingerprint,
            rictus_intelligence_seen_result_string(
                seen_result
            )
        );


        return;
    }


    rictus_intelligence_publish_item(
        item
    );
}


/*
 * ------------------------------------------------
 * COLLECTION CYCLE
 * ------------------------------------------------
 */

static void
rictus_intelligence_collect_cycle(void)
{
    size_t source_count;

    size_t source_index;

    rictus_intelligence_item_set_t
        *items;


    items =
        (rictus_intelligence_item_set_t *)
        malloc(
            sizeof(*items)
        );


    if (
        items == NULL
        )
    {
        printf(
            "[INTELLIGENCE] Collection cycle failed: "
            "item-set allocation failed.\n"
        );


        return;
    }


    source_count =
        rictus_intelligence_source_count();


    printf(
        "[INTELLIGENCE] Collection cycle starting. "
        "sources=%u\n",
        (unsigned int)
        source_count
    );


    for (
        source_index = 0;
        source_index < source_count;
        ++source_index
        )
    {
        const
            rictus_intelligence_source_definition_t
            * source;

        rictus_intelligence_response_t
            response;

        rictus_intelligence_collect_result_t
            collect_result;

        rictus_intelligence_parse_result_t
            parse_result;

        size_t item_index;


        if (
            WaitForSingleObject(
                g_intelligence_stop_event,
                0
            ) == WAIT_OBJECT_0
            )
        {
            free(
                items
            );


            return;
        }


        source =
            rictus_intelligence_source_get(
                source_index
            );


        if (
            source == NULL
            )
        {
            continue;
        }


        memset(
            &response,
            0,
            sizeof(response)
        );


        memset(
            items,
            0,
            sizeof(*items)
        );


        printf(
            "[INTELLIGENCE] Collecting "
            "source=%s id=%s\n",
            source->name,
            source->id
        );


        collect_result =
            rictus_intelligence_collect(
                source,
                &response
            );


        if (
            collect_result !=
            RICTUS_INTELLIGENCE_COLLECT_OK
            )
        {
            printf(
                "[INTELLIGENCE] Collection failed "
                "source=%s result=%s http=%lu\n",
                source->name,
                rictus_intelligence_collect_result_string(
                    collect_result
                ),
                response.http_status
            );


            rictus_intelligence_response_free(
                &response
            );


            continue;
        }


        printf(
            "[INTELLIGENCE] Collection complete "
            "source=%s http=%lu bytes=%u\n",
            source->name,
            response.http_status,
            (unsigned int)
            response.body_length
        );


        parse_result =
            rictus_intelligence_parse_response(
                source,
                &response,
                items
            );


        if (
            parse_result !=
            RICTUS_INTELLIGENCE_PARSE_OK &&
            parse_result !=
            RICTUS_INTELLIGENCE_PARSE_ITEM_LIMIT
            )
        {
            printf(
                "[INTELLIGENCE] Parse failed "
                "source=%s result=%s\n",
                source->name,
                rictus_intelligence_parse_result_string(
                    parse_result
                )
            );


            rictus_intelligence_response_free(
                &response
            );


            continue;
        }


        printf(
            "[INTELLIGENCE] Normalized "
            "source=%s items=%u\n",
            source->name,
            (unsigned int)
            items->count
        );


        rictus_intelligence_response_free(
            &response
        );


        for (
            item_index = 0;
            item_index < items->count;
            ++item_index
            )
        {
            if (
                WaitForSingleObject(
                    g_intelligence_stop_event,
                    0
                ) == WAIT_OBJECT_0
                )
            {
                free(
                    items
                );


                return;
            }


            rictus_intelligence_process_item(
                &items->items[
                    item_index
                ]
            );
        }
    }


    printf(
        "[INTELLIGENCE] Collection cycle complete.\n"
    );


    free(
        items
    );
}


/*
 * ------------------------------------------------
 * WORKER
 * ------------------------------------------------
 */

static DWORD WINAPI
rictus_intelligence_worker(
    LPVOID parameter
)
{
    DWORD wait_result;


    (void)parameter;


    printf(
        "[INTELLIGENCE] Worker started.\n"
    );


    for (;;)
    {
        if (
            WaitForSingleObject(
                g_intelligence_stop_event,
                0
            ) == WAIT_OBJECT_0
            )
        {
            break;
        }


        rictus_intelligence_collect_cycle();


        wait_result =
            WaitForSingleObject(
                g_intelligence_stop_event,
                RICTUS_INTELLIGENCE_COLLECTION_INTERVAL_MS
            );


        if (
            wait_result ==
            WAIT_OBJECT_0
            )
        {
            break;
        }


        if (
            wait_result !=
            WAIT_TIMEOUT
            )
        {
            break;
        }
    }


    printf(
        "[INTELLIGENCE] Worker stopped.\n"
    );


    InterlockedExchange(
        &g_intelligence_running,
        0
    );


    return 0;
}


/*
 * ------------------------------------------------
 * START
 * ------------------------------------------------
 */

static rictus_module_result_t
rictus_intelligence_start(
    const rictus_module_host_t* host
)
{
    HANDLE stop_event;

    HANDLE thread;

    rictus_intelligence_seen_result_t
        seen_result;


    if (
        host == NULL ||
        host->send_message == NULL ||
        host->register_command == NULL ||
        host->unregister_command == NULL
        )
    {
        return
            RICTUS_MODULE_ERR_INVALID_ARGUMENT;
    }


    if (
        InterlockedCompareExchange(
            &g_intelligence_running,
            0,
            0
        ) != 0
        )
    {
        return
            RICTUS_MODULE_ERR_INVALID_STATE;
    }


    if (
        g_intelligence_thread != NULL ||
        g_intelligence_stop_event != NULL
        )
    {
        return
            RICTUS_MODULE_ERR_INVALID_STATE;
    }


    g_intelligence_host =
        host;


    rictus_intelligence_seen_init(
        &g_intelligence_seen
    );


    seen_result =
        rictus_intelligence_seen_load(
            &g_intelligence_seen
        );


    if (
        seen_result !=
        RICTUS_INTELLIGENCE_SEEN_OK
        )
    {
        printf(
            "[INTELLIGENCE] Seen-state load failed "
            "result=%s\n",
            rictus_intelligence_seen_result_string(
                seen_result
            )
        );


        g_intelligence_host =
            NULL;


        return
            RICTUS_MODULE_ERR_START_FAILED;
    }


    printf(
        "[INTELLIGENCE] Seen-state loaded "
        "records=%u\n",
        (unsigned int)
        g_intelligence_seen.count
    );


    rictus_intelligence_record_store_init(
        &g_intelligence_records
    );


    if (
        !rictus_intelligence_record_store_load(
            &g_intelligence_records,
            g_intelligence_record_path
        )
        )
    {
        printf(
            "[INTELLIGENCE] Record-store load failed: %s\n",
            g_intelligence_record_path
        );


        g_intelligence_host =
            NULL;


        return
            RICTUS_MODULE_ERR_START_FAILED;
    }


    printf(
        "[INTELLIGENCE] Record-store loaded records=%u\n",
        (unsigned int)
        g_intelligence_records.count
    );


    rictus_intelligence_srt_store_init(
        &g_intelligence_srt_requests
    );


    if (
        !rictus_intelligence_srt_store_load(
            &g_intelligence_srt_requests,
            RICTUS_INTELLIGENCE_SRT_REQUEST_PATH
        )
        )
    {
        printf(
            "[INTELLIGENCE] SRT request-store load failed: %s\n",
            RICTUS_INTELLIGENCE_SRT_REQUEST_PATH
        );


        g_intelligence_host =
            NULL;


        return
            RICTUS_MODULE_ERR_START_FAILED;
    }


    printf(
        "[INTELLIGENCE] SRT request-store loaded records=%u\n",
        (unsigned int)
        g_intelligence_srt_requests.count
    );


    if (
        !g_intelligence_host->register_command(
            "show",
            rictus_intelligence_command_show,
            NULL
        )
        )
    {
        printf(
            "[INTELLIGENCE] Command registration failed: show\n"
        );


        g_intelligence_host =
            NULL;


        return
            RICTUS_MODULE_ERR_START_FAILED;
    }


    printf(
        "[INTELLIGENCE] Command registered: show\n"
    );


    if (
        !g_intelligence_host->register_command(
            "srt",
            rictus_intelligence_command_srt,
            NULL
        )
        )
    {
        printf(
            "[INTELLIGENCE] Command registration failed: srt\n"
        );


        (void)
            g_intelligence_host->unregister_command(
                "srt",
                NULL
            );


        (void)
            g_intelligence_host->unregister_command(
                "show",
                NULL
            );


        g_intelligence_host =
            NULL;


        return
            RICTUS_MODULE_ERR_START_FAILED;
    }


    printf(
        "[INTELLIGENCE] Command registered: srt\n"
    );


    if (
        !g_intelligence_host->register_command(
            "approve",
            rictus_intelligence_command_approve,
            NULL
        )
        )
    {
        printf(
            "[INTELLIGENCE] Command registration failed: approve\n"
        );

        (void)
            g_intelligence_host->unregister_command(
                "srt",
                NULL
            );

        (void)
            g_intelligence_host->unregister_command(
                "show",
                NULL
            );

        g_intelligence_host =
            NULL;

        return
            RICTUS_MODULE_ERR_START_FAILED;
    }


    printf(
        "[INTELLIGENCE] Command registered: approve\n"
    );


    if (
        !g_intelligence_host->register_command(
            "chain",
            rictus_intelligence_command_chain,
            NULL
        )
        )
    {
        printf(
            "[INTELLIGENCE] Command registration failed: chain\n"
        );

        (void)
            g_intelligence_host->unregister_command(
                "chain",
                NULL
            );

        (void)
            g_intelligence_host->unregister_command(
                "approve",
                NULL
            );

        (void)
            g_intelligence_host->unregister_command(
                "srt",
                NULL
            );

        (void)
            g_intelligence_host->unregister_command(
                "show",
                NULL
            );

        g_intelligence_host =
            NULL;

        return
            RICTUS_MODULE_ERR_START_FAILED;
    }


    printf(
        "[INTELLIGENCE] Command registered: chain\n"
    );


    if (
        !g_intelligence_host->register_command(
            "rag",
            rictus_intelligence_command_rag,
            NULL
        )
        )
    {
        printf(
            "[INTELLIGENCE] Command registration failed: rag\n"
        );

        (void)
            g_intelligence_host->unregister_command(
                "chain",
                NULL
            );

        (void)
            g_intelligence_host->unregister_command(
                "approve",
                NULL
            );

        (void)
            g_intelligence_host->unregister_command(
                "srt",
                NULL
            );

        (void)
            g_intelligence_host->unregister_command(
                "show",
                NULL
            );

        g_intelligence_host =
            NULL;

        return
            RICTUS_MODULE_ERR_START_FAILED;
    }


    printf(
        "[INTELLIGENCE] Command registered: rag\n"
    );


    stop_event =
        CreateEventA(
            NULL,
            TRUE,
            FALSE,
            NULL
        );


    if (
        stop_event == NULL
        )
    {
        (void)
            g_intelligence_host->unregister_command(
                "rag",
                NULL
            );

        (void)
            g_intelligence_host->unregister_command(
                "chain",
                NULL
            );

        (void)
            g_intelligence_host->unregister_command(
                "approve",
                NULL
            );

        (void)
            g_intelligence_host->unregister_command(
                "srt",
                NULL
            );


        (void)
            g_intelligence_host->unregister_command(
                "show",
                NULL
            );


        g_intelligence_host =
            NULL;


        return
            RICTUS_MODULE_ERR_START_FAILED;
    }


    g_intelligence_stop_event =
        stop_event;


    thread =
        CreateThread(
            NULL,
            0,
            rictus_intelligence_worker,
            NULL,
            0,
            NULL
        );


    if (
        thread == NULL
        )
    {
        CloseHandle(
            g_intelligence_stop_event
        );


        g_intelligence_stop_event =
            NULL;


        (void)
            g_intelligence_host->unregister_command(
                "rag",
                NULL
            );

        (void)
            g_intelligence_host->unregister_command(
                "chain",
                NULL
            );

        (void)
            g_intelligence_host->unregister_command(
                "approve",
                NULL
            );

        (void)
            g_intelligence_host->unregister_command(
                "srt",
                NULL
            );

        (void)
            g_intelligence_host->unregister_command(
                "show",
                NULL
            );


        g_intelligence_host =
            NULL;


        return
            RICTUS_MODULE_ERR_START_FAILED;
    }


    g_intelligence_thread =
        thread;


    InterlockedExchange(
        &g_intelligence_running,
        1
    );


    printf(
        "[INTELLIGENCE] Module ACTIVE.\n"
    );


    return
        RICTUS_MODULE_OK;
}


/*
 * ------------------------------------------------
 * STOP
 * ------------------------------------------------
 */

static rictus_module_result_t
rictus_intelligence_stop(void)
{
    DWORD wait_result;


    if (
        g_intelligence_thread == NULL ||
        g_intelligence_stop_event == NULL
        )
    {
        return
            RICTUS_MODULE_ERR_INVALID_STATE;
    }


    printf(
        "[INTELLIGENCE] Stop requested.\n"
    );


    if (
        !SetEvent(
            g_intelligence_stop_event
        )
        )
    {
        return
            RICTUS_MODULE_ERR_STOP_FAILED;
    }


    wait_result =
        WaitForSingleObject(
            g_intelligence_thread,
            INFINITE
        );


    if (
        wait_result !=
        WAIT_OBJECT_0
        )
    {
        return
            RICTUS_MODULE_ERR_STOP_FAILED;
    }


    CloseHandle(
        g_intelligence_thread
    );


    g_intelligence_thread =
        NULL;


    CloseHandle(
        g_intelligence_stop_event
    );


    g_intelligence_stop_event =
        NULL;


    if (
        g_intelligence_host == NULL ||
        g_intelligence_host->unregister_command == NULL ||
        !g_intelligence_host->unregister_command(
            "rag",
            NULL
        )
        )
    {
        return
            RICTUS_MODULE_ERR_STOP_FAILED;
    }


    printf(
        "[INTELLIGENCE] Command unregistered: rag\n"
    );


    if (
        !g_intelligence_host->unregister_command(
            "chain",
            NULL
        )
        )
    {
        return
            RICTUS_MODULE_ERR_STOP_FAILED;
    }


    printf(
        "[INTELLIGENCE] Command unregistered: chain\n"
    );


    if (
        !g_intelligence_host->unregister_command(
            "approve",
            NULL
        )
        )
    {
        return
            RICTUS_MODULE_ERR_STOP_FAILED;
    }


    printf(
        "[INTELLIGENCE] Command unregistered: approve\n"
    );


    if (
        !g_intelligence_host->unregister_command(
            "srt",
            NULL
        )
        )
    {
        return
            RICTUS_MODULE_ERR_STOP_FAILED;
    }


    printf(
        "[INTELLIGENCE] Command unregistered: srt\n"
    );


    if (
        !g_intelligence_host->unregister_command(
            "show",
            NULL
        )
        )
    {
        return
            RICTUS_MODULE_ERR_STOP_FAILED;
    }


    printf(
        "[INTELLIGENCE] Command unregistered: show\n"
    );


    g_intelligence_host =
        NULL;


    InterlockedExchange(
        &g_intelligence_running,
        0
    );


    printf(
        "[INTELLIGENCE] Module stopped cleanly.\n"
    );


    return
        RICTUS_MODULE_OK;
}


/*
 * ------------------------------------------------
 * QUALIFICATION
 * ------------------------------------------------
 */

static rictus_module_result_t
rictus_intelligence_qualify(
    rictus_module_qualification_result_t* result
)
{
    unsigned int executed =
        0;

    unsigned int passed =
        0;

    unsigned int failed =
        0;

    int negative_executed =
        0;

    int negative_passed =
        0;


    if (
        result == NULL
        )
    {
        return
            RICTUS_MODULE_ERR_INVALID_ARGUMENT;
    }


    memset(
        result,
        0,
        sizeof(*result)
    );


#define RICTUS_TEST(CONDITION) \
    do \
    { \
        ++executed; \
        if (CONDITION) \
        { \
            ++passed; \
        } \
        else \
        { \
            ++failed; \
        } \
    } \
    while (0)


    RICTUS_TEST(
        strcmp(
            rictus_intelligence_descriptor.id,
            RICTUS_INTELLIGENCE_ID
        ) == 0
    );


    RICTUS_TEST(
        strcmp(
            rictus_intelligence_descriptor.name,
            RICTUS_INTELLIGENCE_NAME
        ) == 0
    );


    RICTUS_TEST(
        rictus_intelligence_descriptor.version_major ==
        RICTUS_INTELLIGENCE_VERSION_MAJOR &&
        rictus_intelligence_descriptor.version_minor ==
        RICTUS_INTELLIGENCE_VERSION_MINOR &&
        rictus_intelligence_descriptor.version_patch ==
        RICTUS_INTELLIGENCE_VERSION_PATCH
    );


    RICTUS_TEST(
        rictus_intelligence_descriptor
        .required_core_api_major ==
        RICTUS_MODULE_API_MAJOR &&
        rictus_intelligence_descriptor
        .required_core_api_minor <=
        RICTUS_MODULE_API_MINOR
    );


    RICTUS_TEST(
        rictus_intelligence_descriptor.qualify ==
        rictus_intelligence_qualify &&
        rictus_intelligence_descriptor.start ==
        rictus_intelligence_start &&
        rictus_intelligence_descriptor.stop ==
        rictus_intelligence_stop
    );


    RICTUS_TEST(
        rictus_intelligence_source_from_name(
            "NASA"
        ) ==
        RICTUS_INTELLIGENCE_SOURCE_NASA &&
        rictus_intelligence_source_class(
            RICTUS_INTELLIGENCE_SOURCE_NASA
        ) ==
        RICTUS_INTELLIGENCE_SOURCE_PRIMARY
    );


    RICTUS_TEST(
        rictus_intelligence_source_from_name(
            "SpaceX"
        ) ==
        RICTUS_INTELLIGENCE_SOURCE_SPACEX &&
        rictus_intelligence_source_class(
            RICTUS_INTELLIGENCE_SOURCE_SPACEX
        ) ==
        RICTUS_INTELLIGENCE_SOURCE_PRIMARY
    );


    RICTUS_TEST(
        rictus_intelligence_source_from_name(
            "Unknown Source"
        ) ==
        RICTUS_INTELLIGENCE_SOURCE_OTHER
    );


    RICTUS_TEST(
        rictus_intelligence_source_from_name(
            NULL
        ) ==
        RICTUS_INTELLIGENCE_SOURCE_NONE &&
        rictus_intelligence_source_from_name(
            ""
        ) ==
        RICTUS_INTELLIGENCE_SOURCE_NONE
    );


    ++executed;


    negative_executed =
        1;


    if (
        rictus_intelligence_source_from_name(
            "NASA News"
        ) ==
        RICTUS_INTELLIGENCE_SOURCE_OTHER &&
        rictus_intelligence_source_from_name(
            "SpaceX Updates"
        ) ==
        RICTUS_INTELLIGENCE_SOURCE_OTHER
        )
    {
        negative_passed =
            1;

        ++passed;
    }
    else
    {
        ++failed;
    }


#undef RICTUS_TEST


    result->tests_executed =
        executed;

    result->tests_passed =
        passed;

    result->tests_failed =
        failed;

    result->negative_test_executed =
        negative_executed;

    result->negative_test_passed =
        negative_passed;


    if (
        executed <
        RICTUS_MODULE_MIN_TESTS ||
        passed !=
        executed ||
        failed != 0 ||
        !negative_executed ||
        !negative_passed
        )
    {
        return
            RICTUS_MODULE_ERR_QUALIFICATION;
    }


    return
        RICTUS_MODULE_OK;
}


/*
 * ------------------------------------------------
 * DESCRIPTOR
 * ------------------------------------------------
 */

const rictus_module_descriptor_t
rictus_intelligence_descriptor =
{
    RICTUS_INTELLIGENCE_ID,

    RICTUS_INTELLIGENCE_NAME,

    RICTUS_INTELLIGENCE_VERSION_MAJOR,

    RICTUS_INTELLIGENCE_VERSION_MINOR,

    RICTUS_INTELLIGENCE_VERSION_PATCH,

    RICTUS_MODULE_API_MAJOR,

    RICTUS_MODULE_API_MINOR,

    rictus_intelligence_qualify,

    rictus_intelligence_start,

    rictus_intelligence_stop
};


/*
 * ------------------------------------------------
 * DLL ABI
 * ------------------------------------------------
 */

__declspec(dllexport)
const rictus_module_descriptor_t*
rictus_module_get_descriptor(void)
{
    return
        &rictus_intelligence_descriptor;
}
