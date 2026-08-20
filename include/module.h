#ifndef RICTUS_MODULE_H
#define RICTUS_MODULE_H


#define RICTUS_MODULE_ID_MAX \
    64

#define RICTUS_MODULE_NAME_MAX \
    64

#define RICTUS_MODULE_MIN_TESTS \
    10


/*
 * ------------------------------------------------
 * CORE MODULE API
 * ------------------------------------------------
 */

#define RICTUS_MODULE_API_MAJOR \
    1

#define RICTUS_MODULE_API_MINOR \
    2


/*
 * ------------------------------------------------
 * MODULE LIFECYCLE
 * ------------------------------------------------
 */

typedef enum
{
    RICTUS_MODULE_STATE_DISCOVERED = 0,

    RICTUS_MODULE_STATE_UNVERIFIED,

    RICTUS_MODULE_STATE_TESTING,

    RICTUS_MODULE_STATE_QUALIFIED,

    RICTUS_MODULE_STATE_ACTIVE,

    RICTUS_MODULE_STATE_FAILED,

    RICTUS_MODULE_STATE_QUARANTINED

} rictus_module_state_t;


/*
 * ------------------------------------------------
 * MODULE RESULTS
 * ------------------------------------------------
 */

typedef enum
{
    RICTUS_MODULE_OK = 0,

    RICTUS_MODULE_ERR_INVALID_ARGUMENT,

    RICTUS_MODULE_ERR_INVALID_IDENTITY,

    RICTUS_MODULE_ERR_DUPLICATE,

    RICTUS_MODULE_ERR_REGISTRY_FULL,

    RICTUS_MODULE_ERR_NOT_FOUND,

    RICTUS_MODULE_ERR_INCOMPATIBLE,

    RICTUS_MODULE_ERR_INVALID_STATE,

    RICTUS_MODULE_ERR_QUALIFICATION,

    RICTUS_MODULE_ERR_NOT_QUALIFIED,

    RICTUS_MODULE_ERR_NOT_AUTHORIZED,

    RICTUS_MODULE_ERR_QUARANTINED,

    RICTUS_MODULE_ERR_AUDIT_FULL,

    RICTUS_MODULE_ERR_START_FAILED,

    RICTUS_MODULE_ERR_STOP_FAILED

} rictus_module_result_t;


/*
 * ------------------------------------------------
 * QUALIFICATION RESULT
 * ------------------------------------------------
 */

typedef struct
{
    unsigned int tests_executed;

    unsigned int tests_passed;

    unsigned int tests_failed;

    int negative_test_executed;

    int negative_test_passed;

} rictus_module_qualification_result_t;


/*
 * ------------------------------------------------
 * CORE HOST API
 * ------------------------------------------------
 *
 * Modules do not receive direct access to IRC
 * sockets, TLS state, or Core internals.
 *
 * Core exposes only approved operations.
 */

typedef int
(*rictus_module_send_message_fn)(
    const char *message
);


typedef struct
{
    rictus_module_send_message_fn
        send_message;

} rictus_module_host_t;


/*
 * ------------------------------------------------
 * MODULE CALLBACKS
 * ------------------------------------------------
 */

typedef rictus_module_result_t
(*rictus_module_qualify_fn)(
    rictus_module_qualification_result_t *result
);


typedef rictus_module_result_t
(*rictus_module_start_fn)(
    const rictus_module_host_t *host
);


typedef rictus_module_result_t
(*rictus_module_stop_fn)(void);


/*
 * ------------------------------------------------
 * MODULE DESCRIPTOR
 * ------------------------------------------------
 */

typedef struct
{
    char id[
        RICTUS_MODULE_ID_MAX
    ];

    char name[
        RICTUS_MODULE_NAME_MAX
    ];


    unsigned int version_major;

    unsigned int version_minor;

    unsigned int version_patch;


    unsigned int required_core_api_major;

    unsigned int required_core_api_minor;


    rictus_module_qualify_fn qualify;

    rictus_module_start_fn start;

    rictus_module_stop_fn stop;

} rictus_module_descriptor_t;


const char *
rictus_module_state_string(
    rictus_module_state_t state
);


const char *
rictus_module_result_string(
    rictus_module_result_t result
);


#endif