/*
 * STN-LABZ
 * Rictus Intelligence Module
 *
 * parser.c
 *
 * Normalization layer.
 *
 * Responsibilities:
 *
 * - convert collected source material into bounded
 *   normalized Intelligence items
 * - preserve source identity
 * - preserve source URL
 * - preserve publication metadata where available
 * - generate deterministic non-security fingerprints
 *
 * Fingerprints are used only for local duplicate
 * detection.
 *
 * They are not Trust Chain hashes and are not used
 * as authoritative integrity evidence.
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "parser.h"


/*
 * ------------------------------------------------
 * FNV-1A 64-BIT FINGERPRINT
 * ------------------------------------------------
 */

static uint64_t
rictus_intelligence_fnv1a(
    const unsigned char *data,
    size_t length,
    uint64_t hash
)
{
    size_t index;


    if (
        data == NULL
    )
    {
        return hash;
    }


    for (
        index = 0;
        index < length;
        ++index
    )
    {
        hash ^=
            (uint64_t)data[index];

        hash *=
            UINT64_C(
                1099511628211
            );
    }


    return hash;
}


static void
rictus_intelligence_item_fingerprint(
    rictus_intelligence_item_t *item
)
{
    uint64_t hash;


    if (
        item == NULL
    )
    {
        return;
    }


    hash =
        UINT64_C(
            14695981039346656037
        );


    hash =
        rictus_intelligence_fnv1a(
            (const unsigned char *)
            item->source,
            strlen(
                item->source
            ),
            hash
        );


    hash =
        rictus_intelligence_fnv1a(
            (const unsigned char *)
            item->title,
            strlen(
                item->title
            ),
            hash
        );


    hash =
        rictus_intelligence_fnv1a(
            (const unsigned char *)
            item->url,
            strlen(
                item->url
            ),
            hash
        );


    hash =
        rictus_intelligence_fnv1a(
            (const unsigned char *)
            item->published,
            strlen(
                item->published
            ),
            hash
        );


    (void)
    snprintf(
        item->fingerprint,
        sizeof(item->fingerprint),
        "%016llx",
        (unsigned long long)
        hash
    );
}


/*
 * ------------------------------------------------
 * BOUNDED COPY
 * ------------------------------------------------
 */

static int
rictus_intelligence_copy(
    char *destination,
    size_t destination_size,
    const char *source,
    size_t source_length
)
{
    if (
        destination == NULL ||
        destination_size == 0 ||
        source == NULL
    )
    {
        return 0;
    }


    if (
        source_length >=
        destination_size
    )
    {
        source_length =
            destination_size - 1;
    }


    memcpy(
        destination,
        source,
        source_length
    );


    destination[
        source_length
    ] =
        '\0';


    return 1;
}


/*
 * ------------------------------------------------
 * TRIM
 * ------------------------------------------------
 */

static void
rictus_intelligence_trim(
    char *value
)
{
    char *start;

    size_t length;


    if (
        value == NULL
    )
    {
        return;
    }


    start =
        value;


    while (
        *start == ' ' ||
        *start == '\t' ||
        *start == '\r' ||
        *start == '\n'
    )
    {
        ++start;
    }


    if (
        start != value
    )
    {
        memmove(
            value,
            start,
            strlen(start) + 1
        );
    }


    length =
        strlen(
            value
        );


    while (
        length > 0 &&
        (
            value[length - 1] == ' ' ||
            value[length - 1] == '\t' ||
            value[length - 1] == '\r' ||
            value[length - 1] == '\n'
        )
    )
    {
        value[
            length - 1
        ] =
            '\0';

        --length;
    }
}


/*
 * ------------------------------------------------
 * STRIP CDATA
 * ------------------------------------------------
 */

static void
rictus_intelligence_strip_cdata(
    char *value
)
{
    size_t length;


    if (
        value == NULL
    )
    {
        return;
    }


    length =
        strlen(
            value
        );


    if (
        length >= 12 &&
        strncmp(
            value,
            "<![CDATA[",
            9
        ) == 0 &&
        strcmp(
            value + length - 3,
            "]]>"
        ) == 0
    )
    {
        memmove(
            value,
            value + 9,
            length - 12
        );


        value[
            length - 12
        ] =
            '\0';
    }
}


