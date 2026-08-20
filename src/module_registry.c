/*
 * STN-LABZ
 * Rictus Core
 *
 * module_registry.c
 *
 * Core-controlled module lifecycle registry.
 */

#include <string.h>

#include "module_registry.h"


/*
 * ------------------------------------------------
 * TEXT VALIDATION
 * ------------------------------------------------
 */

static int rictus_module_text_valid(
    const char *text,
    size_t capacity
)
{
    size_t length;


    if (
        text == NULL ||
        capacity == 0
    )
    {
        return 0;
    }


    length =
        strlen(
            text
        );


    if (
        length == 0 ||
        length >= capacity
    )
    {
        return 0;
    }


    return 1;
}


/*
 * ------------------------------------------------
 * MUTABLE LOOKUP
 * ------------------------------------------------
 */

static rictus_module_record_t *rictus_module_registry_find_mutable(
    rictus_module_registry_t *registry,
    const char *module_id
)
{
    size_t index;


    if (
        registry == NULL ||
        module_id == NULL
    )
    {
        return NULL;
    }


    for (
        index = 0;
        index < registry->count;
        ++index
    )
    {
        if (
            strcmp(
                registry
                    ->modules[index]
                    .descriptor
                    .id,
                module_id
            ) == 0
        )
        {
            return
                &registry->modules[index];
        }
    }


    return NULL;
}


/*
 * ------------------------------------------------
 * MODULE AUDIT
 * ------------------------------------------------
 */

static rictus_module_result_t rictus_module_audit(
    rictus_module_registry_t *registry,
    const rictus_module_record_t *module,
    rictus_module_audit_event_t event,
    rictus_module_state_t previous_state,
    rictus_module_result_t result
)
{
    rictus_module_audit_entry_t *entry;

    size_t length;


    if (
        registry == NULL ||
        module == NULL
    )
    {
        return
            RICTUS_MODULE_ERR_INVALID_ARGUMENT;
    }


    if (
        registry->audit_count >=
        RICTUS_MODULE_AUDIT_MAX
    )
    {
        return
            RICTUS_MODULE_ERR_AUDIT_FULL;
    }


    entry =
        &registry->audit[
            registry->audit_count
        ];


    memset(
        entry,
        0,
        sizeof(*entry)
    );


    entry->sequence =
        registry->next_sequence++;


    entry->event =
        event;


    entry->previous_state =
        previous_state;


    entry->resulting_state =
        module->state;


    entry->result =
        result;


    length =
        strlen(
            module->descriptor.id
        );


    if (
        length >=
        sizeof(entry->module_id)
    )
    {
        return
            RICTUS_MODULE_ERR_INVALID_IDENTITY;
    }


    memcpy(
        entry->module_id,
        module->descriptor.id,
        length + 1
    );


    registry->audit_count++;


    return
        RICTUS_MODULE_OK;
}


/*
 * ------------------------------------------------
 * INITIALIZATION
 * ------------------------------------------------
 */

void rictus_module_registry_init(
    rictus_module_registry_t *registry
)
{
    if (
        registry == NULL
    )
    {
        return;
    }


    memset(
        registry,
        0,
        sizeof(*registry)
    );


    registry->next_sequence =
        1;
}


/*
 * ------------------------------------------------
 * DISCOVERY
 * ------------------------------------------------
 */

rictus_module_result_t rictus_module_registry_discover(
    rictus_module_registry_t *registry,
    const rictus_module_descriptor_t *descriptor
)
{
    rictus_module_record_t *record;

    rictus_module_result_t audit_result;


    if (
        registry == NULL ||
        descriptor == NULL
    )
    {
        return
            RICTUS_MODULE_ERR_INVALID_ARGUMENT;
    }


    if (
        !rictus_module_text_valid(
            descriptor->id,
            sizeof(descriptor->id)
        ) ||
        !rictus_module_text_valid(
            descriptor->name,
            sizeof(descriptor->name)
        ) ||
        descriptor->qualify == NULL
    )
    {
        return
            RICTUS_MODULE_ERR_INVALID_IDENTITY;
    }


    if (
        rictus_module_registry_find_mutable(
            registry,
            descriptor->id
        ) != NULL
    )
    {
        return
            RICTUS_MODULE_ERR_DUPLICATE;
    }


    if (
        registry->count >=
        RICTUS_MODULE_REGISTRY_MAX
    )
    {
        return
            RICTUS_MODULE_ERR_REGISTRY_FULL;
    }


    record =
        &registry->modules[
            registry->count
        ];


    memset(
        record,
        0,
        sizeof(*record)
    );


    record->descriptor =
        *descriptor;


    record->state =
        RICTUS_MODULE_STATE_DISCOVERED;


    record->activation_authorized =
        0;


    audit_result =
        rictus_module_audit(
            registry,
            record,
            RICTUS_MODULE_AUDIT_DISCOVERED,
            RICTUS_MODULE_STATE_DISCOVERED,
            RICTUS_MODULE_OK
        );


    if (
        audit_result !=
        RICTUS_MODULE_OK
    )
    {
        memset(
            record,
            0,
            sizeof(*record)
        );


        return
            audit_result;
    }


    registry->count++;


    return
        RICTUS_MODULE_OK;
}


