#define WIN32_LEAN_AND_MEAN

#include <windows.h>

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "log.h"

#define RICTUS_LOG_PATH_MAX       512
#define RICTUS_LOG_MESSAGE_MAX    4096
#define RICTUS_LOG_LINE_MAX       4608

static HANDLE g_log_handle =
INVALID_HANDLE_VALUE;

static CRITICAL_SECTION g_log_lock;

static int g_log_lock_initialized = 0;

static char g_log_path[
    RICTUS_LOG_PATH_MAX
] = "";

static unsigned long long g_sequence = 0;

static DWORD g_process_id = 0;

static unsigned long long g_session_id = 0;


static int build_timestamp(
    char* buffer,
    size_t buffer_size
)
{
    SYSTEMTIME local_time;

    int written;

    if (
        buffer == NULL ||
        buffer_size == 0
        )
    {
        return 0;
    }

    GetLocalTime(
        &local_time
    );

    written = snprintf(
        buffer,
        buffer_size,
        "%04u-%02u-%02uT%02u:%02u:%02u.%03u",
        local_time.wYear,
        local_time.wMonth,
        local_time.wDay,
        local_time.wHour,
        local_time.wMinute,
        local_time.wSecond,
        local_time.wMilliseconds
    );

    if (
        written <= 0 ||
        written >= (int)buffer_size
        )
    {
        return 0;
    }

    return 1;
}


static unsigned long long build_session_id(void)
{
    FILETIME file_time;

    ULARGE_INTEGER value;

    GetSystemTimeAsFileTime(
        &file_time
    );

    value.LowPart =
        file_time.dwLowDateTime;

    value.HighPart =
        file_time.dwHighDateTime;

    return
        value.QuadPart ^
        ((unsigned long long)GetCurrentProcessId() << 32);
}


int rictus_log_init(
    const char* directory,
    const char* filename
)
{
    DWORD attributes;

    int length;

    if (
        directory == NULL ||
        filename == NULL
        )
    {
        return 0;
    }

    attributes =
        GetFileAttributesA(
            directory
        );

    if (
        attributes ==
        INVALID_FILE_ATTRIBUTES
        )
    {
        if (
            !CreateDirectoryA(
                directory,
                NULL
            )
            )
        {
            DWORD error =
                GetLastError();

            if (
                error !=
                ERROR_ALREADY_EXISTS
                )
            {
                fprintf(
                    stderr,
                    "Unable to create log directory: %lu\n",
                    error
                );

                return 0;
            }
        }
    }
    else if (
        !(attributes &
            FILE_ATTRIBUTE_DIRECTORY)
        )
    {
        fprintf(
            stderr,
            "Log path exists but is not a directory.\n"
        );

        return 0;
    }

    length = snprintf(
        g_log_path,
        sizeof(g_log_path),
        "%s\\%s",
        directory,
        filename
    );

    if (
        length <= 0 ||
        length >=
        (int)sizeof(g_log_path)
        )
    {
        fprintf(
            stderr,
            "Log path is too long.\n"
        );

        return 0;
    }

    g_log_handle =
        CreateFileA(
            g_log_path,
            FILE_APPEND_DATA,
            FILE_SHARE_READ |
            FILE_SHARE_WRITE,
            NULL,
            OPEN_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            NULL
        );

    if (
        g_log_handle ==
        INVALID_HANDLE_VALUE
        )
    {
        fprintf(
            stderr,
            "Unable to open log file: %s error=%lu\n",
            g_log_path,
            GetLastError()
        );

        g_log_path[0] =
            '\0';

        return 0;
    }

    InitializeCriticalSection(
        &g_log_lock
    );

    g_log_lock_initialized = 1;

    g_sequence = 0;

    g_process_id =
        GetCurrentProcessId();

    g_session_id =
        build_session_id();

    return 1;
}


const char* rictus_log_path(void)
{
    return g_log_path;
}


unsigned long long rictus_log_session_id(void)
{
    return g_session_id;
}


void rictus_log_close(void)
{
    if (
        g_log_lock_initialized
        )
    {
        EnterCriticalSection(
            &g_log_lock
        );
    }

    if (
        g_log_handle !=
        INVALID_HANDLE_VALUE
        )
    {
        FlushFileBuffers(
            g_log_handle
        );

        CloseHandle(
            g_log_handle
        );

        g_log_handle =
            INVALID_HANDLE_VALUE;
    }

    if (
        g_log_lock_initialized
        )
    {
        LeaveCriticalSection(
            &g_log_lock
        );

        DeleteCriticalSection(
            &g_log_lock
        );

        g_log_lock_initialized = 0;
    }
}


int rictus_log_write(
    const char* level,
    const char* event,
    const char* format,
    ...
)
{
    char timestamp[64];

    char message[
        RICTUS_LOG_MESSAGE_MAX
    ];

    char line[
        RICTUS_LOG_LINE_MAX
    ];

    va_list arguments;

    int message_length;
    int line_length;

    DWORD bytes_written;

    unsigned long long sequence;

    int result = 0;

    if (
        g_log_handle ==
        INVALID_HANDLE_VALUE ||
        level == NULL ||
        event == NULL
        )
    {
        return 0;
    }

    if (
        !build_timestamp(
            timestamp,
            sizeof(timestamp)
        )
        )
    {
        return 0;
    }

    message[0] =
        '\0';

    if (
        format != NULL &&
        format[0] != '\0'
        )
    {
        va_start(
            arguments,
            format
        );

        message_length =
            vsnprintf(
                message,
                sizeof(message),
                format,
                arguments
            );

        va_end(
            arguments
        );

        if (
            message_length < 0
            )
        {
            return 0;
        }

        message[
            sizeof(message) - 1
        ] = '\0';
    }

    if (
        g_log_lock_initialized
        )
    {
        EnterCriticalSection(
            &g_log_lock
        );
    }

    sequence =
        ++g_sequence;

    line_length = snprintf(
        line,
        sizeof(line),
        "%s seq=%llu session=%llu pid=%lu %-5s %-22s %s\r\n",
        timestamp,
        sequence,
        g_session_id,
        (unsigned long)g_process_id,
        level,
        event,
        message
    );

    if (
        line_length <= 0 ||
        line_length >=
        (int)sizeof(line)
        )
    {
        goto cleanup;
    }

    bytes_written = 0;

    if (
        !WriteFile(
            g_log_handle,
            line,
            (DWORD)line_length,
            &bytes_written,
            NULL
        )
        )
    {
        goto cleanup;
    }

    if (
        bytes_written !=
        (DWORD)line_length
        )
    {
        goto cleanup;
    }

    if (
        !FlushFileBuffers(
            g_log_handle
        )
        )
    {
        goto cleanup;
    }

    result = 1;

cleanup:

    if (
        g_log_lock_initialized
        )
    {
        LeaveCriticalSection(
            &g_log_lock
        );
    }

    return result;
}