/*
 * ------------------------------------------------
 * SIMPLE ENTITY DECODING
 * ------------------------------------------------
 */

static void
rictus_intelligence_decode_entities(
    char *value
)
{
    char output[
        RICTUS_INTELLIGENCE_ITEM_SUMMARY_MAX
    ];

    size_t input_index =
        0;

    size_t output_index =
        0;


    if (
        value == NULL
    )
    {
        return;
    }


    memset(
        output,
        0,
        sizeof(output)
    );


    while (
        value[input_index] != '\0' &&
        output_index <
            sizeof(output) - 1
    )
    {
        if (
            strncmp(
                value + input_index,
                "&amp;",
                5
            ) == 0
        )
        {
            output[
                output_index++
            ] =
                '&';

            input_index +=
                5;

            continue;
        }


        if (
            strncmp(
                value + input_index,
                "&lt;",
                4
            ) == 0
        )
        {
            output[
                output_index++
            ] =
                '<';

            input_index +=
                4;

            continue;
        }


        if (
            strncmp(
                value + input_index,
                "&gt;",
                4
            ) == 0
        )
        {
            output[
                output_index++
            ] =
                '>';

            input_index +=
                4;

            continue;
        }


        if (
            strncmp(
                value + input_index,
                "&quot;",
                6
            ) == 0
        )
        {
            output[
                output_index++
            ] =
                '"';

            input_index +=
                6;

            continue;
        }


        if (
            strncmp(
                value + input_index,
                "&#39;",
                5
            ) == 0
        )
        {
            output[
                output_index++
            ] =
                '\'';

            input_index +=
                5;

            continue;
        }


        output[
            output_index++
        ] =
            value[
                input_index++
            ];
    }


    output[
        output_index
    ] =
        '\0';


    (void)
    rictus_intelligence_copy(
        value,
        RICTUS_INTELLIGENCE_ITEM_SUMMARY_MAX,
        output,
        strlen(output)
    );
}


/*
 * ------------------------------------------------
 * REMOVE HTML TAGS
 * ------------------------------------------------
 */

static void
rictus_intelligence_strip_tags(
    char *value
)
{
    char output[
        RICTUS_INTELLIGENCE_ITEM_SUMMARY_MAX
    ];

    size_t input_index =
        0;

    size_t output_index =
        0;

    int inside_tag =
        0;


    if (
        value == NULL
    )
    {
        return;
    }


    memset(
        output,
        0,
        sizeof(output)
    );


    while (
        value[input_index] != '\0' &&
        output_index <
            sizeof(output) - 1
    )
    {
        char c;


        c =
            value[
                input_index++
            ];


        if (
            c == '<'
        )
        {
            inside_tag =
                1;

            continue;
        }


        if (
            c == '>'
        )
        {
            inside_tag =
                0;

            continue;
        }


        if (
            !inside_tag
        )
        {
            output[
                output_index++
            ] =
                c;
        }
    }


    output[
        output_index
    ] =
        '\0';


    (void)
    rictus_intelligence_copy(
        value,
        RICTUS_INTELLIGENCE_ITEM_SUMMARY_MAX,
        output,
        strlen(output)
    );


    rictus_intelligence_trim(
        value
    );
}


/*
 * ------------------------------------------------
 * XML FIELD
 * ------------------------------------------------
 */

