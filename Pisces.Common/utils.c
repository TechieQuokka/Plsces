#include "pch.h"
#include "utils.h"
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <winsock2.h>

// =============================================================================
// 전역 변수
// =============================================================================

log_level_t g_current_log_level = LOG_LEVEL_INFO;

// =============================================================================
// 시간 관련 함수들
// =============================================================================

char* utils_get_current_time_string(char* buffer, size_t buffer_size) {
    if (!buffer || buffer_size < TIME_STRING_SIZE) {
        return NULL;
    }

    time_t now;
    struct tm local_time;

    time(&now);
    if (localtime_s(&local_time, &now) != 0) {
        return NULL;
    }

    if (strftime(buffer, buffer_size, "%Y-%m-%d %H:%M:%S", &local_time) == 0) {
        return NULL;
    }

    return buffer;
}

char* utils_get_current_date_string(char* buffer, size_t buffer_size) {
    if (!buffer || buffer_size < DATE_STRING_SIZE) {
        return NULL;
    }

    time_t now;
    struct tm local_time;

    time(&now);
    if (localtime_s(&local_time, &now) != 0) {
        return NULL;
    }

    if (strftime(buffer, buffer_size, "%Y-%m-%d", &local_time) == 0) {
        return NULL;
    }

    return buffer;
}

char* utils_time_to_string(time_t time_val, char* buffer, size_t buffer_size) {
    if (!buffer || buffer_size < TIME_STRING_SIZE) {
        return NULL;
    }

    struct tm local_time;
    if (localtime_s(&local_time, &time_val) != 0) {
        return NULL;
    }

    if (strftime(buffer, buffer_size, "%Y-%m-%d %H:%M:%S", &local_time) == 0) {
        return NULL;
    }

    return buffer;
}

uint64_t utils_get_current_timestamp_ms(void) {
    FILETIME ft;
    ULARGE_INTEGER uli;

    GetSystemTimeAsFileTime(&ft);
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;

    // Windows FILETIME is in 100-nanosecond intervals since January 1, 1601
    // Convert to milliseconds since Unix epoch (January 1, 1970)
    return (uli.QuadPart - 116444736000000000ULL) / 10000ULL;
}

double utils_time_diff_seconds(time_t start, time_t end) {
    return difftime(end, start);
}

// =============================================================================
// 문자열 유틸리티 (ASCII 전용)
// =============================================================================

int utils_string_copy(char* dest, size_t dest_size, const char* src) {
    if (!dest || !src || dest_size == 0) {
        return -1;
    }

    errno_t result = strcpy_s(dest, dest_size, src);
    return (result == 0) ? 0 : -1;
}

int utils_string_concat(char* dest, size_t dest_size, const char* src) {
    if (!dest || !src || dest_size == 0) {
        return -1;
    }

    errno_t result = strcat_s(dest, dest_size, src);
    return (result == 0) ? 0 : -1;
}

char* utils_string_trim(char* str) {
    if (!str) return NULL;

    // 앞쪽 공백 제거
    char* start = str;
    while (*start && isspace((unsigned char)*start)) {
        start++;
    }

    // 빈 문자열인 경우
    if (*start == 0) {
        *str = 0;
        return str;
    }

    // 뒤쪽 공백 제거
    char* end = start + strlen(start) - 1;
    while (end > start && isspace((unsigned char)*end)) {
        end--;
    }
    *(end + 1) = 0;

    // 앞쪽이 잘린 경우 문자열 이동
    if (start != str) {
        memmove(str, start, end - start + 2);
    }

    return str;
}

char* utils_string_to_lower(char* str) {
    if (!str) return NULL;

    for (char* p = str; *p; p++) {
        if (*p >= 'A' && *p <= 'Z') {  // ASCII만 처리
            *p = *p + ('a' - 'A');
        }
    }
    return str;
}

char* utils_string_to_upper(char* str) {
    if (!str) return NULL;

    for (char* p = str; *p; p++) {
        if (*p >= 'a' && *p <= 'z') {  // ASCII만 처리
            *p = *p - ('a' - 'A');
        }
    }
    return str;
}

int utils_string_is_empty(const char* str) {
    return (!str || *str == 0) ? 1 : 0;
}

int utils_string_count_char(const char* str, char ch) {
    if (!str) return 0;

    int count = 0;
    for (const char* p = str; *p; p++) {
        if (*p == ch) count++;
    }
    return count;
}

// =============================================================================
// 메모리 유틸리티
// =============================================================================

void utils_memory_zero(void* ptr, size_t size) {
    if (ptr && size > 0) {
        memset(ptr, 0, size);
    }
}

void utils_memory_secure_zero(void* ptr, size_t size) {
    if (ptr && size > 0) {
        SecureZeroMemory(ptr, size);  // Windows secure memory clearing
    }
}

// =============================================================================
// 바이트 변환 유틸리티
// =============================================================================

char* utils_bytes_to_human_readable(uint64_t bytes, char* buffer, size_t buffer_size) {
    if (!buffer || buffer_size < 16) {  // 충분한 버퍼 크기 확인
        return NULL;
    }

    const char* units[] = { "B", "KB", "MB", "GB", "TB" };
    int unit_index = 0;
    double size = (double)bytes;

    while (size >= 1024.0 && unit_index < 4) {
        size /= 1024.0;
        unit_index++;
    }

    if (unit_index == 0) {
        sprintf_s(buffer, buffer_size, "%llu %s", bytes, units[unit_index]);
    }
    else {
        sprintf_s(buffer, buffer_size, "%.2f %s", size, units[unit_index]);
    }

    return buffer;
}

