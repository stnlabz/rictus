#include "tls_win.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#pragma comment(lib, "Secur32.lib")

#define TLS_BUFFER_SIZE 16384

int tls_init(
    rictus_tls* tls
)
{
    SECURITY_STATUS status;
    TimeStamp expiry;

    SCHANNEL_CRED credentials;

    memset(
        tls,
        0,
        sizeof(*tls)
    );

    memset(
        &credentials,
        0,
        sizeof(credentials)
    );

    credentials.dwVersion =
        SCHANNEL_CRED_VERSION;

    /*
     * Strong cryptography.
     *
     * Do not automatically select a client certificate.
     */
    credentials.dwFlags =
        SCH_USE_STRONG_CRYPTO |
        SCH_CRED_NO_DEFAULT_CREDS;

    status = AcquireCredentialsHandleA(
        NULL,
        UNISP_NAME_A,
        SECPKG_CRED_OUTBOUND,
        NULL,
        &credentials,
        NULL,
        NULL,
        &tls->credentials,
        &expiry
    );

    if (status != SEC_E_OK)
    {
        fprintf(
            stderr,
            "AcquireCredentialsHandle failed: 0x%08lx\n",
            (unsigned long)status
        );

        return 0;
    }

    tls->credentials_valid = 1;

    return 1;
}