static int
rictus_intelligence_xml_field(
    const char *begin,
    const char *end,
    const char *tag,
    char *output,
    size_t output_size
)
{
    char open_tag[
        64
    ];

    char close_tag[
        64
    ];

    const char *start;

    const char *finish;

    int written;


    if (
        begin == NULL ||
        end == NULL ||
        tag == NULL ||
        output == NULL ||
        output_size == 0 ||
        end <= begin
    )
    {
        return 0;
    }


    output[0] =
        '\0';


    written =
        snprintf(
            open_tag,
            sizeof(open_tag),
            "<%s>",
            tag
        );


    if (
        written <= 0 ||
        (size_t)written >=
            sizeof(open_tag)
    )
    {
        return 0;
    }


    written =
        snprintf(
            close_tag,
            sizeof(close_tag),
            "</%s>",
            tag
        );


    if (
        written <= 0 ||
        (size_t)written >=
            sizeof(close_tag)
    )
    {
        return 0;
    }


    start =
        strstr(
            begin,
            open_tag
        );


    if (
        start == NULL ||
        start >= end
    )
    {
        return 0;
    }


    start +=
        strlen(
            open_tag
        );


    finish =
        strstr(
            start,
            close_tag
        );


    if (
        finish == NULL ||
        finish > end
    )
    {
        return 0;
    }


    if (
        !rictus_intelligence_copy(
            output,
            output_size,
            start,
            (size_t)
            (
                finish -
                start
            )
        )
    )
    {
        return 0;
    }


    rictus_intelligence_trim(
        output
    );


    rictus_intelligence_strip_cdata(
        output
    );


    rictus_intelligence_decode_entities(
        output
    );


    return 1;
}


/*
 * ------------------------------------------------
 * NASA RSS
 * ------------------------------------------------
 */

static
rictus_intelligence_parse_result_t
rictus_intelligence_parse_rss(
    const rictus_intelligence_source_definition_t *source,
    const rictus_intelligence_response_t *response,
    rictus_intelligence_item_set_t *items
)
{
    const char *cursor;

    const char *document_end;


    cursor =
        response->body;


    document_end =
        response->body +
        response->body_length;


    while (
        cursor <
        document_end
    )
    {
        const char *item_begin;

        const char *item_end;

        rictus_intelligence_item_t
            *item;


        item_begin =
            strstr(
                cursor,
                "<item"
            );


        if (
            item_begin == NULL ||
            item_begin >=
                document_end
        )
        {
            break;
        }


        item_begin =
            strchr(
                item_begin,
                '>'
            );


        if (
            item_begin == NULL ||
            item_begin >=
                document_end
        )
        {
            return
                RICTUS_INTELLIGENCE_PARSE_INVALID_FORMAT;
        }


        ++item_begin;


        item_end =
            strstr(
                item_begin,
                "</item>"
            );


        if (
            item_end == NULL ||
            item_end >
                document_end
        )
        {
            return
                RICTUS_INTELLIGENCE_PARSE_INVALID_FORMAT;
        }


        if (
            items->count >=
            RICTUS_INTELLIGENCE_ITEMS_MAX
        )
        {
            return
                RICTUS_INTELLIGENCE_PARSE_ITEM_LIMIT;
        }


        item =
            &items->items[
                items->count
            ];


        memset(
            item,
            0,
            sizeof(*item)
        );


        (void)
        rictus_intelligence_copy(
            item->source,
            sizeof(item->source),
            source->name,
            strlen(source->name)
        );


        (void)
        rictus_intelligence_xml_field(
            item_begin,
            item_end,
            "title",
            item->title,
            sizeof(item->title)
        );


        (void)
        rictus_intelligence_xml_field(
            item_begin,
            item_end,
            "link",
            item->url,
            sizeof(item->url)
        );


        (void)
        rictus_intelligence_xml_field(
            item_begin,
            item_end,
            "pubDate",
            item->published,
            sizeof(item->published)
        );


        (void)
        rictus_intelligence_xml_field(
            item_begin,
            item_end,
            "description",
            item->summary,
            sizeof(item->summary)
        );


        rictus_intelligence_strip_tags(
            item->summary
        );


        /*
         * Title and URL form the minimum useful
         * normalized RSS intelligence object.
         */

        if (
            item->title[0] != '\0' &&
            item->url[0] != '\0'
        )
        {
            rictus_intelligence_item_fingerprint(
                item
            );


            items->count++;
        }


        cursor =
            item_end +
            strlen(
                "</item>"
            );
    }


    if (
        items->count == 0
    )
    {
        return
            RICTUS_INTELLIGENCE_PARSE_INVALID_FORMAT;
    }


    return
        RICTUS_INTELLIGENCE_PARSE_OK;
}


