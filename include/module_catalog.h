#ifndef RICTUS_MODULE_CATALOG_H
#define RICTUS_MODULE_CATALOG_H

#include <stddef.h>

#include "module.h"


#define RICTUS_MODULE_CATALOG_MAX \
    32


typedef struct
{
    const rictus_module_descriptor_t *
        entries[
            RICTUS_MODULE_CATALOG_MAX
        ];

    size_t count;

} rictus_module_catalog_t;


void rictus_module_catalog_init(
    rictus_module_catalog_t *catalog
);


rictus_module_result_t rictus_module_catalog_register(
    rictus_module_catalog_t *catalog,
    const rictus_module_descriptor_t *descriptor
);


const rictus_module_descriptor_t *rictus_module_catalog_find(
    const rictus_module_catalog_t *catalog,
    const char *module_id
);


#endif