/*
 * STN-LABZ
 * Rictus Intelligence Module
 *
 * sources.c
 *
 * Approved Intelligence source definitions.
 *
 * Primary sources:
 *
 * - NASA
 * - SpaceX
 *
 * Source configuration is deterministic and
 * module-owned.
 */

#include <string.h>

#include "sources.h"


/*
 * ------------------------------------------------
 * APPROVED SOURCES
 * ------------------------------------------------
 *
 * HTTPS is mandatory.
 *
 * NASA:
 * Official NASA RSS content.
 *
 * SpaceX:
 * Official SpaceX Updates page.
 */

static const
rictus_intelligence_source_definition_t
g_sources[] =
{
    {
        "nasa_news",

        "NASA",

        "www.nasa.gov",

        "/rss/dyn/breaking_news.rss",

        RICTUS_INTELLIGENCE_TRANSPORT_RSS,

        1
    },

    {
        "spacex_updates",

        "SpaceX",

        "www.spacex.com",

        "/updates",

        RICTUS_INTELLIGENCE_TRANSPORT_HTML,

        1
    },
	
	{
    "nist_csrc",

    "NIST CSRC",

    "csrc.nist.gov",

    "/news?ipp-sm=100&sortBy-sm=NewsDateTime+DESC&topicsMatch-sm=ANY",

    RICTUS_INTELLIGENCE_TRANSPORT_HTML,

    1
}
};


size_t
rictus_intelligence_source_count(void)
{
    return
        sizeof(g_sources) /
        sizeof(g_sources[0]);
}


const rictus_intelligence_source_definition_t *
rictus_intelligence_source_get(
    size_t index
)
{
    if (
        index >=
        rictus_intelligence_source_count()
    )
    {
        return NULL;
    }


    return
        &g_sources[index];
}


const rictus_intelligence_source_definition_t *
rictus_intelligence_source_find(
    const char *id
)
{
    size_t index;


    if (
        id == NULL ||
        id[0] == '\0'
    )
    {
        return NULL;
    }


    for (
        index = 0;
        index <
            rictus_intelligence_source_count();
        ++index
    )
    {
        if (
            strcmp(
                g_sources[index].id,
                id
            ) == 0
        )
        {
            return
                &g_sources[index];
        }
    }


    return NULL;
}