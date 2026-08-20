#ifndef RICTUS_SASL_H
#define RICTUS_SASL_H

#include "config.h"

int sasl_build_plain(
    const rictus_config* config,
    char* output,
    int output_size
);

#endif