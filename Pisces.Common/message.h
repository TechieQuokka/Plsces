#ifndef MESSAGE_H
#define MESSAGE_H

#include "protocol.h"
#include <stdint.h>
#include <time.h>

// =============================================================================
// �淮ȭ�� �޽��� ����ü
// =============================================================================

// �⺻ �޽��� ��� (12����Ʈ�� �淮ȭ)
typedef struct {
    uint32_t magic;           // ���� �ѹ� (0x43484154)
    uint16_t version;         // �������� ����
    uint16_t type;            // �޽��� Ÿ�� (message_type_t)
    uint32_t payload_size;    // ���̷ε� ũ��
} message_header_t;

// ��ü �޽��� ����
typedef struct {
    message_header_t header;
    char* payload;            // ���� �Ҵ�� ���̷ε�
} message_t;

// =============================================================================
// ���� ���Ǵ� ���̷ε� ����ü��
// =============================================================================

// ���� ��û ���̷ε�
typedef struct {
    char username[MAX_USERNAME_LENGTH];
    uint32_t client_version;
} connect_request_payload_t;

// ���� ���� ���̷ε�
typedef struct {
    response_code_t result;
    char message[256];        // ����/���� �޽���
    uint32_t user_id;         // ���� �� �Ҵ�� ����� ID
} connect_response_payload_t;

// ���� ��û ���̷ε�
typedef struct {
    char username[MAX_USERNAME_LENGTH];
    char password[64];        // ���� �ؽð����� ��ü ����
} auth_request_payload_t;

// ���� ���� ���̷ε�
typedef struct {
    response_code_t result;
    char session_token[64];   // ���� �� ���� ��ū
    char message[256];
} auth_response_payload_t;

// ä�� �޽��� ���̷ε�
typedef struct {
    uint32_t sender_id;
    char sender_name[MAX_USERNAME_LENGTH];
    char message[MAX_MESSAGE_SIZE - sizeof(message_header_t) - 64]; // ���� ���� Ȱ��
    time_t timestamp;
} chat_message_payload_t;

// ����� ��� ���� ���̷ε�
typedef struct {
    uint32_t user_count;
    char users[][MAX_USERNAME_LENGTH];  // ���� ���� �迭
} user_list_payload_t;

// ���� �޽��� ���̷ε�
typedef struct {
    response_code_t error_code;
    char error_message[512];
    uint32_t error_context;   // �߰� ���ؽ�Ʈ ����
} error_payload_t;

// =============================================================================
// �޽��� ���� �Լ���
// =============================================================================

/**
 * �� �޽��� ���� (�޸� �Ҵ� ����)
 * @param type �޽��� Ÿ��
 * @param payload ���̷ε� ������
 * @param payload_size ���̷ε� ũ��
 * @return ������ �޽��� ������ (���� �� NULL)
 */
message_t* message_create(message_type_t type, const void* payload, uint32_t payload_size);

/**
 * �޽��� �޸� ����
 * @param msg ������ �޽���
 */
void message_destroy(message_t* msg);

/**
 * �޽��� ����
 * @param src ���� �޽���
 * @return ����� �޽��� (���� �� NULL)
 */
message_t* message_clone(const message_t* src);

// =============================================================================
// ����ȭ/������ȭ �Լ���
// =============================================================================

/**
 * �޽����� ����Ʈ �迭�� ����ȭ
 * @param msg ����ȭ�� �޽���
 * @param buffer ��� ����
 * @param buffer_size ���� ũ��
 * @return ���� ���� ����Ʈ �� (���� �� -1)
 */
int message_serialize(const message_t* msg, char* buffer, size_t buffer_size);

/**
 * ����Ʈ �迭���� �޽��� ������ȭ
 * @param buffer �Է� ����Ʈ �迭
 * @param buffer_size ���� ũ��
 * @return ������ȭ�� �޽��� (���� �� NULL)
 */
message_t* message_deserialize(const char* buffer, size_t buffer_size);

/**
 * �޽��� ����� ������ȭ (���̷ε� ũ�� Ȯ�ο�)
 * @param buffer �Է� ����Ʈ �迭
 * @param header ��µ� ���
 * @return ���� �� 0, ���� �� ����
 */
int message_deserialize_header(const char* buffer, message_header_t* header);

// =============================================================================
// �޽��� ���� �Լ���
// =============================================================================

/**
 * �޽��� ��ȿ�� ����
 * @param msg ������ �޽���
 * @return ��ȿ�ϸ� 1, ��ȿ�ϸ� 0
 */
int message_validate(const message_t* msg);

/**
 * �޽��� ��� ��ȿ�� ����
 * @param header ������ ���
 * @return ��ȿ�ϸ� 1, ��ȿ�ϸ� 0
 */
int message_validate_header(const message_header_t* header);

// =============================================================================
// ���� �Լ��� (Ư�� Ÿ�� �޽��� ����)
// =============================================================================

/**
 * ���� ��û �޽��� ����
 * @param username ����ڸ�
 * @return ������ �޽���
 */
message_t* message_create_connect_request(const char* username);

/**
 * ���� ���� �޽��� ����
 * @param result ��� �ڵ�
 * @param message �޽��� ����
 * @param user_id ����� ID
 * @return ������ �޽���
 */
message_t* message_create_connect_response(response_code_t result, const char* message, uint32_t user_id);

/**
 * ä�� �޽��� ����
 * @param sender_id �߽��� ID
 * @param sender_name �߽��� �̸�
 * @param content �޽��� ����
 * @return ������ �޽���
 */
message_t* message_create_chat(uint32_t sender_id, const char* sender_name, const char* content);

/**
 * ���� �޽��� ����
 * @param error_code ���� �ڵ�
 * @param error_message ���� �޽���
 * @return ������ �޽���
 */
message_t* message_create_error(response_code_t error_code, const char* error_message);

// =============================================================================
// ����� ���� �Լ���
// =============================================================================

/**
 * �޽��� Ÿ���� ���ڿ��� ��ȯ (protocol.h���� �����)
 * @param type �޽��� Ÿ��
 * @return Ÿ�� ���ڿ�
 */
const char* message_type_to_string(message_type_t type);

/**
 * �޽��� ������ �α׿����� ���
 * @param msg ����� �޽���
 */
void message_print_debug(const message_t* msg);

/**
 * �޽��� ũ�� ���
 * @param msg �޽���
 * @return ��ü �޽��� ũ�� (��� + ���̷ε�)
 */
size_t message_get_total_size(const message_t* msg);

#endif // MESSAGE_H