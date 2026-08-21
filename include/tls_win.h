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

    /*
     * Serializes every TLS application-data
     * transmission that uses this Schannel context.
     *
     * Schannel permits concurrent encrypt/decrypt,
     * but multiple simultaneous EncryptMessage()
     * calls on one context are not permitted.
     *
     * The lock therefore protects the complete
     * QueryContextAttributes -> EncryptMessage ->
     * socket send sequence for one TLS record.
     */
    CRITICAL_SECTION send_lock;
    int send_lock_initialized;

    unsigned char recv_buffer[RICTUS_TLS_RECV_BUFFER];
    int recv_buffer_len;

} rictus_tls;


/*
 * Initializes the TLS object, its transmit lock,
 * and the outbound Schannel credential handle.
 *
 * Returns 1 on success and 0 on failure.
 */
int tls_init(
    rictus_tls* tls
);


/*
 * Establishes the Schannel TLS security context on
 * an already-connected socket.
 *
 * This function is expected to run before normal
 * multi-threaded application transmission begins.
 *
 * Returns 1 on success and 0 on failure.
 */
int tls_handshake(
    rictus_tls* tls,
    SOCKET socket,
    const char* hostname
);


/*
 * Encrypts and transmits one application-data
 * record.
 *
 * Calls are serialized per rictus_tls object so
 * multiple Core/module threads cannot concurrently
 * encrypt or interleave TLS records on the socket.
 *
 * Receive/decrypt remains independent.
 *
 * Returns 1 on success and 0 on failure.
 */
int tls_send(
    rictus_tls* tls,
    SOCKET socket,
    const char* data,
    int data_len
);


/*
 * Receives and decrypts TLS application data.
 *
 * Returns:
 * - positive plaintext byte count on success;
 * - 0 when the peer closes/expires the context;
 * - -1 on failure.
 */
int tls_recv(
    rictus_tls* tls,
    SOCKET socket,
    char* output,
    int output_size
);


/*
 * Releases the Schannel security context,
 * credentials, and transmit lock.
 *
 * All worker threads using this TLS object must be
 * stopped before cleanup is called.
 */
void tls_cleanup(
    rictus_tls* tls
);

#endif