/*
 * ------------------------------------------------
 * VERIFY
 * ------------------------------------------------
 */

rictus_module_result_t rictus_module_registry_verify(
    rictus_module_registry_t *registry,
    const char *module_id
)
{
    rictus_module_record_t *record;

    rictus_module_state_t previous;

    rictus_module_result_t audit_result;


    record =
        rictus_module_registry_find_mutable(
            registry,
            module_id
        );


    if (
        record == NULL
    )
    {
        return
            RICTUS_MODULE_ERR_NOT_FOUND;
    }


    if (
        record->state ==
        RICTUS_MODULE_STATE_QUARANTINED
    )
    {
        return
            RICTUS_MODULE_ERR_QUARANTINED;
    }


    if (
        record->state !=
        RICTUS_MODULE_STATE_DISCOVERED
    )
    {
        return
            RICTUS_MODULE_ERR_INVALID_STATE;
    }


    previous =
        record->state;


    /*
     * Major API must match exactly.
     *
     * Module minor requirement may not exceed the
     * Core minor API supplied by this build.
     */

    if (
        record
            ->descriptor
            .required_core_api_major !=
            RICTUS_MODULE_API_MAJOR ||
        record
            ->descriptor
            .required_core_api_minor >
            RICTUS_MODULE_API_MINOR
    )
    {
        record->state =
            RICTUS_MODULE_STATE_FAILED;


        (void)rictus_module_audit(
            registry,
            record,
            RICTUS_MODULE_AUDIT_FAILED,
            previous,
            RICTUS_MODULE_ERR_INCOMPATIBLE
        );


        return
            RICTUS_MODULE_ERR_INCOMPATIBLE;
    }


    record->state =
        RICTUS_MODULE_STATE_UNVERIFIED;


    audit_result =
        rictus_module_audit(
            registry,
            record,
            RICTUS_MODULE_AUDIT_VERIFIED,
            previous,
            RICTUS_MODULE_OK
        );


    if (
        audit_result !=
        RICTUS_MODULE_OK
    )
    {
        record->state =
            previous;


        return
            audit_result;
    }


    return
        RICTUS_MODULE_OK;
}


/*
 * ------------------------------------------------
 * QUALIFY
 * ------------------------------------------------
 */

rictus_module_result_t rictus_module_registry_qualify(
    rictus_module_registry_t *registry,
    const char *module_id
)
{
    rictus_module_record_t *record;

    rictus_module_state_t previous;

    rictus_module_result_t module_result;

    rictus_module_result_t audit_result;

    rictus_module_qualification_result_t report;


    record =
        rictus_module_registry_find_mutable(
            registry,
            module_id
        );


    if (
        record == NULL
    )
    {
        return
            RICTUS_MODULE_ERR_NOT_FOUND;
    }


    if (
        record->state ==
        RICTUS_MODULE_STATE_QUARANTINED
    )
    {
        return
            RICTUS_MODULE_ERR_QUARANTINED;
    }


    if (
        record->state !=
        RICTUS_MODULE_STATE_UNVERIFIED
    )
    {
        return
            RICTUS_MODULE_ERR_INVALID_STATE;
    }


    previous =
        record->state;


    record->state =
        RICTUS_MODULE_STATE_TESTING;


    audit_result =
        rictus_module_audit(
            registry,
            record,
            RICTUS_MODULE_AUDIT_TESTING,
            previous,
            RICTUS_MODULE_OK
        );


    if (
        audit_result !=
        RICTUS_MODULE_OK
    )
    {
        record->state =
            previous;


        return
            audit_result;
    }


    memset(
        &report,
        0,
        sizeof(report)
    );


    /*
     * Core invokes the module qualification suite.
     *
     * The module executes tests.
     *
     * Core decides whether the reported evidence
     * satisfies qualification requirements.
     */

    module_result =
        record
            ->descriptor
            .qualify(
                &report
            );


    record->qualification =
        report;


    previous =
        record->state;


    if (
        module_result !=
            RICTUS_MODULE_OK ||
        report.tests_executed <
            RICTUS_MODULE_MIN_TESTS ||
        report.tests_passed !=
            report.tests_executed ||
        report.tests_failed != 0 ||
        report.tests_passed +
            report.tests_failed !=
            report.tests_executed ||
        !report.negative_test_executed ||
        !report.negative_test_passed
    )
    {
        record->state =
            RICTUS_MODULE_STATE_FAILED;


        record->activation_authorized =
            0;


        (void)rictus_module_audit(
            registry,
            record,
            RICTUS_MODULE_AUDIT_FAILED,
            previous,
            RICTUS_MODULE_ERR_QUALIFICATION
        );


        return
            RICTUS_MODULE_ERR_QUALIFICATION;
    }


    record->state =
        RICTUS_MODULE_STATE_QUALIFIED;


    audit_result =
        rictus_module_audit(
            registry,
            record,
            RICTUS_MODULE_AUDIT_QUALIFIED,
            previous,
            RICTUS_MODULE_OK
        );


    if (
        audit_result !=
        RICTUS_MODULE_OK
    )
    {
        record->state =
            RICTUS_MODULE_STATE_FAILED;


        return
            audit_result;
    }


    return
        RICTUS_MODULE_OK;
}


