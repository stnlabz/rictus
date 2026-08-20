#ifndef RICTUS_INTELLIGENCE_ITEM_H
#define RICTUS_INTELLIGENCE_ITEM_H

#include <stddef.h>


#define RICTUS_INTELLIGENCE_ITEM_SOURCE_MAX \
    64

#define RICTUS_INTELLIGENCE_ITEM_TITLE_MAX \
    512

#define RICTUS_INTELLIGENCE_ITEM_URL_MAX \
    1024

#define RICTUS_INTELLIGENCE_ITEM_DATE_MAX \
    128

#define RICTUS_INTELLIGENCE_ITEM_SUMMARY_MAX \
    2048

#define RICTUS_INTELLIGENCE_ITEM_CONTENT_MAX \
    16384

#define RICTUS_INTELLIGENCE_ITEM_FINGERPRINT_MAX \
    32


typedef struct
{
    char source[
        RICTUS_INTELLIGENCE_ITEM_SOURCE_MAX
    ];

    char title[
        RICTUS_INTELLIGENCE_ITEM_TITLE_MAX
    ];

    char url[
        RICTUS_INTELLIGENCE_ITEM_URL_MAX
    ];

    char published[
        RICTUS_INTELLIGENCE_ITEM_DATE_MAX
    ];

    char summary[
        RICTUS_INTELLIGENCE_ITEM_SUMMARY_MAX
    ];

    char content[
        RICTUS_INTELLIGENCE_ITEM_CONTENT_MAX
    ];

    char fingerprint[
        RICTUS_INTELLIGENCE_ITEM_FINGERPRINT_MAX
    ];

} rictus_intelligence_item_t;


#endif