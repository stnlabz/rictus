#include "sasl.h"

#include <windows.h>
#include <wincrypt.h>

#include <stdio.h>
#include <string.h>

#pragma comment(lib, "Crypt32.lib")

int sasl_build_plain(
    const rictus_config* config,
    char* output,
    int output_size
)
{
    unsigned char plain[768];

    size_t account_len;
    size_t password_len;

    size_t plain_len;

    DWORD encoded_len;

    if (
        config == NULL ||
        output == NULL ||
        output_size <= 0
        )
    {
        return 0;
    }

    account_len =
        strlen(
            config->irc_account
        );

    password_len =
        strlen(
            config->irc_password
        );

    /*
     * SASL PLAIN payload:
     *
     * authzid = account
     * authcid = account
     * passwd  = password
     *
     * account\0account\0password
     */
    plain_len =
        account_len +
        1 +
        account_len +
        1 +
        password_len;

    if (
        plain_len >
        sizeof(plain)
        )
    {
        fprintf(
            stderr,
            "SASL credential payload too large.\n"
        );

        return 0;
    }

    memcpy(
        plain,
        config->irc_account,
        account_len
    );

    plain[
        account_len
    ] = '\0';

    memcpy(
        plain +
        account_len +
        1,
        config->irc_account,
        account_len
    );

    plain[
        account_len +
            1 +
            account_len
    ] = '\0';

    memcpy(
        plain +
        account_len +
        1 +
        account_len +
        1,
        config->irc_password,
        password_len
    );

    encoded_len =
        (DWORD)output_size;

    if (
        !CryptBinaryToStringA(
            plain,
            (DWORD)plain_len,
            CRYPT_STRING_BASE64 |
            CRYPT_STRING_NOCRLF,
            output,
            &encoded_len
        )
        )
    {
        fprintf(
            stderr,
            "SASL Base64 encoding failed: %lu\n",
            GetLastError()
        );

        SecureZeroMemory(
            plain,
            sizeof(plain)
        );

        return 0;
    }

    SecureZeroMemory(
        plain,
        sizeof(plain)
    );

    return 1;
}