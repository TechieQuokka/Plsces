#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <time.h>
#include <stdint.h>

// =============================================================================
// �α� ���� ����
// =============================================================================

typedef enum {
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO = 1,
    LOG_LEVEL_WARNING = 2,
    LOG_LEVEL_ERROR = 3,
    LOG_LEVEL_CRITICAL = 4
} log_level_t;

// =============================================================================
// �α� ��ũ�� (ASCII ����)
// =============================================================================

// ���� �α� ���� (���� ����)
extern log_level_t g_current_log_level;

// �α� ��� ��ũ�ε�
#define LOG_DEBUG(fmt, ...)    utils_log(LOG_LEVEL_DEBUG, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)     utils_log(LOG_LEVEL_INFO, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_WARNING(fmt, ...)  utils_log(LOG_LEVEL_WARNING, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...)    utils_log(LOG_LEVEL_ERROR, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_CRITICAL(fmt, ...) utils_log(LOG_LEVEL_CRITICAL, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

// ���Ǻ� �α�
#define LOG_IF(condition, level, fmt, ...) \
    do { if (condition) utils_log(level, __FILE__, __LINE__, fmt, ##__VA_ARGS__); } while(0)

// =============================================================================
// �ð� ���� ��� �� �Լ�
// =============================================================================

#define TIME_STRING_SIZE    20      // "2025-08-06 15:30:45" + null
#define DATE_STRING_SIZE    11      // "2025-08-06" + null
#define TIME_ONLY_SIZE      9       // "15:30:45" + null

/**
 * ���� �ð��� ���ڿ��� ��ȯ (ASCII)
 * @param buffer ��� ���� (�ּ� TIME_STRING_SIZE)
 * @param buffer_size ���� ũ��
 * @return ���� �� buffer ������, ���� �� NULL
 */
char* utils_get_current_time_string(char* buffer, size_t buffer_size);

/**
 * ���� ��¥�� ���ڿ��� ��ȯ (ASCII)
 * @param buffer ��� ���� (�ּ� DATE_STRING_SIZE)
 * @param buffer_size ���� ũ��
 * @return ���� �� buffer ������, ���� �� NULL
 */
char* utils_get_current_date_string(char* buffer, size_t buffer_size);

/**
 * time_t�� ���ڿ��� ��ȯ (ASCII)
 * @param time_val �ð���
 * @param buffer ��� ���� (�ּ� TIME_STRING_SIZE)
 * @param buffer_size ���� ũ��
 * @return ���� �� buffer ������, ���� �� NULL
 */
char* utils_time_to_string(time_t time_val, char* buffer, size_t buffer_size);

/**
 * �и��� ���� ���� �ð� ��ȯ
 * @return �и��� ���� �ð�
 */
uint64_t utils_get_current_timestamp_ms(void);

/**
 * �� �ð� ������ ���� ��� (��)
 * @param start ���� �ð�
 * @param end �� �ð�
 * @return �ð� ���� (��)
 */
double utils_time_diff_seconds(time_t start, time_t end);

// =============================================================================
// ���ڿ� ��ƿ��Ƽ (ASCII ����)
// =============================================================================

/**
 * ���ڿ� �����ϰ� ���� (ASCII ����)
 * @param dest ��� ����
 * @param dest_size ��� ���� ũ��
 * @param src ���� ���ڿ�
 * @return ���� �� 0, ���� �� -1
 */
int utils_string_copy(char* dest, size_t dest_size, const char* src);

/**
 * ���ڿ� �����ϰ� ���� (ASCII ����)
 * @param dest ��� ����
 * @param dest_size ��� ���� ũ��
 * @param src �߰��� ���ڿ�
 * @return ���� �� 0, ���� �� -1
 */
int utils_string_concat(char* dest, size_t dest_size, const char* src);

/**
 * ���ڿ����� �յ� ���� ���� (ASCII)
 * @param str ��� ���ڿ� (���� ������)
 * @return ������ ���ڿ� ������
 */
char* utils_string_trim(char* str);

/**
 * ���ڿ��� �ҹ��ڷ� ��ȯ (ASCII)
 * @param str ��� ���ڿ� (���� ������)
 * @return ������ ���ڿ� ������
 */
char* utils_string_to_lower(char* str);

/**
 * ���ڿ��� �빮�ڷ� ��ȯ (ASCII)
 * @param str ��� ���ڿ� (���� ������)
 * @return ������ ���ڿ� ������
 */
char* utils_string_to_upper(char* str);

/**
 * ���ڿ��� ����ִ��� Ȯ�� (NULL ����)
 * @param str Ȯ���� ���ڿ�
 * @return ��������� 1, �ƴϸ� 0
 */
int utils_string_is_empty(const char* str);

/**
 * ���ڿ����� Ư�� ���� ���� ����
 * @param str ��� ���ڿ�
 * @param ch ã�� ����
 * @return ���� ����
 */
int utils_string_count_char(const char* str, char ch);

// =============================================================================
// �޸� ��ƿ��Ƽ
// =============================================================================

/**
 * �޸� �����ϰ� 0���� �ʱ�ȭ
 * @param ptr �޸� ������
 * @param size ũ��
 */
void utils_memory_zero(void* ptr, size_t size);

/**
 * �ΰ��� ������ ��� �޸𸮸� �����ϰ� �����
 * @param ptr �޸� ������
 * @param size ũ��
 */
void utils_memory_secure_zero(void* ptr, size_t size);

// =============================================================================
// ����Ʈ ��ȯ ��ƿ��Ƽ
// =============================================================================

/**
 * ����Ʈ ũ�⸦ �б� ���� ���ڿ��� ��ȯ (ASCII)
 * @param bytes ����Ʈ ��
 * @param buffer ��� ����
 * @param buffer_size ���� ũ��
 * @return ��ȯ�� ���ڿ� ������
 */
char* utils_bytes_to_human_readable(uint64_t bytes, char* buffer, size_t buffer_size);

/**
 * 16���� ���ڿ��� ��ȯ
 * @param data ��ȯ�� ������
 * @param data_size ������ ũ��
 * @param hex_buffer ��� ���� (�ּ� data_size * 2 + 1)
 * @param hex_buffer_size ���� ũ��
 * @return ���� �� hex_buffer, ���� �� NULL
 */
char* utils_bytes_to_hex(const void* data, size_t data_size,
    char* hex_buffer, size_t hex_buffer_size);

// =============================================================================
// ���� ó�� ��ƿ��Ƽ
// =============================================================================

/**
 * Windows ���� �ڵ带 ���ڿ��� ��ȯ (ASCII)
 * @param error_code ���� �ڵ�
 * @param buffer ��� ����
 * @param buffer_size ���� ũ��
 * @return ���� �޽��� ���ڿ�
 */
char* utils_get_last_error_string(DWORD error_code, char* buffer, size_t buffer_size);

/**
 * Winsock ������ ���ڿ��� ��ȯ (ASCII)
 * @param wsa_error WSA ���� �ڵ�
 * @return ���� �޽��� ��� ���ڿ�
 */
const char* utils_winsock_error_to_string(int wsa_error);

// =============================================================================
// ����/�ؽ� ��ƿ��Ƽ
// =============================================================================

/**
 * ������ ���� ���ڿ� ���� (ASCII ������)
 * @param buffer ��� ����
 * @param length ������ ���ڿ� ���� (null ����)
 * @return ������ ���ڿ� ������
 */
char* utils_generate_random_string(char* buffer, size_t length);

/**
 * ������ �ؽ� �Լ� (���ڿ���)
 * @param str �ؽ��� ���ڿ�
 * @return 32��Ʈ �ؽð�
 */
uint32_t utils_hash_string(const char* str);

// =============================================================================
// �α� �Լ� (���� ���)
// =============================================================================

/**
 * �α� ��� �Լ� (��ũ�ο��� ȣ��)
 * @param level �α� ����
 * @param file ���ϸ�
 * @param line ���� ��ȣ
 * @param fmt ���� ���ڿ�
 * @param ... ���� �μ�
 */
void utils_log(log_level_t level, const char* file, int line, const char* fmt, ...);

/**
 * �α� ���� ����
 * @param level ������ �α� ����
 */
void utils_set_log_level(log_level_t level);

/**
 * �α� ������ ���ڿ��� ��ȯ
 * @param level �α� ����
 * @return �α� ���� ���ڿ�
 */
const char* utils_log_level_to_string(log_level_t level);

#endif // UTILS_H