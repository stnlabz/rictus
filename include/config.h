#ifndef RICTUS_CONFIG_H
#define RICTUS_CONFIG_H

#define RICTUS_CONFIG_VALUE_MAX 256

typedef struct
{
    char irc_server[RICTUS_CONFIG_VALUE_MAX];
    char irc_port[RICTUS_CONFIG_VALUE_MAX];
    char irc_nick[RICTUS_CONFIG_VALUE_MAX];
    char irc_account[RICTUS_CONFIG_VALUE_MAX];
    char irc_password[RICTUS_CONFIG_VALUE_MAX];
    char irc_channel[RICTUS_CONFIG_VALUE_MAX];

} rictus_config;

int config_load(
    const char* path,
    rictus_config* config
);

#endif