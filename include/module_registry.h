#ifndef RICTUS_MODULE_REGISTRY_H
#define RICTUS_MODULE_REGISTRY_H

#include <stddef.h>

#include "module.h"


#define RICTUS_MODULE_REGISTRY_MAX \
    32

#define RICTUS_MODULE_AUDIT_MAX \
    128


/*
 * ------------------------------------------------
 * MODULE AUDIT EVENTS
 * ------------------------------------------------
 */

typedef enum
{
    RICTUS_MODULE_AUDIT_DISCOVERED = 0,

    RICTUS_MODULE_AUDIT_VERIFIED,

    RICTUS_MODULE_AUDIT_TESTING,

    RICTUS_MODULE_AUDIT_QUALIFIED,

    RICTUS_MODULE_AUDIT_FAILED,

    RICTUS_MODULE_AUDIT_AUTHORIZED,

    RICTUS_MODULE_AUDIT_ACTIVE,

    RICTUS_MODULE_AUDIT_QUARANTINED

} rictus_module_audit_event_t;


/*
 * ------------------------------------------------
 * MODULE RECORD
 * ------------------------------------------------
 */

typedef struct
{
    rictus_module_descriptor_t descriptor;

    rictus_module_state_t state;

    rictus_module_qualification_result_t qualification;

    int activation_authorized;

} rictus_module_record_t;


/*
 * ------------------------------------------------
 * MODULE AUDIT ENTRY
 * ------------------------------------------------
 */

typedef struct
{
    unsigned long sequence;

    char module_id[
        RICTUS_MODULE_ID_MAX
    ];

    rictus_module_audit_event_t event;

    rictus_module_state_t previous_state;

    rictus_module_state_t resulting_state;

    rictus_module_result_t result;

} rictus_module_audit_entry_t;


/*
 * ------------------------------------------------
 * MODULE REGISTRY
 * ------------------------------------------------
 */

typedef struct
{
    rictus_module_record_t modules[
        RICTUS_MODULE_REGISTRY_MAX
    ];

    size_t count;

    rictus_module_audit_entry_t audit[
        RICTUS_MODULE_AUDIT_MAX
    ];

    size_t audit_count;

    unsigned long next_sequence;

} rictus_module_registry_t;


/*
 * ------------------------------------------------
 * REGISTRY API
 * ------------------------------------------------
 */

void rictus_module_registry_init(
    rictus_module_registry_t *registry
);


rictus_module_result_t rictus_module_registry_discover(
    rictus_module_registry_t *registry,
    const rictus_module_descriptor_t *descriptor
);


rictus_module_result_t rictus_module_registry_verify(
    rictus_module_registry_t *registry,
    const char *module_id
);


rictus_module_result_t rictus_module_registry_qualify(
    rictus_module_registry_t *registry,
    const char *module_id
);


rictus_module_result_t rictus_module_registry_restore_qualification(
    rictus_module_registry_t *registry,
    const char *module_id,
    const rictus_module_qualification_result_t *qualification
);


rictus_module_result_t rictus_module_registry_authorize_activation(
    rictus_module_registry_t *registry,
    const char *module_id
);


rictus_module_result_t rictus_module_registry_activate(
    rictus_module_registry_t *registry,
    const char *module_id
);


rictus_module_result_t rictus_module_registry_fail(
    rictus_module_registry_t *registry,
    const char *module_id
);


rictus_module_result_t rictus_module_registry_quarantine(
    rictus_module_registry_t *registry,
    const char *module_id
);


const rictus_module_record_t *rictus_module_registry_find(
    const rictus_module_registry_t *registry,
    const char *module_id
);


#endif