/*
 * STN-LABZ
 * Rictus Intelligence Module
 *
 * intelligence.c
 *
 * Intelligence module revision 0.8.0.
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
 * This revision proves Core-to-module command
 * dispatch. Persistent INT record lookup is added
 * in the next Intelligence revision.
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


    /*
     * New source evidence.
     *
     * This is not yet an approved intelligence
     * briefing. It is a normalized new input.
     */

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


    /*
     * Persist first.
     *
     * If persistence fails, the item is not treated
     * as successfully accepted and is not published.
     */

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


    /*
     * Publication occurs only after successful
     * persistent deduplication state update.
     */

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

        rictus_intelligence_item_set_t
            items;

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
            &items,
            0,
            sizeof(items)
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


        /*
         * ------------------------------------------------
         * NORMALIZATION
         * ------------------------------------------------
         */

        parse_result =
            rictus_intelligence_parse_response(
                source,
                &response,
                &items
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
            items.count
        );


        rictus_intelligence_response_free(
            &response
        );


        /*
         * ------------------------------------------------
         * DEDUPLICATION / PUBLICATION
         * ------------------------------------------------
         */

        for (
            item_index = 0;
            item_index < items.count;
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
                return;
            }


            rictus_intelligence_process_item(
                &items.items[
                    item_index
                ]
            );
        }
    }


    printf(
        "[INTELLIGENCE] Collection cycle complete.\n"
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


    /*
     * Intelligence requires the Core publication
     * service for this revision.
     */

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


    /*
     * Store Core host services before the worker
     * can begin.
     */

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


    /*
     * Worker is gone. Remove module-owned commands
     * before releasing the Core host reference.
     */

    if (
        g_intelligence_host == NULL ||
        g_intelligence_host->unregister_command == NULL ||
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


    /*
     * M01 identity
     */

    RICTUS_TEST(
        strcmp(
            rictus_intelligence_descriptor.id,
            RICTUS_INTELLIGENCE_ID
        ) == 0
    );


    /*
     * M02 name
     */

    RICTUS_TEST(
        strcmp(
            rictus_intelligence_descriptor.name,
            RICTUS_INTELLIGENCE_NAME
        ) == 0
    );


    /*
     * M03 version
     */

    RICTUS_TEST(
        rictus_intelligence_descriptor.version_major ==
        RICTUS_INTELLIGENCE_VERSION_MAJOR &&
        rictus_intelligence_descriptor.version_minor ==
        RICTUS_INTELLIGENCE_VERSION_MINOR &&
        rictus_intelligence_descriptor.version_patch ==
        RICTUS_INTELLIGENCE_VERSION_PATCH
    );


    /*
     * M04 API
     */

    RICTUS_TEST(
        rictus_intelligence_descriptor
        .required_core_api_major ==
        RICTUS_MODULE_API_MAJOR &&
        rictus_intelligence_descriptor
        .required_core_api_minor <=
        RICTUS_MODULE_API_MINOR
    );


    /*
     * M05 callbacks
     */

    RICTUS_TEST(
        rictus_intelligence_descriptor.qualify ==
        rictus_intelligence_qualify &&
        rictus_intelligence_descriptor.start ==
        rictus_intelligence_start &&
        rictus_intelligence_descriptor.stop ==
        rictus_intelligence_stop
    );


    /*
     * M06 NASA
     */

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


    /*
     * M07 SpaceX
     */

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


    /*
     * M08 unknown source
     */

    RICTUS_TEST(
        rictus_intelligence_source_from_name(
            "Unknown Source"
        ) ==
        RICTUS_INTELLIGENCE_SOURCE_OTHER
    );


    /*
     * M09 invalid source
     */

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


    /*
     * M10 negative validation
     */

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