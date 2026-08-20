#ifndef RICTUS_INTELLIGENCE_H
#define RICTUS_INTELLIGENCE_H

#include "module.h"


#define RICTUS_INTELLIGENCE_ID \
    "intelligence"

#define RICTUS_INTELLIGENCE_NAME \
    "Rictus Intelligence"

#define RICTUS_INTELLIGENCE_VERSION_MAJOR \
    0

#define RICTUS_INTELLIGENCE_VERSION_MINOR \
    9

#define RICTUS_INTELLIGENCE_VERSION_PATCH \
    0


typedef enum
{
    RICTUS_INTELLIGENCE_SOURCE_INVALID = 0,

    RICTUS_INTELLIGENCE_SOURCE_PRIMARY,

    RICTUS_INTELLIGENCE_SOURCE_SECONDARY

} rictus_intelligence_source_class_t;


typedef enum
{
    RICTUS_INTELLIGENCE_SOURCE_NONE = 0,

    RICTUS_INTELLIGENCE_SOURCE_NASA,

    RICTUS_INTELLIGENCE_SOURCE_SPACEX,

    RICTUS_INTELLIGENCE_SOURCE_OTHER

} rictus_intelligence_source_t;


rictus_intelligence_source_t
rictus_intelligence_source_from_name(
    const char *name
);


rictus_intelligence_source_class_t
rictus_intelligence_source_class(
    rictus_intelligence_source_t source
);


const char *
rictus_intelligence_source_string(
    rictus_intelligence_source_t source
);


const char *
rictus_intelligence_source_class_string(
    rictus_intelligence_source_class_t source_class
);


extern const rictus_module_descriptor_t
    rictus_intelligence_descriptor;


__declspec(dllexport)
const rictus_module_descriptor_t *
rictus_module_get_descriptor(void);


#endif