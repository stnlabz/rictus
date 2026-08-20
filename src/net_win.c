#define WIN32_LEAN_AND_MEAN

#include <winsock2.h>
#include <ws2tcpip.h>

#include <stdio.h>
#include <string.h>

#include "net_win.h"

#pragma comment(lib, "Ws2_32.lib")

int net_init(void)
{
    WSADATA wsa;

    int rc = WSAStartup(
        MAKEWORD(2, 2),
        &wsa
    );

    if (rc != 0)
    {
        fprintf(
            stderr,
            "WSAStartup failed: %d\n",
            rc
        );

        return 0;
    }

    return 1;
}

void net_cleanup(void)
{
    WSACleanup();
}

int net_connect_tcp(
    rictus_connection* conn,
    const char* host,
    const char* port
)
{
    struct addrinfo hints;
    struct addrinfo* result = NULL;
    struct addrinfo* ptr = NULL;

    int rc;

    memset(&hints, 0, sizeof(hints));

    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    rc = getaddrinfo(
        host,
        port,
        &hints,
        &result
    );

    if (rc != 0)
    {
        fprintf(
            stderr,
            "getaddrinfo failed: %d\n",
            rc
        );

        return 0;
    }

    conn->socket = INVALID_SOCKET;

    for (
        ptr = result;
        ptr != NULL;
        ptr = ptr->ai_next
        )
    {
        SOCKET s = socket(
            ptr->ai_family,
            ptr->ai_socktype,
            ptr->ai_protocol
        );

        if (s == INVALID_SOCKET)
            continue;

        if (
            connect(
                s,
                ptr->ai_addr,
                (int)ptr->ai_addrlen
            ) == 0
            )
        {
            conn->socket = s;
            break;
        }

        closesocket(s);
    }

    freeaddrinfo(result);

    if (conn->socket == INVALID_SOCKET)
    {
        fprintf(
            stderr,
            "Unable to connect to %s:%s\n",
            host,
            port
        );

        return 0;
    }

    return 1;
}

void net_close(
    rictus_connection* conn
)
{
    if (
        conn != NULL &&
        conn->socket != INVALID_SOCKET
        )
    {
        closesocket(conn->socket);
        conn->socket = INVALID_SOCKET;
    }
}