/*
 * ------------------------------------------------
 * SOURCE DOCUMENT
 * ------------------------------------------------
 *
 * Used for HTML sources until a source-specific
 * article parser is qualified.
 *
 * A deterministic fingerprint of the complete
 * response detects changes to the official source
 * document without pretending the HTML has already
 * been semantically understood.
 */

static
rictus_intelligence_parse_result_t
rictus_intelligence_parse_document(
    const rictus_intelligence_source_definition_t *source,
    const rictus_intelligence_response_t *response,
    rictus_intelligence_item_set_t *items
)
{
    rictus_intelligence_item_t
        *item;

    uint64_t hash;


    if (
        items->count >=
        RICTUS_INTELLIGENCE_ITEMS_MAX
    )
    {
        return
            RICTUS_INTELLIGENCE_PARSE_ITEM_LIMIT;
    }


    item =
        &items->items[
            items->count
        ];


    memset(
        item,
        0,
        sizeof(*item)
    );


    (void)
    rictus_intelligence_copy(
        item->source,
        sizeof(item->source),
        source->name,
        strlen(source->name)
    );


    (void)
    snprintf(
        item->title,
        sizeof(item->title),
        "%s official updates",
        source->name
    );


    (void)
    snprintf(
        item->url,
        sizeof(item->url),
        "https://%s%s",
        source->host,
        source->path
    );


    hash =
        UINT64_C(
            14695981039346656037
        );


    hash =
        rictus_intelligence_fnv1a(
            (const unsigned char *)
            response->body,
            response->body_length,
            hash
        );


    (void)
    snprintf(
        item->fingerprint,
        sizeof(item->fingerprint),
        "%016llx",
        (unsigned long long)
        hash
    );


    items->count++;


    return
        RICTUS_INTELLIGENCE_PARSE_OK;
}


/*
 * ------------------------------------------------
 * PUBLIC PARSER
 * ------------------------------------------------
 */

rictus_intelligence_parse_result_t
rictus_intelligence_parse_response(
    const rictus_intelligence_source_definition_t *source,
    const rictus_intelligence_response_t *response,
    rictus_intelligence_item_set_t *items
)
{
    if (
        source == NULL ||
        response == NULL ||
        items == NULL ||
        response->body == NULL ||
        response->body_length == 0
    )
    {
        return
            RICTUS_INTELLIGENCE_PARSE_INVALID_ARGUMENT;
    }


    memset(
        items,
        0,
        sizeof(*items)
    );


    switch (
        source->transport
    )
    {
        case RICTUS_INTELLIGENCE_TRANSPORT_RSS:

            return
                rictus_intelligence_parse_rss(
                    source,
                    response,
                    items
                );


        case RICTUS_INTELLIGENCE_TRANSPORT_HTML:

            return
                rictus_intelligence_parse_document(
                    source,
                    response,
                    items
                );


        default:

            return
                RICTUS_INTELLIGENCE_PARSE_UNSUPPORTED;
    }
}


/*
 * ------------------------------------------------
 * RESULT STRING
 * ------------------------------------------------
 */

const char *
rictus_intelligence_parse_result_string(
    rictus_intelligence_parse_result_t result
)
{
    switch (
        result
    )
    {
        case RICTUS_INTELLIGENCE_PARSE_OK:

            return "OK";


        case RICTUS_INTELLIGENCE_PARSE_INVALID_ARGUMENT:

            return "INVALID_ARGUMENT";


        case RICTUS_INTELLIGENCE_PARSE_UNSUPPORTED:

            return "UNSUPPORTED";


        case RICTUS_INTELLIGENCE_PARSE_INVALID_FORMAT:

            return "INVALID_FORMAT";


        case RICTUS_INTELLIGENCE_PARSE_ITEM_LIMIT:

            return "ITEM_LIMIT";


        default:

            return "UNKNOWN";
    }
}