int tls_handshake(
    rictus_tls* tls,
    SOCKET socket,
    const char* hostname
)
{
    SECURITY_STATUS status;

    SecBuffer out_buffer;
    SecBufferDesc out_desc;

    SecBuffer in_buffers[2];
    SecBufferDesc in_desc;

    unsigned long context_flags = 0;

    unsigned long requested_flags =
        ISC_REQ_SEQUENCE_DETECT |
        ISC_REQ_REPLAY_DETECT |
        ISC_REQ_CONFIDENTIALITY |
        ISC_REQ_EXTENDED_ERROR |
        ISC_REQ_ALLOCATE_MEMORY |
        ISC_REQ_STREAM |
        ISC_REQ_USE_SUPPLIED_CREDS;

    char input[TLS_BUFFER_SIZE];

    int input_len = 0;
    int first_call = 1;

    memset(
        &tls->context,
        0,
        sizeof(tls->context)
    );

    memset(
        input,
        0,
        sizeof(input)
    );

    for (;;)
    {
        memset(
            &out_buffer,
            0,
            sizeof(out_buffer)
        );

        out_buffer.BufferType =
            SECBUFFER_TOKEN;

        out_desc.ulVersion =
            SECBUFFER_VERSION;

        out_desc.cBuffers = 1;
        out_desc.pBuffers =
            &out_buffer;

        memset(
            in_buffers,
            0,
            sizeof(in_buffers)
        );

        if (first_call)
        {
            status =
                InitializeSecurityContextA(
                    &tls->credentials,
                    NULL,
                    (SEC_CHAR*)hostname,
                    requested_flags,
                    0,
                    SECURITY_NATIVE_DREP,
                    NULL,
                    0,
                    &tls->context,
                    &out_desc,
                    &context_flags,
                    NULL
                );

            first_call = 0;
        }
        else
        {
            in_buffers[0].BufferType =
                SECBUFFER_TOKEN;

            in_buffers[0].pvBuffer =
                input;

            in_buffers[0].cbBuffer =
                input_len;

            in_buffers[1].BufferType =
                SECBUFFER_EMPTY;

            in_buffers[1].pvBuffer =
                NULL;

            in_buffers[1].cbBuffer =
                0;

            in_desc.ulVersion =
                SECBUFFER_VERSION;

            in_desc.cBuffers = 2;
            in_desc.pBuffers =
                in_buffers;

            status =
                InitializeSecurityContextA(
                    &tls->credentials,
                    &tls->context,
                    (SEC_CHAR*)hostname,
                    requested_flags,
                    0,
                    SECURITY_NATIVE_DREP,
                    &in_desc,
                    0,
                    &tls->context,
                    &out_desc,
                    &context_flags,
                    NULL
                );
        }

        if (
            status == SEC_I_COMPLETE_NEEDED ||
            status == SEC_I_COMPLETE_AND_CONTINUE
            )
        {
            SECURITY_STATUS complete_status;

            complete_status =
                CompleteAuthToken(
                    &tls->context,
                    &out_desc
                );

            if (complete_status != SEC_E_OK)
            {
                fprintf(
                    stderr,
                    "CompleteAuthToken failed: 0x%08lx\n",
                    (unsigned long)complete_status
                );

                if (out_buffer.pvBuffer != NULL)
                {
                    FreeContextBuffer(
                        out_buffer.pvBuffer
                    );
                }

                return 0;
            }
        }

        /*
         * Send any TLS handshake token generated
         * by Schannel.
         */
        if (
            out_buffer.pvBuffer != NULL &&
            out_buffer.cbBuffer > 0
            )
        {
            int total_sent = 0;

            while (
                total_sent <
                (int)out_buffer.cbBuffer
                )
            {
                int sent;

                sent = send(
                    socket,
                    (const char*)
                    out_buffer.pvBuffer +
                    total_sent,
                    (int)out_buffer.cbBuffer -
                    total_sent,
                    0
                );

                if (sent == SOCKET_ERROR)
                {
                    fprintf(
                        stderr,
                        "TLS handshake send failed: %d\n",
                        WSAGetLastError()
                    );

                    FreeContextBuffer(
                        out_buffer.pvBuffer
                    );

                    return 0;
                }

                if (sent == 0)
                {
                    fprintf(
                        stderr,
                        "TLS handshake send returned zero.\n"
                    );

                    FreeContextBuffer(
                        out_buffer.pvBuffer
                    );

                    return 0;
                }

                total_sent += sent;
            }

            FreeContextBuffer(
                out_buffer.pvBuffer
            );

            out_buffer.pvBuffer = NULL;
            out_buffer.cbBuffer = 0;
        }

        /*
         * Handshake complete.
         */
        if (
            status == SEC_E_OK ||
            status == SEC_I_COMPLETE_NEEDED
            )
        {
            tls->context_valid = 1;

            printf(
                "TLS handshake established.\n"
            );

            return 1;
        }

        /*
         * Server requested a client certificate.
         * Rictus intentionally has none configured.
         */
        if (
            status ==
            SEC_I_INCOMPLETE_CREDENTIALS
            )
        {
            printf(
                "TLS client certificate requested; "
                "continuing without one.\n"
            );

            continue;
        }

        if (
            status != SEC_I_CONTINUE_NEEDED &&
            status != SEC_I_COMPLETE_AND_CONTINUE &&
            status != SEC_E_INCOMPLETE_MESSAGE
            )
        {
            fprintf(
                stderr,
                "InitializeSecurityContext failed: 0x%08lx\n",
                (unsigned long)status
            );

            return 0;
        }

        /*
         * Preserve bytes Schannel did not consume.
         */
        if (
            in_buffers[1].BufferType ==
            SECBUFFER_EXTRA
            )
        {
            int extra;

            extra =
                (int)in_buffers[1].cbBuffer;

            if (
                extra < 0 ||
                extra > input_len
                )
            {
                fprintf(
                    stderr,
                    "Invalid Schannel extra-byte count.\n"
                );

                return 0;
            }

            memmove(
                input,
                input + input_len - extra,
                extra
            );

            input_len = extra;
        }
        else if (
            status !=
            SEC_E_INCOMPLETE_MESSAGE
            )
        {
            input_len = 0;
        }

        if (
            input_len >=
            TLS_BUFFER_SIZE
            )
        {
            fprintf(
                stderr,
                "TLS handshake buffer exhausted.\n"
            );

            return 0;
        }

        {
            int received;

            received = recv(
                socket,
                input + input_len,
                TLS_BUFFER_SIZE - input_len,
                0
            );

            if (received == SOCKET_ERROR)
            {
                fprintf(
                    stderr,
                    "TLS handshake receive failed: %d\n",
                    WSAGetLastError()
                );

                return 0;
            }

            if (received == 0)
            {
                fprintf(
                    stderr,
                    "TLS handshake connection closed by peer.\n"
                );

                return 0;
            }

            input_len += received;
        }
    }
}

