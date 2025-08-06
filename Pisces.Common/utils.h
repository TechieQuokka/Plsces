#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <time.h>
#include <stdint.h>

// =============================================================================
// 로그 레벨 정의
// =============================================================================

typedef enum {
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO = 1,
    LOG_LEVEL_WARNING = 2,
    LOG_LEVEL_ERROR = 3,
    LOG_LEVEL_CRITICAL = 4
} log_level_t;

// =============================================================================
// 로깅 매크로 (ASCII 전용)
// =============================================================================

// 현재 로그 레벨 (전역 변수)
extern log_level_t g_current_log_level;

// 로그 출력 매크로들
#define LOG_DEBUG(fmt, ...)    utils_log(LOG_LEVEL_DEBUG, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)     utils_log(LOG_LEVEL_INFO, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_WARNING(fmt, ...)  utils_log(LOG_LEVEL_WARNING, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...)    utils_log(LOG_LEVEL_ERROR, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_CRITICAL(fmt, ...) utils_log(LOG_LEVEL_CRITICAL, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

// 조건부 로깅
#define LOG_IF(condition, level, fmt, ...) \
    do { if (condition) utils_log(level, __FILE__, __LINE__, fmt, ##__VA_ARGS__); } while(0)

// =============================================================================
// 시간 관련 상수 및 함수
// =============================================================================

#define TIME_STRING_SIZE    20      // "2025-08-06 15:30:45" + null
#define DATE_STRING_SIZE    11      // "2025-08-06" + null
#define TIME_ONLY_SIZE      9       // "15:30:45" + null

/**
 * 현재 시간을 문자열로 반환 (ASCII)
 * @param buffer 출력 버퍼 (최소 TIME_STRING_SIZE)
 * @param buffer_size 버퍼 크기
 * @return 성공 시 buffer 포인터, 실패 시 NULL
 */
char* utils_get_current_time_string(char* buffer, size_t buffer_size);

/**
 * 현재 날짜를 문자열로 반환 (ASCII)
 * @param buffer 출력 버퍼 (최소 DATE_STRING_SIZE)
 * @param buffer_size 버퍼 크기
 * @return 성공 시 buffer 포인터, 실패 시 NULL
 */
char* utils_get_current_date_string(char* buffer, size_t buffer_size);

/**
 * time_t를 문자열로 변환 (ASCII)
 * @param time_val 시간값
 * @param buffer 출력 버퍼 (최소 TIME_STRING_SIZE)
 * @param buffer_size 버퍼 크기
 * @return 성공 시 buffer 포인터, 실패 시 NULL
 */
char* utils_time_to_string(time_t time_val, char* buffer, size_t buffer_size);

/**
 * 밀리초 단위 현재 시간 반환
 * @return 밀리초 단위 시간
 */
uint64_t utils_get_current_timestamp_ms(void);

/**
 * 두 시간 사이의 차이 계산 (초)
 * @param start 시작 시간
 * @param end 끝 시간
 * @return 시간 차이 (초)
 */
double utils_time_diff_seconds(time_t start, time_t end);

// =============================================================================
// 문자열 유틸리티 (ASCII 안전)
// =============================================================================

/**
 * 문자열 안전하게 복사 (ASCII 전용)
 * @param dest 대상 버퍼
 * @param dest_size 대상 버퍼 크기
 * @param src 원본 문자열
 * @return 성공 시 0, 실패 시 -1
 */
int utils_string_copy(char* dest, size_t dest_size, const char* src);

/**
 * 문자열 안전하게 연결 (ASCII 전용)
 * @param dest 대상 버퍼
 * @param dest_size 대상 버퍼 크기
 * @param src 추가할 문자열
 * @return 성공 시 0, 실패 시 -1
 */
int utils_string_concat(char* dest, size_t dest_size, const char* src);

/**
 * 문자열에서 앞뒤 공백 제거 (ASCII)
 * @param str 대상 문자열 (직접 수정됨)
 * @return 수정된 문자열 포인터
 */
char* utils_string_trim(char* str);

/**
 * 문자열을 소문자로 변환 (ASCII)
 * @param str 대상 문자열 (직접 수정됨)
 * @return 수정된 문자열 포인터
 */
char* utils_string_to_lower(char* str);

/**
 * 문자열을 대문자로 변환 (ASCII)
 * @param str 대상 문자열 (직접 수정됨)
 * @return 수정된 문자열 포인터
 */
char* utils_string_to_upper(char* str);

/**
 * 문자열이 비어있는지 확인 (NULL 안전)
 * @param str 확인할 문자열
 * @return 비어있으면 1, 아니면 0
 */
int utils_string_is_empty(const char* str);

/**
 * 문자열에서 특정 문자 개수 세기
 * @param str 대상 문자열
 * @param ch 찾을 문자
 * @return 문자 개수
 */
int utils_string_count_char(const char* str, char ch);

// =============================================================================
// 메모리 유틸리티
// =============================================================================

/**
 * 메모리 안전하게 0으로 초기화
 * @param ptr 메모리 포인터
 * @param size 크기
 */
void utils_memory_zero(void* ptr, size_t size);

/**
 * 민감한 정보가 담긴 메모리를 안전하게 지우기
 * @param ptr 메모리 포인터
 * @param size 크기
 */
void utils_memory_secure_zero(void* ptr, size_t size);

// =============================================================================
// 바이트 변환 유틸리티
// =============================================================================

/**
 * 바이트 크기를 읽기 쉬운 문자열로 변환 (ASCII)
 * @param bytes 바이트 수
 * @param buffer 출력 버퍼
 * @param buffer_size 버퍼 크기
 * @return 변환된 문자열 포인터
 */
char* utils_bytes_to_human_readable(uint64_t bytes, char* buffer, size_t buffer_size);

/**
 * 16진수 문자열로 변환
 * @param data 변환할 데이터
 * @param data_size 데이터 크기
 * @param hex_buffer 출력 버퍼 (최소 data_size * 2 + 1)
 * @param hex_buffer_size 버퍼 크기
 * @return 성공 시 hex_buffer, 실패 시 NULL
 */
char* utils_bytes_to_hex(const void* data, size_t data_size,
    char* hex_buffer, size_t hex_buffer_size);

// =============================================================================
// 에러 처리 유틸리티
// =============================================================================

/**
 * Windows 에러 코드를 문자열로 변환 (ASCII)
 * @param error_code 에러 코드
 * @param buffer 출력 버퍼
 * @param buffer_size 버퍼 크기
 * @return 에러 메시지 문자열
 */
char* utils_get_last_error_string(DWORD error_code, char* buffer, size_t buffer_size);

/**
 * Winsock 에러를 문자열로 변환 (ASCII)
 * @param wsa_error WSA 에러 코드
 * @return 에러 메시지 상수 문자열
 */
const char* utils_winsock_error_to_string(int wsa_error);

// =============================================================================
// 랜덤/해시 유틸리티
// =============================================================================

/**
 * 간단한 랜덤 문자열 생성 (ASCII 영숫자)
 * @param buffer 출력 버퍼
 * @param length 생성할 문자열 길이 (null 제외)
 * @return 생성된 문자열 포인터
 */
char* utils_generate_random_string(char* buffer, size_t length);

/**
 * 간단한 해시 함수 (문자열용)
 * @param str 해시할 문자열
 * @return 32비트 해시값
 */
uint32_t utils_hash_string(const char* str);

// =============================================================================
// 로깅 함수 (내부 사용)
// =============================================================================

/**
 * 로그 출력 함수 (매크로에서 호출)
 * @param level 로그 레벨
 * @param file 파일명
 * @param line 라인 번호
 * @param fmt 포맷 문자열
 * @param ... 가변 인수
 */
void utils_log(log_level_t level, const char* file, int line, const char* fmt, ...);

/**
 * 로그 레벨 설정
 * @param level 설정할 로그 레벨
 */
void utils_set_log_level(log_level_t level);

/**
 * 로그 레벨을 문자열로 변환
 * @param level 로그 레벨
 * @return 로그 레벨 문자열
 */
const char* utils_log_level_to_string(log_level_t level);

#endif // UTILS_H