/*
 * STN-LABZ
 * Rictus Core
 *
 * main.c
 *
 * Production Core entry point.
 *
 * Responsibilities:
 *
 * - configuration
 * - operational logging
 * - network transport
 * - TLS
 * - IRC registration
 * - SASL authentication
 * - session recovery
 * - controlled shutdown
 * - dynamic module discovery
 * - module verification
 * - module qualification
 * - qualification persistence
 * - deferred module activation
 * - Core-to-module host services
 * - controlled module shutdown
 * - DLL unload
 */

#define WIN32_LEAN_AND_MEAN

#include <winsock2.h>
#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "log.h"
#include "net_win.h"
#include "tls_win.h"
#include "irc.h"
#include "session.h"
#include "shutdown.h"

#include "module.h"
#include "module_discovery.h"
#include "module_inventory.h"
#include "module_loader.h"
#include "module_registry.h"


#define RICTUS_CONFIG_FILE \
    "rictus.local.conf"

#define RICTUS_REALNAME \
    "STN-LABZ Rictus Bot"

#define RICTUS_LOG_DIRECTORY \
    "C:\\stn-labz\\rictus\\logs"

#define RICTUS_LOG_FILENAME \
    "rictus.log"

#define RICTUS_STATE_DIRECTORY \
    "C:\\stn-labz\\rictus"

#define IRC_BUFFER_SIZE \
    8192

#define RICTUS_MODULE_PATH_MAX \
    1024

#define RICTUS_IRC_COMMAND_MAX \
    512


 /*
  * ------------------------------------------------
  * MODULE HOST CONTEXT
  * ------------------------------------------------
  */

static rictus_connection*
g_module_connection =
NULL;


static rictus_tls*
g_module_tls =
NULL;


static const rictus_config*
g_module_config =
NULL;


/*
 * ------------------------------------------------
 * MODULE IRC CALLBACK
 * ------------------------------------------------
 */

