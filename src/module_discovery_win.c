/*
 * STN-LABZ
 * Rictus Core
 *
 * module_discovery_win.c
 *
 * Windows dynamic module discovery.
 *
 * Expected layout:
 *
 *     <rictus.exe directory>\
 *         modules\
 *             intelligence\
 *                 module.conf
 *                 intelligence.dll
 *
 * module.conf:
 *
 *     id=intelligence
 *
 * The DLL filename is derived from the module ID:
 *
 *     <id>.dll
 *
 * Discovery does not establish qualification,
 * authorization, or activation.
 */

#define WIN32_LEAN_AND_MEAN

#include <windows.h>

#include <stdio.h>
#include <string.h>

#include "module_discovery.h"


#define RICTUS_DISCOVERY_PATH_MAX \
    1024

#define RICTUS_MODULE_CONF_NAME \
    "module.conf"


/*
 * ------------------------------------------------
 * PATH JOIN
 * ------------------------------------------------
 */

static int rictus_discovery_join_path(
    char *output,
    size_t output_size,
    const char *left,
    const char *right
)
{
    int written;


    if (
        output == NULL ||
        left == NULL ||
        right == NULL ||
        output_size == 0
    )
    {
        return 0;
    }


    written =
        snprintf(
            output,
            output_size,
            "%s\\%s",
            left,
            right
        );


    if (
        written < 0 ||
        (size_t)written >=
            output_size
    )
    {
        return 0;
    }


    return 1;
}


/*
 * ------------------------------------------------
 * MODULE ROOT
 * ------------------------------------------------
 */

rictus_module_result_t
rictus_module_discovery_get_path(
    char *modules_path,
    size_t modules_path_size
)
{
    char executable_path[
        RICTUS_DISCOVERY_PATH_MAX
    ];

    char *separator;

    DWORD length;

    int written;


    if (
        modules_path == NULL ||
        modules_path_size == 0
    )
    {
        return
            RICTUS_MODULE_ERR_INVALID_ARGUMENT;
    }


    memset(
        executable_path,
        0,
        sizeof(executable_path)
    );


    length =
        GetModuleFileNameA(
            NULL,
            executable_path,
            (DWORD)sizeof(executable_path)
        );


    if (
        length == 0 ||
        length >=
            sizeof(executable_path)
    )
    {
        return
            RICTUS_MODULE_ERR_NOT_FOUND;
    }


    separator =
        strrchr(
            executable_path,
            '\\'
        );


    if (
        separator == NULL
    )
    {
        separator =
            strrchr(
                executable_path,
                '/'
            );
    }


    if (
        separator == NULL
    )
    {
        return
            RICTUS_MODULE_ERR_NOT_FOUND;
    }


    *separator =
        '\0';


    written =
        snprintf(
            modules_path,
            modules_path_size,
            "%s\\modules",
            executable_path
        );


    if (
        written < 0 ||
        (size_t)written >=
            modules_path_size
    )
    {
        return
            RICTUS_MODULE_ERR_INVALID_ARGUMENT;
    }


    return
        RICTUS_MODULE_OK;
}


/*
 * ------------------------------------------------
 * READ MODULE ID
 * ------------------------------------------------
 *
 * Accepted module.conf:
 *
 *     id=<module-id>
 *
 * Blank lines and comments beginning with '#'
 * are accepted.
 *
 * Unknown keys or duplicate id declarations are
 * rejected.
 */

