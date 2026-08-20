#ifndef RICTUS_TLS_WIN_H
#define RICTUS_TLS_WIN_H

#define SECURITY_WIN32
#define WIN32_LEAN_AND_MEAN

#include <winsock2.h>
#include <windows.h>
#include <security.h>
#include <schannel.h>

#define RICTUS_TLS_RECV_BUFFER 65536

typedef struct
{
    CredHandle credentials;
    CtxtHandle context;

    int credentials_valid;
    int context_valid;

    unsigned char recv_buffer[RICTUS_TLS_RECV_BUFFER];
    int recv_buffer_len;

} rictus_tls;

int tls_init(
    rictus_tls* tls
);

int tls_handshake(
    rictus_tls* tls,
    SOCKET socket,
    const char* hostname
);

int tls_send(
    rictus_tls* tls,
    SOCKET socket,
    const char* data,
    int data_len
);

int tls_recv(
    rictus_tls* tls,
    SOCKET socket,
    char* output,
    int output_size
);

void tls_cleanup(
    rictus_tls* tls
);

#endif