int tls_send(
    rictus_tls* tls,
    SOCKET socket,
    const char* data,
    int data_len
)
{
    SECURITY_STATUS status;
    SecPkgContext_StreamSizes sizes;

    SecBuffer buffers[4];
    SecBufferDesc message;

    unsigned char* packet;

    int packet_size;
    int total_size;
    int total_sent;

    if (
        tls == NULL ||
        !tls->context_valid ||
        data == NULL ||
        data_len <= 0
        )
    {
        return 0;
    }

    status = QueryContextAttributesA(
        &tls->context,
        SECPKG_ATTR_STREAM_SIZES,
        &sizes
    );

    if (status != SEC_E_OK)
    {
        fprintf(
            stderr,
            "QueryContextAttributes failed: 0x%08lx\n",
            (unsigned long)status
        );

        return 0;
    }

    if (
        (unsigned long)data_len >
        sizes.cbMaximumMessage
        )
    {
        fprintf(
            stderr,
            "TLS message exceeds maximum size.\n"
        );

        return 0;
    }

    packet_size =
        (int)sizes.cbHeader +
        data_len +
        (int)sizes.cbTrailer;

    packet =
        (unsigned char*)malloc(
            packet_size
        );

    if (packet == NULL)
    {
        fprintf(
            stderr,
            "TLS send allocation failed.\n"
        );

        return 0;
    }

    memcpy(
        packet + sizes.cbHeader,
        data,
        data_len
    );

    memset(
        buffers,
        0,
        sizeof(buffers)
    );

    buffers[0].BufferType =
        SECBUFFER_STREAM_HEADER;

    buffers[0].pvBuffer =
        packet;

    buffers[0].cbBuffer =
        sizes.cbHeader;

    buffers[1].BufferType =
        SECBUFFER_DATA;

    buffers[1].pvBuffer =
        packet + sizes.cbHeader;

    buffers[1].cbBuffer =
        data_len;

    buffers[2].BufferType =
        SECBUFFER_STREAM_TRAILER;

    buffers[2].pvBuffer =
        packet + sizes.cbHeader + data_len;

    buffers[2].cbBuffer =
        sizes.cbTrailer;

    buffers[3].BufferType =
        SECBUFFER_EMPTY;

    message.ulVersion =
        SECBUFFER_VERSION;

    message.cBuffers = 4;
    message.pBuffers =
        buffers;

    status = EncryptMessage(
        &tls->context,
        0,
        &message,
        0
    );

    if (status != SEC_E_OK)
    {
        fprintf(
            stderr,
            "EncryptMessage failed: 0x%08lx\n",
            (unsigned long)status
        );

        free(packet);

        return 0;
    }

    total_size =
        (int)buffers[0].cbBuffer +
        (int)buffers[1].cbBuffer +
        (int)buffers[2].cbBuffer;

    total_sent = 0;

    while (
        total_sent <
        total_size
        )
    {
        int sent;

        sent = send(
            socket,
            (const char*)packet +
            total_sent,
            total_size -
            total_sent,
            0
        );

        if (sent == SOCKET_ERROR)
        {
            fprintf(
                stderr,
                "TLS send failed: %d\n",
                WSAGetLastError()
            );

            free(packet);

            return 0;
        }

        if (sent == 0)
        {
            fprintf(
                stderr,
                "TLS send returned zero.\n"
            );

            free(packet);

            return 0;
        }

        total_sent += sent;
    }

    free(packet);

    return 1;
}

/*
 * TLS receive/decryption is the next root step.
 */