static int rictus_discovery_read_module_id(
    const char *config_path,
    char *module_id,
    size_t module_id_size
)
{
    FILE *file =
        NULL;

    char line[
        512
    ];

    int id_seen =
        0;


    if (
        config_path == NULL ||
        module_id == NULL ||
        module_id_size == 0
    )
    {
        return 0;
    }


    module_id[0] =
        '\0';


    if (
        fopen_s(
            &file,
            config_path,
            "r"
        ) != 0 ||
        file == NULL
    )
    {
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
        char *separator;

        char *key;

        char *value;

        size_t length;


        length =
            strlen(
                line
            );


        while (
            length > 0 &&
            (
                line[length - 1] == '\n' ||
                line[length - 1] == '\r'
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
            line[0] == '\0' ||
            line[0] == '#'
        )
        {
            continue;
        }


        separator =
            strchr(
                line,
                '='
            );


        if (
            separator == NULL
        )
        {
            fclose(
                file
            );

            return 0;
        }


        *separator =
            '\0';


        key =
            line;

        value =
            separator + 1;


        if (
            strcmp(
                key,
                "id"
            ) != 0
        )
        {
            fclose(
                file
            );

            return 0;
        }


        if (
            id_seen
        )
        {
            fclose(
                file
            );

            return 0;
        }


        length =
            strlen(
                value
            );


        if (
            length == 0 ||
            length >=
                module_id_size
        )
        {
            fclose(
                file
            );

            return 0;
        }


        memcpy(
            module_id,
            value,
            length + 1
        );


        id_seen =
            1;
    }


    fclose(
        file
    );


    return
        id_seen;
}


/*
 * ------------------------------------------------
 * DISCOVERY SCAN
 * ------------------------------------------------
 */

rictus_module_result_t
rictus_module_discovery_scan(
    rictus_module_registry_t *registry,
    rictus_module_loader_t *loader,
    const char *modules_path,
    rictus_module_discovery_report_t *report
)
{
    char search_path[
        RICTUS_DISCOVERY_PATH_MAX
    ];

    char directory_path[
        RICTUS_DISCOVERY_PATH_MAX
    ];

    char config_path[
        RICTUS_DISCOVERY_PATH_MAX
    ];

    char dll_name[
        RICTUS_MODULE_ID_MAX + 8
    ];

    char dll_path[
        RICTUS_DISCOVERY_PATH_MAX
    ];

    char module_id[
        RICTUS_MODULE_ID_MAX
    ];

    WIN32_FIND_DATAA find_data;

    HANDLE search;

    int written;


    if (
        registry == NULL ||
        loader == NULL ||
        modules_path == NULL ||
        report == NULL
    )
    {
        return
            RICTUS_MODULE_ERR_INVALID_ARGUMENT;
    }


    memset(
        report,
        0,
        sizeof(*report)
    );


    written =
        snprintf(
            search_path,
            sizeof(search_path),
            "%s\\*",
            modules_path
        );


    if (
        written < 0 ||
        (size_t)written >=
            sizeof(search_path)
    )
    {
        return
            RICTUS_MODULE_ERR_INVALID_ARGUMENT;
    }


    search =
        FindFirstFileA(
            search_path,
            &find_data
        );


    if (
        search ==
        INVALID_HANDLE_VALUE
    )
    {
        return
            RICTUS_MODULE_ERR_NOT_FOUND;
    }


    do
    {
        const rictus_module_descriptor_t
            *descriptor;

        rictus_module_loader_result_t
            loader_result;

        rictus_module_result_t
            registry_result;


        /*
         * Directories only.
         */

        if (
            (
                find_data.dwFileAttributes &
                FILE_ATTRIBUTE_DIRECTORY
            ) == 0
        )
        {
            continue;
        }


        if (
            strcmp(
                find_data.cFileName,
                "."
            ) == 0 ||
            strcmp(
                find_data.cFileName,
                ".."
            ) == 0
        )
        {
            continue;
        }


        report
            ->directories_examined++;


        /*
         * Build:
         *
         *     <modules>\<directory>
         */

        if (
            !rictus_discovery_join_path(
                directory_path,
                sizeof(directory_path),
                modules_path,
                find_data.cFileName
            )
        )
        {
            report
                ->modules_rejected++;

            continue;
        }


        /*
         * Build:
         *
         *     <module-directory>\module.conf
         */

        if (
            !rictus_discovery_join_path(
                config_path,
                sizeof(config_path),
                directory_path,
                RICTUS_MODULE_CONF_NAME
            )
        )
        {
            report
                ->modules_rejected++;

            continue;
        }


        /*
         * Read declared module ID.
         */

        if (
            !rictus_discovery_read_module_id(
                config_path,
                module_id,
                sizeof(module_id)
            )
        )
        {
            report
                ->modules_rejected++;

            continue;
        }


        /*
         * DLL filename is deterministic:
         *
         *     <module-id>.dll
         */

        written =
            snprintf(
                dll_name,
                sizeof(dll_name),
                "%s.dll",
                module_id
            );


        if (
            written < 0 ||
            (size_t)written >=
                sizeof(dll_name)
        )
        {
            report
                ->modules_rejected++;

            continue;
        }


        /*
         * Build:
         *
         *     <module-directory>\<module-id>.dll
         */

        if (
            !rictus_discovery_join_path(
                dll_path,
                sizeof(dll_path),
                directory_path,
                dll_name
            )
        )
        {
            report
                ->modules_rejected++;

            continue;
        }


        descriptor =
            NULL;


        /*
         * ------------------------------------------------
         * LOAD DLL
         * ------------------------------------------------
         *
         * Loader verifies:
         *
         * - DLL can load
         * - required ABI export exists
         * - descriptor is structurally valid
         * - descriptor ID matches module.conf ID
         */

        loader_result =
            rictus_module_loader_load(
                loader,
                module_id,
                dll_path,
                &descriptor
            );


        if (
            loader_result !=
            RICTUS_MODULE_LOADER_OK
        )
        {
            report
                ->modules_rejected++;

            continue;
        }


        report
            ->modules_loaded++;


        /*
         * ------------------------------------------------
         * REGISTER DISCOVERED MODULE
         * ------------------------------------------------
         *
         * Loading does not establish qualification.
         *
         * Registry places the module into the normal
         * Core-controlled lifecycle.
         */

        registry_result =
            rictus_module_registry_discover(
                registry,
                descriptor
            );


        if (
            registry_result !=
            RICTUS_MODULE_OK
        )
        {
            /*
             * Registry rejected it.
             *
             * Do not leave rejected executable code
             * resident.
             */

            (void)
            rictus_module_loader_unload(
                loader,
                module_id
            );


            report
                ->modules_rejected++;

            continue;
        }


        report
            ->modules_discovered++;

    } while (
        FindNextFileA(
            search,
            &find_data
        )
    );


    FindClose(
        search
    );


    return
        RICTUS_MODULE_OK;
}