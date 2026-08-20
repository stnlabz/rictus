int rictus_log_init(
    const char* directory,
    const char* filename
);

void rictus_log_close(void);

int rictus_log_write(
    const char* level,
    const char* event,
    const char* format,
    ...
);

const char* rictus_log_path(void);

unsigned long long rictus_log_session_id(void);