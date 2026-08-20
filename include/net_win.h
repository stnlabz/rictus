#ifndef RICTUS_NET_WIN_H
#define RICTUS_NET_WIN_H

#include <winsock2.h>

typedef struct
{
    SOCKET socket;
} rictus_connection;

int net_init(void);
void net_cleanup(void);

int net_connect_tcp(
    rictus_connection* conn,
    const char* host,
    const char* port
);

void net_close(rictus_connection* conn);

#endif