/*
 * ------------------------------------------------
 * RESTORE QUALIFICATION
 * ------------------------------------------------
 */

rictus_module_result_t rictus_module_registry_restore_qualification(
    rictus_module_registry_t *registry,
    const char *module_id,
    const rictus_module_qualification_result_t *qualification
)
{
    rictus_module_record_t *record;

    rictus_module_state_t previous;

    rictus_module_result_t audit_result;


    if (
        registry == NULL ||
        module_id == NULL ||
        qualification == NULL
    )
    {
        return
            RICTUS_MODULE_ERR_INVALID_ARGUMENT;
    }


    record =
        rictus_module_registry_find_mutable(
            registry,
            module_id
        );


    if (
        record == NULL
    )
    {
        return
            RICTUS_MODULE_ERR_NOT_FOUND;
    }


    if (
        record->state ==
        RICTUS_MODULE_STATE_QUARANTINED
    )
    {
        return
            RICTUS_MODULE_ERR_QUARANTINED;
    }


    /*
     * Persisted qualification may only be restored
     * after normal identity/API verification.
     */

    if (
        record->state !=
        RICTUS_MODULE_STATE_UNVERIFIED
    )
    {
        return
            RICTUS_MODULE_ERR_INVALID_STATE;
    }


    /*
     * Persisted evidence must satisfy exactly the
     * same qualification floor as live evidence.
     */

    if (
        qualification->tests_executed <
            RICTUS_MODULE_MIN_TESTS ||
        qualification->tests_passed !=
            qualification->tests_executed ||
        qualification->tests_failed != 0 ||
        qualification->tests_passed +
            qualification->tests_failed !=
            qualification->tests_executed ||
        !qualification->negative_test_executed ||
        !qualification->negative_test_passed
    )
    {
        return
            RICTUS_MODULE_ERR_QUALIFICATION;
    }


    previous =
        record->state;


    record->qualification =
        *qualification;


    /*
     * Qualification never restores activation
     * authority.
     */

    record->activation_authorized =
        0;


    record->state =
        RICTUS_MODULE_STATE_QUALIFIED;


    audit_result =
        rictus_module_audit(
            registry,
            record,
            RICTUS_MODULE_AUDIT_QUALIFIED,
            previous,
            RICTUS_MODULE_OK
        );


    if (
        audit_result !=
        RICTUS_MODULE_OK
    )
    {
        record->state =
            previous;


        memset(
            &record->qualification,
            0,
            sizeof(record->qualification)
        );


        return
            audit_result;
    }


    return
        RICTUS_MODULE_OK;
}


/*
 * ------------------------------------------------
 * AUTHORIZE ACTIVATION
 * ------------------------------------------------
 */