static int
rictus_module_send_message(
    const char* message
)
{
    char command[
        RICTUS_IRC_COMMAND_MAX
    ];

    int written;


    if (
        message == NULL ||
        message[0] == '\0' ||
        g_module_connection == NULL ||
        g_module_tls == NULL ||
        g_module_config == NULL
        )
    {
        return 0;
    }


    if (
        g_module_connection->socket ==
        INVALID_SOCKET
        )
    {
        return 0;
    }


    /*
     * Prevent command injection.
     */

    if (
        strchr(
            message,
            '\r'
        ) != NULL ||
        strchr(
            message,
            '\n'
        ) != NULL
        )
    {
        return 0;
    }


    written =
        snprintf(
            command,
            sizeof(command),
            "PRIVMSG %s :%s",
            g_module_config->irc_channel,
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


    if (
        !irc_send(
            g_module_tls,
            g_module_connection->socket,
            command
        )
        )
    {
        rictus_log_write(
            "ERROR",
            "MODULE_IRC_MESSAGE_FAILED",
            "target=%s",
            g_module_config->irc_channel
        );


        return 0;
    }


    rictus_log_write(
        "INFO",
        "MODULE_IRC_MESSAGE",
        "target=%s",
        g_module_config->irc_channel
    );


    return 1;
}


/*
 * ------------------------------------------------
 * MODULE HOST API
 * ------------------------------------------------
 */

static const rictus_module_host_t
g_module_host =
{
    rictus_module_send_message
};


/*
 * ------------------------------------------------
 * CONSOLE CONTROL HANDLER
 * ------------------------------------------------
 */

static BOOL WINAPI
rictus_console_handler(
    DWORD control_type
)
{
    switch (
        control_type
        )
    {
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:

        rictus_shutdown_request(
            RICTUS_SHUTDOWN_OPERATOR
        );

        return TRUE;


    case CTRL_CLOSE_EVENT:
    case CTRL_LOGOFF_EVENT:
    case CTRL_SHUTDOWN_EVENT:

        rictus_shutdown_request(
            RICTUS_SHUTDOWN_SYSTEM
        );

        return TRUE;


    default:

        return FALSE;
    }
}


/*
 * ------------------------------------------------
 * ENSURE DIRECTORY
 * ------------------------------------------------
 */

static int
rictus_ensure_directory(
    const char* path
)
{
    DWORD attributes;


    if (
        path == NULL ||
        path[0] == '\0'
        )
    {
        return 0;
    }


    attributes =
        GetFileAttributesA(
            path
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
            path,
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


/*
 * ------------------------------------------------
 * MODULE INITIALIZATION
 * ------------------------------------------------
 */

static int
rictus_modules_initialize(
    rictus_module_registry_t* registry,
    rictus_module_loader_t* loader,
    rictus_module_inventory_t* inventory,
    char* modules_path,
    size_t modules_path_size
)
{
    rictus_module_result_t
        module_result;

    rictus_module_inventory_result_t
        inventory_result;


    if (
        registry == NULL ||
        loader == NULL ||
        inventory == NULL ||
        modules_path == NULL ||
        modules_path_size == 0
        )
    {
        return 0;
    }


    rictus_module_registry_init(
        registry
    );


    rictus_module_loader_init(
        loader
    );


    rictus_module_inventory_init(
        inventory
    );


    module_result =
        rictus_module_discovery_get_path(
            modules_path,
            modules_path_size
        );


    if (
        module_result !=
        RICTUS_MODULE_OK
        )
    {
        rictus_log_write(
            "ERROR",
            "MODULE_PATH_FAILED",
            "result=%s",
            rictus_module_result_string(
                module_result
            )
        );


        return 0;
    }


    if (
        !rictus_ensure_directory(
            modules_path
        )
        )
    {
        rictus_log_write(
            "ERROR",
            "MODULE_DIRECTORY_FAILED",
            "path=%s",
            modules_path
        );


        return 0;
    }


    if (
        !rictus_log_write(
            "INFO",
            "MODULE_DIRECTORY",
            "path=%s",
            modules_path
        )
        )
    {
        return 0;
    }


    if (
        !rictus_ensure_directory(
            RICTUS_STATE_DIRECTORY
        )
        )
    {
        rictus_log_write(
            "ERROR",
            "MODULE_STATE_DIRECTORY_FAILED",
            "path=%s",
            RICTUS_STATE_DIRECTORY
        );


        return 0;
    }


    inventory_result =
        rictus_module_inventory_configure(
            inventory,
            RICTUS_STATE_DIRECTORY
        );


    if (
        inventory_result !=
        RICTUS_MODULE_INVENTORY_OK
        )
    {
        rictus_log_write(
            "ERROR",
            "MODULE_INVENTORY_CONFIG_FAILED",
            "result=%s",
            rictus_module_inventory_result_string(
                inventory_result
            )
        );


        return 0;
    }


    inventory_result =
        rictus_module_inventory_load(
            inventory
        );


    if (
        inventory_result !=
        RICTUS_MODULE_INVENTORY_OK
        )
    {
        if (
            !rictus_log_write(
                "WARN",
                "MODULE_INVENTORY_INVALID",
                "result=%s action=REQUALIFICATION_REQUIRED",
                rictus_module_inventory_result_string(
                    inventory_result
                )
            )
            )
        {
            return 0;
        }


        rictus_module_inventory_init(
            inventory
        );


        inventory_result =
            rictus_module_inventory_configure(
                inventory,
                RICTUS_STATE_DIRECTORY
            );


        if (
            inventory_result !=
            RICTUS_MODULE_INVENTORY_OK
            )
        {
            return 0;
        }
    }


    printf(
        "[CORE] Module path: %s\n",
        modules_path
    );


    printf(
        "[CORE] Module inventory: %u record(s)\n",
        (unsigned int)
        inventory->count
    );


    return
        rictus_log_write(
            "INFO",
            "MODULE_INVENTORY",
            "records=%u",
            (unsigned int)
            inventory->count
        );
}


/*
 * ------------------------------------------------
 * MODULE ACTIVATION
 * ------------------------------------------------
 */

static int
rictus_module_activate(
    rictus_module_record_t* record
)
{
    rictus_module_result_t
        result;


    if (
        record == NULL
        )
    {
        return 0;
    }


    if (
        record->state !=
        RICTUS_MODULE_STATE_QUALIFIED
        )
    {
        rictus_log_write(
            "ERROR",
            "MODULE_ACTIVATION_DENIED",
            "id=%s state=%s",
            record->descriptor.id,
            rictus_module_state_string(
                record->state
            )
        );


        return 0;
    }


    if (
        record->descriptor.start == NULL
        )
    {
        record->state =
            RICTUS_MODULE_STATE_FAILED;


        rictus_log_write(
            "ERROR",
            "MODULE_START_MISSING",
            "id=%s",
            record->descriptor.id
        );


        return 0;
    }


    printf(
        "[MODULE] Activation starting: %s\n",
        record->descriptor.id
    );


    if (
        !rictus_log_write(
            "INFO",
            "MODULE_ACTIVATION_START",
            "id=%s",
            record->descriptor.id
        )
        )
    {
        return 0;
    }


    result =
        record->descriptor.start(
            &g_module_host
        );


    if (
        result !=
        RICTUS_MODULE_OK
        )
    {
        record->state =
            RICTUS_MODULE_STATE_FAILED;


        printf(
            "[MODULE] Activation FAILED: %s (%s)\n",
            record->descriptor.id,
            rictus_module_result_string(
                result
            )
        );


        rictus_log_write(
            "ERROR",
            "MODULE_ACTIVATION_FAILED",
            "id=%s result=%s",
            record->descriptor.id,
            rictus_module_result_string(
                result
            )
        );


        return 0;
    }


    record->state =
        RICTUS_MODULE_STATE_ACTIVE;


    printf(
        "[MODULE] ACTIVE: %s\n",
        record->descriptor.id
    );


    return
        rictus_log_write(
            "INFO",
            "MODULE_ACTIVE",
            "id=%s version=%u.%u.%u",
            record->descriptor.id,
            record->descriptor.version_major,
            record->descriptor.version_minor,
            record->descriptor.version_patch
        );
}


/*
 * ------------------------------------------------
 * ACTIVATE QUALIFIED MODULES
 * ------------------------------------------------
 */

static void
rictus_modules_activate_all(
    rictus_module_registry_t* registry
)
{
    size_t index;


    if (
        registry == NULL
        )
    {
        return;
    }


    printf(
        "[CORE] On station. Activating qualified modules.\n"
    );


    rictus_log_write(
        "INFO",
        "MODULE_ACTIVATION_GATE_OPEN",
        "reason=IRC_CHANNEL_JOINED count=%u",
        (unsigned int)
        registry->count
    );


    for (
        index = 0;
        index < registry->count;
        ++index
        )
    {
        rictus_module_record_t
            * record;


        record =
            &registry->modules[index];


        if (
            record->state !=
            RICTUS_MODULE_STATE_QUALIFIED
            )
        {
            continue;
        }


        /*
         * Failure remains isolated to the module.
         */

        (void)
            rictus_module_activate(
                record
            );
    }
}


/*
 * ------------------------------------------------
 * MODULE STOP
 * ------------------------------------------------
 */

static int
rictus_module_stop(
    rictus_module_record_t* record
)
{
    rictus_module_result_t
        result;


    if (
        record == NULL
        )
    {
        return 0;
    }


    if (
        record->state !=
        RICTUS_MODULE_STATE_ACTIVE
        )
    {
        return 1;
    }


    if (
        record->descriptor.stop == NULL
        )
    {
        record->state =
            RICTUS_MODULE_STATE_FAILED;


        rictus_log_write(
            "ERROR",
            "MODULE_STOP_MISSING",
            "id=%s",
            record->descriptor.id
        );


        return 0;
    }


    printf(
        "[MODULE] Stop starting: %s\n",
        record->descriptor.id
    );


    rictus_log_write(
        "INFO",
        "MODULE_STOP_START",
        "id=%s",
        record->descriptor.id
    );


    result =
        record->descriptor.stop();


    if (
        result !=
        RICTUS_MODULE_OK
        )
    {
        record->state =
            RICTUS_MODULE_STATE_FAILED;


        printf(
            "[MODULE] Stop FAILED: %s (%s)\n",
            record->descriptor.id,
            rictus_module_result_string(
                result
            )
        );


        rictus_log_write(
            "ERROR",
            "MODULE_STOP_FAILED",
            "id=%s result=%s",
            record->descriptor.id,
            rictus_module_result_string(
                result
            )
        );


        return 0;
    }


    record->state =
        RICTUS_MODULE_STATE_QUALIFIED;


    printf(
        "[MODULE] Stopped: %s\n",
        record->descriptor.id
    );


    rictus_log_write(
        "INFO",
        "MODULE_STOPPED",
        "id=%s",
        record->descriptor.id
    );


    return 1;
}


/*
 * ------------------------------------------------
 * STOP ALL ACTIVE MODULES
 * ------------------------------------------------
 */

static void
rictus_modules_stop_all(
    rictus_module_registry_t* registry
)
{
    size_t index;


    if (
        registry == NULL
        )
    {
        return;
    }


    index =
        registry->count;


    while (
        index > 0
        )
    {
        --index;


        (void)
            rictus_module_stop(
                &registry->modules[index]
            );
    }
}


/*
 * ------------------------------------------------
 * PROCESS DISCOVERED MODULES
 * ------------------------------------------------
 */

static int
rictus_modules_process(
    rictus_module_registry_t* registry,
    rictus_module_inventory_t* inventory
)
{
    size_t index;


    if (
        registry == NULL ||
        inventory == NULL
        )
    {
        return 0;
    }


    for (
        index = 0;
        index < registry->count;
        ++index
        )
    {
        rictus_module_record_t
            * record;

        const rictus_module_inventory_record_t
            * inventory_record;

        rictus_module_result_t
            module_result;

        rictus_module_inventory_result_t
            inventory_result;

        int qualified =
            0;


        record =
            &registry->modules[index];


        printf(
            "[MODULE] Discovered: %s (%s)\n",
            record->descriptor.name,
            record->descriptor.id
        );


        if (
            !rictus_log_write(
                "INFO",
                "MODULE_DISCOVERED",
                "id=%s name=%s version=%u.%u.%u",
                record->descriptor.id,
                record->descriptor.name,
                record->descriptor.version_major,
                record->descriptor.version_minor,
                record->descriptor.version_patch
            )
            )
        {
            return 0;
        }


        /*
         * ------------------------------------------------
         * VERIFY
         * ------------------------------------------------
         */

        module_result =
            rictus_module_registry_verify(
                registry,
                record->descriptor.id
            );


        if (
            module_result !=
            RICTUS_MODULE_OK
            )
        {
            printf(
                "[MODULE] Verification FAILED: %s (%s)\n",
                record->descriptor.id,
                rictus_module_result_string(
                    module_result
                )
            );


            rictus_log_write(
                "ERROR",
                "MODULE_VERIFY_FAILED",
                "id=%s result=%s",
                record->descriptor.id,
                rictus_module_result_string(
                    module_result
                )
            );


            continue;
        }


        /*
         * ------------------------------------------------
         * RESTORE QUALIFICATION
         * ------------------------------------------------
         */

        inventory_record =
            rictus_module_inventory_find(
                inventory,
                &record->descriptor
            );


        if (
            inventory_record != NULL
            )
        {
            module_result =
                rictus_module_registry_restore_qualification(
                    registry,
                    record->descriptor.id,
                    &inventory_record->qualification
                );


            if (
                module_result ==
                RICTUS_MODULE_OK
                )
            {
                printf(
                    "[MODULE] QUALIFIED: %s "
                    "(restored %u/%u, negative PASS)\n",
                    record->descriptor.id,
                    inventory_record
                    ->qualification
                    .tests_passed,
                    inventory_record
                    ->qualification
                    .tests_executed
                );


                if (
                    !rictus_log_write(
                        "INFO",
                        "MODULE_QUALIFICATION_RESTORED",
                        "id=%s tests=%u/%u negative=PASS",
                        record->descriptor.id,
                        inventory_record
                        ->qualification
                        .tests_passed,
                        inventory_record
                        ->qualification
                        .tests_executed
                    )
                    )
                {
                    return 0;
                }


                qualified =
                    1;
            }
        }


        /*
         * ------------------------------------------------
         * LIVE QUALIFICATION
         * ------------------------------------------------
         */

        if (
            !qualified
            )
        {
            printf(
                "[MODULE] Qualification starting: %s\n",
                record->descriptor.id
            );


            if (
                !rictus_log_write(
                    "INFO",
                    "MODULE_QUALIFICATION_START",
                    "id=%s",
                    record->descriptor.id
                )
                )
            {
                return 0;
            }


            module_result =
                rictus_module_registry_qualify(
                    registry,
                    record->descriptor.id
                );


            if (
                module_result !=
                RICTUS_MODULE_OK
                )
            {
                printf(
                    "[MODULE] Qualification FAILED: %s (%s)\n",
                    record->descriptor.id,
                    rictus_module_result_string(
                        module_result
                    )
                );


                rictus_log_write(
                    "ERROR",
                    "MODULE_QUALIFICATION_FAILED",
                    "id=%s result=%s",
                    record->descriptor.id,
                    rictus_module_result_string(
                        module_result
                    )
                );


                continue;
            }


            printf(
                "[MODULE] Qualification PASS: %s (%u/%u)\n",
                record->descriptor.id,
                record->qualification.tests_passed,
                record->qualification.tests_executed
            );


            printf(
                "[MODULE] Negative validation: %s\n",
                record
                ->qualification
                .negative_test_passed
                ? "PASS"
                : "FAIL"
            );


            inventory_result =
                rictus_module_inventory_store(
                    inventory,
                    &record->descriptor,
                    &record->qualification
                );


            if (
                inventory_result !=
                RICTUS_MODULE_INVENTORY_OK
                )
            {
                fprintf(
                    stderr,
                    "[MODULE] Inventory write failed: %s (%s)\n",
                    record->descriptor.id,
                    rictus_module_inventory_result_string(
                        inventory_result
                    )
                );


                (void)
                    rictus_module_registry_fail(
                        registry,
                        record->descriptor.id
                    );


                rictus_log_write(
                    "ERROR",
                    "MODULE_INVENTORY_WRITE_FAILED",
                    "id=%s result=%s",
                    record->descriptor.id,
                    rictus_module_inventory_result_string(
                        inventory_result
                    )
                );


                continue;
            }


            if (
                !rictus_log_write(
                    "INFO",
                    "MODULE_QUALIFIED",
                    "id=%s tests=%u/%u negative=PASS inventory=STORED",
                    record->descriptor.id,
                    record->qualification.tests_passed,
                    record->qualification.tests_executed
                )
                )
            {
                return 0;
            }


            qualified =
                1;
        }


        /*
         * ------------------------------------------------
         * WAIT FOR IRC OPERATIONAL STATE
         * ------------------------------------------------
         */

        if (
            qualified
            )
        {
            printf(
                "[MODULE] Awaiting on-station activation: %s\n",
                record->descriptor.id
            );


            if (
                !rictus_log_write(
                    "INFO",
                    "MODULE_AWAITING_ACTIVATION",
                    "id=%s state=%s",
                    record->descriptor.id,
                    rictus_module_state_string(
                        record->state
                    )
                )
                )
            {
                return 0;
            }
        }
    }


    return 1;
}


/*
 * ------------------------------------------------
 * MODULE DISCOVERY
 * ------------------------------------------------
 */

static int
rictus_modules_discover(
    rictus_module_registry_t* registry,
    rictus_module_loader_t* loader,
    rictus_module_inventory_t* inventory,
    const char* modules_path
)
{
    rictus_module_discovery_report_t
        report;

    rictus_module_result_t
        result;


    if (
        registry == NULL ||
        loader == NULL ||
        inventory == NULL ||
        modules_path == NULL
        )
    {
        return 0;
    }


    memset(
        &report,
        0,
        sizeof(report)
    );


    result =
        rictus_module_discovery_scan(
            registry,
            loader,
            modules_path,
            &report
        );


    if (
        result !=
        RICTUS_MODULE_OK
        )
    {
        rictus_log_write(
            "ERROR",
            "MODULE_DISCOVERY_FAILED",
            "result=%s",
            rictus_module_result_string(
                result
            )
        );


        return 0;
    }


    printf(
        "[MODULE] Discovery: "
        "%u directories, "
        "%u loaded, "
        "%u discovered, "
        "%u rejected\n",
        (unsigned int)
        report.directories_examined,
        (unsigned int)
        report.modules_loaded,
        (unsigned int)
        report.modules_discovered,
        (unsigned int)
        report.modules_rejected
    );


    if (
        !rictus_log_write(
            "INFO",
            "MODULE_DISCOVERY_COMPLETE",
            "directories=%u loaded=%u discovered=%u rejected=%u",
            (unsigned int)
            report.directories_examined,
            (unsigned int)
            report.modules_loaded,
            (unsigned int)
            report.modules_discovered,
            (unsigned int)
            report.modules_rejected
        )
        )
    {
        return 0;
    }


    return
        rictus_modules_process(
            registry,
            inventory
        );
}


/*
 * ------------------------------------------------
 * TRANSPORT CLOSE
 * ------------------------------------------------
 */

static void
rictus_transport_close(
    rictus_connection* conn,
    rictus_tls* tls,
    int* tls_initialized
)
{
    if (
        tls != NULL &&
        tls_initialized != NULL &&
        *tls_initialized
        )
    {
        rictus_log_write(
            "INFO",
            "TLS_CLEANUP",
            ""
        );


        tls_cleanup(
            tls
        );


        *tls_initialized =
            0;
    }


    if (
        conn != NULL &&
        conn->socket !=
        INVALID_SOCKET
        )
    {
        rictus_log_write(
            "INFO",
            "TCP_CLOSE",
            ""
        );


        net_close(
            conn
        );
    }
}


/*
 * ------------------------------------------------
 * TRANSPORT OPEN
 * ------------------------------------------------
 */

static int
rictus_transport_open(
    const rictus_config* config,
    rictus_connection* conn,
    rictus_tls* tls,
    int* tls_initialized,
    int recovery
)
{
    if (
        config == NULL ||
        conn == NULL ||
        tls == NULL ||
        tls_initialized == NULL
        )
    {
        return 0;
    }


    conn->socket =
        INVALID_SOCKET;


    memset(
        tls,
        0,
        sizeof(*tls)
    );


    if (
        !rictus_log_write(
            recovery
            ? "WARN"
            : "INFO",
            recovery
            ? "RECOVERY_TCP_ATTEMPT"
            : "TCP_CONNECT_ATTEMPT",
            "server=%s port=%s",
            config->irc_server,
            config->irc_port
        )
        )
    {
        return 0;
    }


    printf(
        "%s %s:%s...\n",
        recovery
        ? "Recovering connection to"
        : "Connecting to",
        config->irc_server,
        config->irc_port
    );


    if (
        !net_connect_tcp(
            conn,
            config->irc_server,
            config->irc_port
        )
        )
    {
        rictus_log_write(
            "ERROR",
            recovery
            ? "RECOVERY_TCP_FAILED"
            : "TCP_CONNECT_FAILED",
            "server=%s port=%s",
            config->irc_server,
            config->irc_port
        );


        return 0;
    }


    if (
        !rictus_log_write(
            "INFO",
            recovery
            ? "RECOVERY_TCP_ESTABLISHED"
            : "TCP_CONNECTED",
            "server=%s port=%s",
            config->irc_server,
            config->irc_port
        )
        )
    {
        net_close(
            conn
        );


        return 0;
    }


    if (
        !tls_init(
            tls
        )
        )
    {
        rictus_log_write(
            "ERROR",
            recovery
            ? "RECOVERY_TLS_INIT_FAILED"
            : "TLS_INIT_FAILED",
            ""
        );


        net_close(
            conn
        );


        return 0;
    }


    *tls_initialized =
        1;


    if (
        !rictus_log_write(
            "INFO",
            recovery
            ? "RECOVERY_TLS_HANDSHAKE_START"
            : "TLS_HANDSHAKE_START",
            "server=%s",
            config->irc_server
        )
        )
    {
        rictus_transport_close(
            conn,
            tls,
            tls_initialized
        );


        return 0;
    }


    if (
        !tls_handshake(
            tls,
            conn->socket,
            config->irc_server
        )
        )
    {
        rictus_log_write(
            "ERROR",
            recovery
            ? "RECOVERY_TLS_FAILED"
            : "TLS_HANDSHAKE_FAILED",
            "server=%s",
            config->irc_server
        );


        rictus_transport_close(
            conn,
            tls,
            tls_initialized
        );


        return 0;
    }


    return
        rictus_log_write(
            "INFO",
            recovery
            ? "RECOVERY_TLS_ESTABLISHED"
            : "TLS_ESTABLISHED",
            "server=%s",
            config->irc_server
        );
}


/*
 * ------------------------------------------------
 * IRC REGISTRATION
 * ------------------------------------------------
 */

static int
rictus_registration_start(
    const rictus_config* config,
    rictus_connection* conn,
    rictus_tls* tls,
    int recovery
)
{
    char command[
        512
    ];

    int length;


    if (
        config == NULL ||
        conn == NULL ||
        tls == NULL
        )
    {
        return 0;
    }


    if (
        !irc_send(
            tls,
            conn->socket,
            "CAP LS 302"
        )
        )
    {
        return 0;
    }


    length =
        snprintf(
            command,
            sizeof(command),
            "NICK %s",
            config->irc_nick
        );


    if (
        length <= 0 ||
        length >=
        (int)sizeof(command)
        )
    {
        return 0;
    }


    if (
        !irc_send(
            tls,
            conn->socket,
            command
        )
        )
    {
        return 0;
    }


    length =
        snprintf(
            command,
            sizeof(command),
            "USER %s 0 * :%s",
            config->irc_nick,
            RICTUS_REALNAME
        );


    if (
        length <= 0 ||
        length >=
        (int)sizeof(command)
        )
    {
        return 0;
    }


    if (
        !irc_send(
            tls,
            conn->socket,
            command
        )
        )
    {
        return 0;
    }


    return
        rictus_log_write(
            "INFO",
            recovery
            ? "RECOVERY_IRC_REGISTRATION_START"
            : "IRC_REGISTRATION_START",
            "nick=%s",
            config->irc_nick
        );
}


/*
 * ------------------------------------------------
 * SESSION RECOVERY
 * ------------------------------------------------
 */

static int
rictus_recover_session(
    const rictus_config* config,
    rictus_connection* conn,
    rictus_tls* tls,
    rictus_irc_state* state,
    int* tls_initialized
)
{
    if (
        config == NULL ||
        conn == NULL ||
        tls == NULL ||
        state == NULL ||
        tls_initialized == NULL
        )
    {
        return 0;
    }


    if (
        !rictus_log_write(
            "WARN",
            "RECOVERY_BEGIN",
            "last_registered=%d last_sasl=%d last_joined=%d channel=%s",
            state->registered,
            state->sasl_complete,
            state->joined,
            config->irc_channel
        )
        )
    {
        return 0;
    }


    rictus_transport_close(
        conn,
        tls,
        tls_initialized
    );


    memset(
        state,
        0,
        sizeof(*state)
    );


    if (
        !rictus_log_write(
            "INFO",
            "RECOVERY_STATE_RESET",
            "registered=0 sasl=0 joined=0"
        )
        )
    {
        return 0;
    }


    if (
        rictus_shutdown_requested()
        )
    {
        return 0;
    }


    if (
        !rictus_transport_open(
            config,
            conn,
            tls,
            tls_initialized,
            1
        )
        )
    {
        return 0;
    }


    if (
        rictus_shutdown_requested()
        )
    {
        return 0;
    }


    if (
        !rictus_registration_start(
            config,
            conn,
            tls,
            1
        )
        )
    {
        rictus_transport_close(
            conn,
            tls,
            tls_initialized
        );


        return 0;
    }


    return
        rictus_log_write(
            "INFO",
            "RECOVERY_IN_PROGRESS",
            "awaiting_sasl_registration_channel"
        );
}


/*
 * ------------------------------------------------
 * MAIN
 * ------------------------------------------------
 */

int
main(void)
{
    rictus_config
        config;

    rictus_connection
        conn;

    rictus_tls
        tls;

    rictus_irc_state
        state;


    rictus_module_registry_t
        module_registry;

    rictus_module_loader_t
        module_loader;

    rictus_module_inventory_t
        module_inventory;


    char modules_path[
        RICTUS_MODULE_PATH_MAX
    ];

    char receive_buffer[
        IRC_BUFFER_SIZE
    ];

    char line_buffer[
        IRC_BUFFER_SIZE
    ];


    int line_length =
        0;

    int exit_code =
        EXIT_SUCCESS;

    int log_initialized =
        0;

    int network_initialized =
        0;

    int tls_initialized =
        0;

    int console_handler_installed =
        0;

    int controlled_shutdown =
        0;

    int recovering =
        0;

    int modules_activated =
        0;


    rictus_shutdown_reason
        shutdown_reason =
        RICTUS_SHUTDOWN_NONE;


    memset(
        &config,
        0,
        sizeof(config)
    );

    memset(
        &conn,
        0,
        sizeof(conn)
    );

    memset(
        &tls,
        0,
        sizeof(tls)
    );

    memset(
        &state,
        0,
        sizeof(state)
    );

    memset(
        &module_registry,
        0,
        sizeof(module_registry)
    );

    memset(
        &module_loader,
        0,
        sizeof(module_loader)
    );

    memset(
        &module_inventory,
        0,
        sizeof(module_inventory)
    );

    memset(
        modules_path,
        0,
        sizeof(modules_path)
    );

    memset(
        receive_buffer,
        0,
        sizeof(receive_buffer)
    );

    memset(
        line_buffer,
        0,
        sizeof(line_buffer)
    );


    conn.socket =
        INVALID_SOCKET;


    /*
     * ------------------------------------------------
     * SHUTDOWN CONTROL
     * ------------------------------------------------
     */

    rictus_shutdown_reset();


    if (
        !SetConsoleCtrlHandler(
            rictus_console_handler,
            TRUE
        )
        )
    {
        return
            EXIT_FAILURE;
    }


    console_handler_installed =
        1;


    /*
     * ------------------------------------------------
     * LOGGING
     * ------------------------------------------------
     */

    if (
        !rictus_log_init(
            RICTUS_LOG_DIRECTORY,
            RICTUS_LOG_FILENAME
        )
        )
    {
        return
            EXIT_FAILURE;
    }


    log_initialized =
        1;


    if (
        !rictus_log_write(
            "INFO",
            "START",
            "Rictus starting"
        )
        )
    {
        exit_code =
            EXIT_FAILURE;

        goto final_shutdown;
    }


    /*
     * ------------------------------------------------
     * CONFIGURATION
     * ------------------------------------------------
     */

    if (
        !config_load(
            RICTUS_CONFIG_FILE,
            &config
        )
        )
    {
        exit_code =
            EXIT_FAILURE;

        goto module_cleanup;
    }


    /*
     * ------------------------------------------------
     * MODULE DISCOVERY / QUALIFICATION
     * ------------------------------------------------
     */

    if (
        !rictus_modules_initialize(
            &module_registry,
            &module_loader,
            &module_inventory,
            modules_path,
            sizeof(modules_path)
        )
        )
    {
        exit_code =
            EXIT_FAILURE;

        goto module_cleanup;
    }


    if (
        !rictus_modules_discover(
            &module_registry,
            &module_loader,
            &module_inventory,
            modules_path
        )
        )
    {
        exit_code =
            EXIT_FAILURE;

        goto module_cleanup;
    }


    /*
     * ------------------------------------------------
     * NETWORK
     * ------------------------------------------------
     */

    if (
        !net_init()
        )
    {
        exit_code =
            EXIT_FAILURE;

        goto module_cleanup;
    }


    network_initialized =
        1;


    if (
        !rictus_transport_open(
            &config,
            &conn,
            &tls,
            &tls_initialized,
            0
        )
        )
    {
        exit_code =
            EXIT_FAILURE;

        goto network_cleanup;
    }


    if (
        !rictus_registration_start(
            &config,
            &conn,
            &tls,
            0
        )
        )
    {
        exit_code =
            EXIT_FAILURE;

        goto transport_cleanup;
    }


    /*
     * Host callbacks now have valid transport
     * objects, but modules remain inactive until
     * Core reaches IRC operational state.
     */

    g_module_connection =
        &conn;

    g_module_tls =
        &tls;

    g_module_config =
        &config;


    printf(
        "IRC registration transmitted.\n"
    );


    printf(
        "Listening for IRC traffic.\n"
    );


    rictus_log_write(
        "INFO",
        "IRC_LOOP_START",
        ""
    );


    /*
     * ------------------------------------------------
     * IRC LOOP
     * ------------------------------------------------
     */

    for (;;)
    {
        int received;

        int i;


        if (
            rictus_shutdown_requested()
            )
        {
            controlled_shutdown =
                1;

            break;
        }


        received =
            tls_recv(
                &tls,
                conn.socket,
                receive_buffer,
                sizeof(receive_buffer)
            );


        if (
            rictus_shutdown_requested()
            )
        {
            controlled_shutdown =
                1;

            break;
        }


        {
            rictus_session_result
                session_result;


            session_result =
                rictus_session_classify_receive(
                    received,
                    &config,
                    &state
                );


            if (
                session_result ==
                RICTUS_SESSION_RECEIVE_FAILURE ||
                session_result ==
                RICTUS_SESSION_SILENT_LOSS
                )
            {
                if (
                    !rictus_recover_session(
                        &config,
                        &conn,
                        &tls,
                        &state,
                        &tls_initialized
                    )
                    )
                {
                    if (
                        rictus_shutdown_requested()
                        )
                    {
                        controlled_shutdown =
                            1;

                        break;
                    }


                    exit_code =
                        EXIT_FAILURE;

                    break;
                }


                /*
                 * conn/tls storage did not move, so
                 * module host pointers remain valid.
                 */

                recovering =
                    1;


                line_length =
                    0;


                memset(
                    line_buffer,
                    0,
                    sizeof(line_buffer)
                );


                continue;
            }
        }


        for (
            i = 0;
            i < received;
            ++i
            )
        {
            char c;


            if (
                rictus_shutdown_requested()
                )
            {
                controlled_shutdown =
                    1;

                break;
            }


            c =
                receive_buffer[i];


            if (
                c == '\n'
                )
            {
                if (
                    line_length > 0 &&
                    line_buffer[
                        line_length - 1
                    ] == '\r'
                    )
                {
                    --line_length;
                }


                line_buffer[
                    line_length
                ] =
                    '\0';


                    if (
                        line_length > 0
                        )
                    {
                        if (
                            !irc_handle_line(
                                &tls,
                                conn.socket,
                                &config,
                                &state,
                                line_buffer
                            )
                            )
                        {
                            exit_code =
                                EXIT_FAILURE;

                            goto transport_cleanup;
                        }


                        /*
                         * ------------------------------------------------
                         * ON-STATION MODULE ACTIVATION
                         * ------------------------------------------------
                         */

                        if (
                            !modules_activated &&
                            state.sasl_complete &&
                            state.registered &&
                            state.joined
                            )
                        {
                            printf(
                                "[CORE] IRC operational state established.\n"
                            );


                            if (
                                !rictus_log_write(
                                    "INFO",
                                    "CORE_ON_STATION",
                                    "registered=%d sasl=%d joined=%d channel=%s",
                                    state.registered,
                                    state.sasl_complete,
                                    state.joined,
                                    config.irc_channel
                                )
                                )
                            {
                                exit_code =
                                    EXIT_FAILURE;

                                goto transport_cleanup;
                            }


                            rictus_modules_activate_all(
                                &module_registry
                            );


                            modules_activated =
                                1;
                        }


                        /*
                         * ------------------------------------------------
                         * RECOVERY COMPLETE
                         * ------------------------------------------------
                         */

                        if (
                            recovering &&
                            state.sasl_complete &&
                            state.registered &&
                            state.joined
                            )
                        {
                            recovering =
                                0;


                            if (
                                !rictus_log_write(
                                    "INFO",
                                    "RECOVERY_COMPLETE",
                                    "registered=%d sasl=%d joined=%d channel=%s",
                                    state.registered,
                                    state.sasl_complete,
                                    state.joined,
                                    config.irc_channel
                                )
                                )
                            {
                                exit_code =
                                    EXIT_FAILURE;

                                goto transport_cleanup;
                            }
                        }
                    }


                    line_length =
                        0;
            }
            else
            {
                if (
                    line_length >=
                    IRC_BUFFER_SIZE - 1
                    )
                {
                    rictus_log_write(
                        "ERROR",
                        "IRC_LINE_TOO_LONG",
                        ""
                    );


                    exit_code =
                        EXIT_FAILURE;

                    goto transport_cleanup;
                }


                line_buffer[
                    line_length++
                ] =
                    c;
            }
        }


        if (
            controlled_shutdown
            )
        {
            break;
        }
    }


    /*
     * ------------------------------------------------
     * CONTROLLED SHUTDOWN
     * ------------------------------------------------
     */

    if (
        controlled_shutdown
        )
    {
        shutdown_reason =
            rictus_shutdown_reason_get();


        exit_code =
            EXIT_SUCCESS;


        rictus_log_write(
            "INFO",
            "SHUTDOWN_REQUEST",
            "reason=%s",
            rictus_shutdown_reason_string(
                shutdown_reason
            )
        );
    }


transport_cleanup:


    /*
     * Modules that can use IRC must stop while the
     * transport is still valid.
     */

    if (
        module_registry.count > 0
        )
    {
        rictus_log_write(
            "INFO",
            "MODULE_SHUTDOWN_BEGIN",
            "count=%u",
            (unsigned int)
            module_registry.count
        );


        rictus_modules_stop_all(
            &module_registry
        );
    }


    /*
     * No module may call host services beyond here.
     */

    g_module_connection =
        NULL;

    g_module_tls =
        NULL;

    g_module_config =
        NULL;


    rictus_transport_close(
        &conn,
        &tls,
        &tls_initialized
    );


network_cleanup:


    if (
        network_initialized
        )
    {
        net_cleanup();


        network_initialized =
            0;
    }


module_cleanup:


    /*
     * If startup failed before transport creation,
     * this safely handles any module that somehow
     * reached ACTIVE.
     */

    if (
        module_registry.count > 0
        )
    {
        rictus_modules_stop_all(
            &module_registry
        );
    }


    if (
        module_loader.count > 0
        )
    {
        if (
            log_initialized
            )
        {
            rictus_log_write(
                "INFO",
                "MODULE_UNLOAD",
                "count=%u",
                (unsigned int)
                module_loader.count
            );
        }


        rictus_module_loader_unload_all(
            &module_loader
        );
    }


    SecureZeroMemory(
        config.irc_password,
        sizeof(config.irc_password)
    );


final_shutdown:


    if (
        log_initialized
        )
    {
        rictus_log_write(
            "INFO",
            "STOP",
            "exit_code=%d",
            exit_code
        );


        rictus_log_close();


        log_initialized =
            0;
    }


    if (
        console_handler_installed
        )
    {
        SetConsoleCtrlHandler(
            rictus_console_handler,
            FALSE
        );
    }


    rictus_shutdown_reset();


    printf(
        "Rictus stopped.\n"
    );


    return
        exit_code;
}