char* utils_bytes_to_hex(const void* data, size_t data_size,
    char* hex_buffer, size_t hex_buffer_size) {
    if (!data || !hex_buffer || hex_buffer_size < (data_size * 2 + 1)) {
        return NULL;
    }

    const unsigned char* bytes = (const unsigned char*)data;
    for (size_t i = 0; i < data_size; i++) {
        sprintf_s(hex_buffer + (i * 2), hex_buffer_size - (i * 2), "%02x", bytes[i]);
    }

    return hex_buffer;
}

// =============================================================================
// 에러 처리 유틸리티
// =============================================================================

char* utils_get_last_error_string(DWORD error_code, char* buffer, size_t buffer_size) {
    if (!buffer || buffer_size == 0) {
        return NULL;
    }

    DWORD result = FormatMessageA(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        error_code,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        buffer,
        (DWORD)buffer_size,
        NULL
    );

    if (result == 0) {
        sprintf_s(buffer, buffer_size, "Unknown error (code: %lu)", error_code);
    }
    else {
        // 뒤의 개행문자 제거
        utils_string_trim(buffer);
    }

    return buffer;
}

const char* utils_winsock_error_to_string(int wsa_error) {
    switch (wsa_error) {
    case WSAEACCES:           return "Permission denied";
    case WSAEADDRINUSE:       return "Address already in use";
    case WSAEADDRNOTAVAIL:    return "Cannot assign requested address";
    case WSAEAFNOSUPPORT:     return "Address family not supported";
    case WSAEALREADY:         return "Operation already in progress";
    case WSAECONNABORTED:     return "Connection aborted";
    case WSAECONNREFUSED:     return "Connection refused";
    case WSAECONNRESET:       return "Connection reset by peer";
    case WSAEFAULT:           return "Bad address";
    case WSAEHOSTDOWN:        return "Host is down";
    case WSAEHOSTUNREACH:     return "No route to host";
    case WSAEINPROGRESS:      return "Operation now in progress";
    case WSAEINTR:            return "Interrupted function call";
    case WSAEINVAL:           return "Invalid argument";
    case WSAEISCONN:          return "Socket is already connected";
    case WSAEMFILE:           return "Too many open files";
    case WSAEMSGSIZE:         return "Message too long";
    case WSAENETDOWN:         return "Network is down";
    case WSAENETUNREACH:      return "Network is unreachable";
    case WSAENOBUFS:          return "No buffer space available";
    case WSAENOTCONN:         return "Socket is not connected";
    case WSAENOTSOCK:         return "Socket operation on non-socket";
    case WSAEOPNOTSUPP:       return "Operation not supported";
    case WSAEPROTONOSUPPORT:  return "Protocol not supported";
    case WSAEPROTOTYPE:       return "Protocol wrong type for socket";
    case WSAESHUTDOWN:        return "Cannot send after socket shutdown";
    case WSAESOCKTNOSUPPORT:  return "Socket type not supported";
    case WSAETIMEDOUT:        return "Connection timed out";
    case WSAEWOULDBLOCK:      return "Resource temporarily unavailable";
    case WSANOTINITIALISED:   return "WSAStartup not yet performed";
    default:                  return "Unknown Winsock error";
    }
}

// =============================================================================
// 랜덤/해시 유틸리티
// =============================================================================

char* utils_generate_random_string(char* buffer, size_t length) {
    if (!buffer || length == 0) {
        return NULL;
    }

    const char charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    const size_t charset_size = sizeof(charset) - 1;

    // 간단한 시드 초기화 (보안용이 아닌 용도)
    static int seeded = 0;
    if (!seeded) {
        srand((unsigned int)time(NULL));
        seeded = 1;
    }

    for (size_t i = 0; i < length; i++) {
        buffer[i] = charset[rand() % charset_size];
    }
    buffer[length] = '\0';

    return buffer;
}

uint32_t utils_hash_string(const char* str) {
    if (!str) return 0;

    // 간단한 djb2 해시 알고리즘
    uint32_t hash = 5381;
    int c;

    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;  // hash * 33 + c
    }

    return hash;
}

// =============================================================================
// 로깅 함수들
// =============================================================================

void utils_log(log_level_t level, const char* file, int line, const char* fmt, ...) {
    if (level < g_current_log_level) {
        return;  // 현재 로그 레벨보다 낮으면 출력하지 않음
    }

    // 시간 문자열 생성
    char time_buffer[TIME_STRING_SIZE];
    utils_get_current_time_string(time_buffer, sizeof(time_buffer));

    // 파일명에서 경로 제거 (파일명만 표시)
    const char* filename = strrchr(file, '\\');
    if (!filename) filename = strrchr(file, '/');
    if (!filename) filename = file;
    else filename++;  // '\' 또는 '/' 다음 문자부터

    // 로그 레벨에 따른 출력 스트림 선택
    FILE* output = (level >= LOG_LEVEL_WARNING) ? stderr : stdout;

    // 로그 헤더 출력
    fprintf(output, "[%s] [%s] %s:%d: ",
        time_buffer,
        utils_log_level_to_string(level),
        filename,
        line);

    // 실제 메시지 출력
    va_list args;
    va_start(args, fmt);
    vfprintf(output, fmt, args);
    va_end(args);

    fprintf(output, "\n");
    fflush(output);
}

void utils_set_log_level(log_level_t level) {
    g_current_log_level = level;
}

const char* utils_log_level_to_string(log_level_t level) {
    switch (level) {
    case LOG_LEVEL_DEBUG:    return "DEBUG";
    case LOG_LEVEL_INFO:     return "INFO";
    case LOG_LEVEL_WARNING:  return "WARN";
    case LOG_LEVEL_ERROR:    return "ERROR";
    case LOG_LEVEL_CRITICAL: return "CRIT";
    default:                 return "UNKNOWN";
    }
}