rictus_module_result_t rictus_module_registry_authorize_activation(
    rictus_module_registry_t *registry,
    const char *module_id
)
{
    rictus_module_record_t *record;

    rictus_module_result_t audit_result;


    record =
        rictus_module_registry_find_mutable(
            registry,
            module_id
        );


    if (
        record == NULL
    )
    {
        return
            RICTUS_MODULE_ERR_NOT_FOUND;
    }


    if (
        record->state ==
        RICTUS_MODULE_STATE_QUARANTINED
    )
    {
        return
            RICTUS_MODULE_ERR_QUARANTINED;
    }


    if (
        record->state !=
        RICTUS_MODULE_STATE_QUALIFIED
    )
    {
        return
            RICTUS_MODULE_ERR_NOT_QUALIFIED;
    }


    record->activation_authorized =
        1;


    audit_result =
        rictus_module_audit(
            registry,
            record,
            RICTUS_MODULE_AUDIT_AUTHORIZED,
            record->state,
            RICTUS_MODULE_OK
        );


    if (
        audit_result !=
        RICTUS_MODULE_OK
    )
    {
        record->activation_authorized =
            0;


        return
            audit_result;
    }


    return
        RICTUS_MODULE_OK;
}


/*
 * ------------------------------------------------
 * ACTIVATE
 * ------------------------------------------------
 */

rictus_module_result_t rictus_module_registry_activate(
    rictus_module_registry_t *registry,
    const char *module_id
)
{
    rictus_module_record_t *record;

    rictus_module_state_t previous;

    rictus_module_result_t audit_result;


    record =
        rictus_module_registry_find_mutable(
            registry,
            module_id
        );


    if (
        record == NULL
    )
    {
        return
            RICTUS_MODULE_ERR_NOT_FOUND;
    }


    if (
        record->state ==
        RICTUS_MODULE_STATE_QUARANTINED
    )
    {
        return
            RICTUS_MODULE_ERR_QUARANTINED;
    }


    if (
        record->state !=
        RICTUS_MODULE_STATE_QUALIFIED
    )
    {
        return
            RICTUS_MODULE_ERR_NOT_QUALIFIED;
    }


    if (
        !record->activation_authorized
    )
    {
        return
            RICTUS_MODULE_ERR_NOT_AUTHORIZED;
    }


    previous =
        record->state;


    record->state =
        RICTUS_MODULE_STATE_ACTIVE;


    audit_result =
        rictus_module_audit(
            registry,
            record,
            RICTUS_MODULE_AUDIT_ACTIVE,
            previous,
            RICTUS_MODULE_OK
        );


    if (
        audit_result !=
        RICTUS_MODULE_OK
    )
    {
        record->state =
            previous;


        return
            audit_result;
    }


    return
        RICTUS_MODULE_OK;
}


/*
 * ------------------------------------------------
 * FAIL
 * ------------------------------------------------
 */

rictus_module_result_t rictus_module_registry_fail(
    rictus_module_registry_t *registry,
    const char *module_id
)
{
    rictus_module_record_t *record;

    rictus_module_state_t previous;


    record =
        rictus_module_registry_find_mutable(
            registry,
            module_id
        );


    if (
        record == NULL
    )
    {
        return
            RICTUS_MODULE_ERR_NOT_FOUND;
    }


    previous =
        record->state;


    record->state =
        RICTUS_MODULE_STATE_FAILED;


    record->activation_authorized =
        0;


    return
        rictus_module_audit(
            registry,
            record,
            RICTUS_MODULE_AUDIT_FAILED,
            previous,
            RICTUS_MODULE_OK
        );
}


/*
 * ------------------------------------------------
 * QUARANTINE
 * ------------------------------------------------
 */

rictus_module_result_t rictus_module_registry_quarantine(
    rictus_module_registry_t *registry,
    const char *module_id
)
{
    rictus_module_record_t *record;

    rictus_module_state_t previous;


    record =
        rictus_module_registry_find_mutable(
            registry,
            module_id
        );


    if (
        record == NULL
    )
    {
        return
            RICTUS_MODULE_ERR_NOT_FOUND;
    }


    previous =
        record->state;


    record->state =
        RICTUS_MODULE_STATE_QUARANTINED;


    record->activation_authorized =
        0;


    return
        rictus_module_audit(
            registry,
            record,
            RICTUS_MODULE_AUDIT_QUARANTINED,
            previous,
            RICTUS_MODULE_OK
        );
}


/*
 * ------------------------------------------------
 * READ-ONLY LOOKUP
 * ------------------------------------------------
 */

const rictus_module_record_t *rictus_module_registry_find(
    const rictus_module_registry_t *registry,
    const char *module_id
)
{
    size_t index;


    if (
        registry == NULL ||
        module_id == NULL
    )
    {
        return NULL;
    }


    for (
        index = 0;
        index < registry->count;
        ++index
    )
    {
        if (
            strcmp(
                registry
                    ->modules[index]
                    .descriptor
                    .id,
                module_id
            ) == 0
        )
        {
            return
                &registry->modules[index];
        }
    }


    return NULL;
}