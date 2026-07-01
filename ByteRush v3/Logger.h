#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdint.h>

#if defined(_MSC_VER)
// Include Winsock2 BEFORE Windows.h
#include <winsock2.h>
#include <windows.h>
#pragma comment(lib, "Ws2_32.lib")  // link Winsock library
typedef SOCKET log_sock_t;
#else
#include <sys/socket.h>
#include <arpa/inet.h>                // for htonl
typedef int log_sock_t;
#endif

#if defined(_MSC_VER)
#define PATH_SEP '\\'
#else
#define PATH_SEP '/'
#endif

// Extract just the file‑name portion from a full path
static inline const char* _just_filename(const char* path) {
    const char* p = strrchr(path, PATH_SEP);
    return p ? p + 1 : path;
}

// Portable localtime
#if defined(_MSC_VER)
static inline void _get_localtime(const time_t* t, struct tm* out) {
    localtime_s(out, t);
}
#else
static inline void _get_localtime(const time_t* t, struct tm* out) {
    localtime_r(t, out);
}
#endif

// Build log header into buffer, returns length
static inline int _format_log_header(char* buf, size_t bufsize,
    const char* file, int line,
    const char* func) {
    time_t now = time(NULL);
    struct tm tm;
    _get_localtime(&now, &tm);
    return snprintf(buf, bufsize,
        "[%04d-%02d-%02d %02d:%02d:%02d] %s:%d %s():",
        tm.tm_year + 1900,
        tm.tm_mon + 1,
        tm.tm_mday,
        tm.tm_hour,
        tm.tm_min,
        tm.tm_sec,
        _just_filename(file),
        line,
        func);
}

/**
 * LOG macro sending length-prefixed message to socket:
 *   LOG(sock, "message");
 *   LOG(sock, "Value=%d", x);
 * Message format: [4-byte big-endian length][payload...]
 * Requirements on Windows:
 *   - Call WSAStartup() once before any LOG()
 *   - Call WSACleanup() when done
 */
#define LOG(sock, ...)                                                         \
    do {                                                                       \
        char _log_buf[1024];                                                   \
        int _off = _format_log_header(_log_buf, sizeof(_log_buf),             \
                                     __FILE__, __LINE__, __func__);           \
        if ("" #__VA_ARGS__[0]) {                                            \
            _off += snprintf(_log_buf + _off, sizeof(_log_buf) - _off,         \
                              " " __VA_ARGS__);                              \
        }                                                                      \
        _off += snprintf(_log_buf + _off, sizeof(_log_buf) - _off, "\r\n");  \
        /* send 4-byte big-endian length prefix */                             \
        uint32_t _len = (uint32_t)_off;                                         \
        uint32_t _len_be = htonl(_len);                                         \
        send((log_sock_t)(sock), (const char*)&_len_be, sizeof(_len_be), 0);    \
        /* send actual log payload */                                          \
        send((log_sock_t)(sock), _log_buf, (size_t)_len, 0);                   \
    } while (0)

#endif // LOGGER_H