int tls_recv(
    rictus_tls* tls,
    SOCKET socket,
    char* output,
    int output_size
)
{
    SECURITY_STATUS status;

    SecBuffer buffers[4];
    SecBufferDesc message;

    int received;

    if (
        tls == NULL ||
        !tls->context_valid ||
        output == NULL ||
        output_size <= 1
        )
    {
        return -1;
    }

    for (;;)
    {
        /*
         * If no encrypted data is currently buffered,
         * receive some from the socket.
         */
        if (tls->recv_buffer_len == 0)
        {
            received = recv(
                socket,
                (char*)tls->recv_buffer,
                RICTUS_TLS_RECV_BUFFER,
                0
            );

            if (received == SOCKET_ERROR)
            {
                fprintf(
                    stderr,
                    "TLS receive failed: %d\n",
                    WSAGetLastError()
                );

                return -1;
            }

            if (received == 0)
            {
                return 0;
            }

            tls->recv_buffer_len =
                received;
        }

        memset(
            buffers,
            0,
            sizeof(buffers)
        );

        buffers[0].BufferType =
            SECBUFFER_DATA;

        buffers[0].pvBuffer =
            tls->recv_buffer;

        buffers[0].cbBuffer =
            tls->recv_buffer_len;

        buffers[1].BufferType =
            SECBUFFER_EMPTY;

        buffers[2].BufferType =
            SECBUFFER_EMPTY;

        buffers[3].BufferType =
            SECBUFFER_EMPTY;

        message.ulVersion =
            SECBUFFER_VERSION;

        message.cBuffers = 4;
        message.pBuffers =
            buffers;

        status = DecryptMessage(
            &tls->context,
            &message,
            0,
            NULL
        );

        /*
         * We do not yet have an entire TLS record.
         * Append more encrypted bytes.
         */
        if (
            status ==
            SEC_E_INCOMPLETE_MESSAGE
            )
        {
            if (
                tls->recv_buffer_len >=
                RICTUS_TLS_RECV_BUFFER
                )
            {
                fprintf(
                    stderr,
                    "TLS receive buffer exhausted.\n"
                );

                return -1;
            }

            received = recv(
                socket,
                (char*)tls->recv_buffer +
                tls->recv_buffer_len,
                RICTUS_TLS_RECV_BUFFER -
                tls->recv_buffer_len,
                0
            );

            if (received == SOCKET_ERROR)
            {
                fprintf(
                    stderr,
                    "TLS receive failed: %d\n",
                    WSAGetLastError()
                );

                return -1;
            }

            if (received == 0)
            {
                return 0;
            }

            tls->recv_buffer_len +=
                received;

            continue;
        }

        if (
            status ==
            SEC_I_CONTEXT_EXPIRED
            )
        {
            return 0;
        }

        if (status != SEC_E_OK)
        {
            fprintf(
                stderr,
                "DecryptMessage failed: 0x%08lx\n",
                (unsigned long)status
            );

            return -1;
        }

        /*
         * Find decrypted application data and any
         * encrypted bytes belonging to another TLS
         * record.
         */
        {
            SecBuffer* data_buffer = NULL;
            SecBuffer* extra_buffer = NULL;

            int i;
            int copy_len;

            for (i = 0; i < 4; ++i)
            {
                if (
                    buffers[i].BufferType ==
                    SECBUFFER_DATA
                    )
                {
                    data_buffer =
                        &buffers[i];
                }
                else if (
                    buffers[i].BufferType ==
                    SECBUFFER_EXTRA
                    )
                {
                    extra_buffer =
                        &buffers[i];
                }
            }

            if (data_buffer != NULL)
            {
                copy_len =
                    (int)data_buffer->cbBuffer;

                if (
                    copy_len >
                    output_size - 1
                    )
                {
                    fprintf(
                        stderr,
                        "TLS plaintext output buffer too small.\n"
                    );

                    return -1;
                }

                memcpy(
                    output,
                    data_buffer->pvBuffer,
                    copy_len
                );

                output[copy_len] =
                    '\0';
            }
            else
            {
                copy_len = 0;
            }

            /*
             * Preserve another encrypted TLS record
             * if Schannel says one follows.
             */
            if (extra_buffer != NULL)
            {
                int extra_len =
                    (int)extra_buffer->cbBuffer;

                memmove(
                    tls->recv_buffer,
                    extra_buffer->pvBuffer,
                    extra_len
                );

                tls->recv_buffer_len =
                    extra_len;
            }
            else
            {
                tls->recv_buffer_len =
                    0;
            }

            if (copy_len > 0)
            {
                return copy_len;
            }
        }
    }
}

void tls_cleanup(
    rictus_tls* tls
)
{
    if (tls == NULL)
    {
        return;
    }

    if (tls->context_valid)
    {
        DeleteSecurityContext(
            &tls->context
        );

        tls->context_valid = 0;
    }

    if (tls->credentials_valid)
    {
        FreeCredentialsHandle(
            &tls->credentials
        );

        tls->credentials_valid = 0;
    }
}