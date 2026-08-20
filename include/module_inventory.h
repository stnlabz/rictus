#ifndef RICTUS_MODULE_INVENTORY_H
#define RICTUS_MODULE_INVENTORY_H

#include <stddef.h>

#include "module.h"


#define RICTUS_MODULE_INVENTORY_MAX \
    32

#define RICTUS_MODULE_INVENTORY_PATH_MAX \
    1024


typedef enum
{
    RICTUS_MODULE_INVENTORY_OK = 0,

    RICTUS_MODULE_INVENTORY_ERR_INVALID_ARGUMENT,

    RICTUS_MODULE_INVENTORY_ERR_PATH_TOO_LONG,

    RICTUS_MODULE_INVENTORY_ERR_OPEN_FAILED,

    RICTUS_MODULE_INVENTORY_ERR_READ_FAILED,

    RICTUS_MODULE_INVENTORY_ERR_WRITE_FAILED,

    RICTUS_MODULE_INVENTORY_ERR_INVALID_FORMAT,

    RICTUS_MODULE_INVENTORY_ERR_FULL

} rictus_module_inventory_result_t;


/*
 * ------------------------------------------------
 * PERSISTED QUALIFICATION RECORD
 * ------------------------------------------------
 *
 * Core owns this evidence.
 *
 * The module does not write, modify, or determine
 * this record.
 */

typedef struct
{
    char module_id[
        RICTUS_MODULE_ID_MAX
    ];

    unsigned int version_major;

    unsigned int version_minor;

    unsigned int version_patch;


    unsigned int core_api_major;

    unsigned int core_api_minor;


    rictus_module_qualification_result_t qualification;

} rictus_module_inventory_record_t;


/*
 * ------------------------------------------------
 * INVENTORY
 * ------------------------------------------------
 */

typedef struct
{
    rictus_module_inventory_record_t
        records[
            RICTUS_MODULE_INVENTORY_MAX
        ];

    size_t count;


    char path[
        RICTUS_MODULE_INVENTORY_PATH_MAX
    ];

} rictus_module_inventory_t;


/*
 * ------------------------------------------------
 * API
 * ------------------------------------------------
 */

void rictus_module_inventory_init(
    rictus_module_inventory_t *inventory
);


rictus_module_inventory_result_t
rictus_module_inventory_configure(
    rictus_module_inventory_t *inventory,
    const char *state_path
);


rictus_module_inventory_result_t
rictus_module_inventory_load(
    rictus_module_inventory_t *inventory
);


rictus_module_inventory_result_t
rictus_module_inventory_store(
    rictus_module_inventory_t *inventory,
    const rictus_module_descriptor_t *descriptor,
    const rictus_module_qualification_result_t *qualification
);


const rictus_module_inventory_record_t *
rictus_module_inventory_find(
    const rictus_module_inventory_t *inventory,
    const rictus_module_descriptor_t *descriptor
);


const char *rictus_module_inventory_result_string(
    rictus_module_inventory_result_t result
);


#endif