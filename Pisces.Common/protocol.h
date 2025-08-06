#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

// =============================================================================
// �������� �⺻ ���
// =============================================================================

#define PROTOCOL_MAGIC          0x43484154  // "CHAT" in hex
#define PROTOCOL_VERSION        1           // ���� �������� ����
#define MAX_MESSAGE_SIZE        4096        // �ִ� �޽��� ũ�� (4KB)
#define MAX_USERNAME_LENGTH     32          // �ִ� ����ڸ� ����
#define MAX_CLIENTS             100         // �ִ� ���� ������ ��
#define SERVER_DEFAULT_PORT     8080        // �⺻ ���� ��Ʈ

// =============================================================================
// �޽��� Ÿ�� ���� (ī�װ��� �з�)
// =============================================================================

typedef enum {
    // �ý��� �޽��� (1000����)
    MSG_SYSTEM_BASE = 1000,
    MSG_CONNECT_REQUEST = 1001,     // Ŭ���̾�Ʈ -> ����: ���� ��û
    MSG_CONNECT_RESPONSE = 1002,    // ���� -> Ŭ���̾�Ʈ: ���� ����
    MSG_DISCONNECT = 1003,          // �����: ���� ����
    MSG_HEARTBEAT = 1004,           // �����: ���� ���� Ȯ��
    MSG_HEARTBEAT_ACK = 1005,       // �����: ��Ʈ��Ʈ ����

    // ���� �޽��� (2000����)
    MSG_AUTH_BASE = 2000,
    MSG_AUTH_REQUEST = 2001,        // Ŭ���̾�Ʈ -> ����: ���� ��û
    MSG_AUTH_RESPONSE = 2002,       // ���� -> Ŭ���̾�Ʈ: ���� ����
    MSG_AUTH_FAILED = 2003,         // ���� -> Ŭ���̾�Ʈ: ���� ����

    // ä�� �޽��� (3000����)
    MSG_CHAT_BASE = 3000,
    MSG_CHAT_SEND = 3001,           // Ŭ���̾�Ʈ -> ����: ä�� �޽���
    MSG_CHAT_BROADCAST = 3002,      // ���� -> Ŭ���̾�Ʈ: ��ε�ĳ��Ʈ
    MSG_CHAT_PRIVATE = 3003,        // �����: ���� �޽��� (���� Ȯ��)

    // ����� ���� (4000����)
    MSG_USER_BASE = 4000,
    MSG_USER_LIST_REQUEST = 4001,   // Ŭ���̾�Ʈ -> ����: ����� ��� ��û
    MSG_USER_LIST_RESPONSE = 4002,  // ���� -> Ŭ���̾�Ʈ: ����� ��� ����
    MSG_USER_JOINED = 4003,         // ���� -> Ŭ���̾�Ʈ: ����� ����
    MSG_USER_LEFT = 4004,           // ���� -> Ŭ���̾�Ʈ: ����� ����

    // ���� �޽��� (9000����)
    MSG_ERROR_BASE = 9000,
    MSG_ERROR_GENERIC = 9001,       // �Ϲ� ����
    MSG_ERROR_PROTOCOL = 9002,      // �������� ����
    MSG_ERROR_AUTH = 9003,          // ���� ����
    MSG_ERROR_PERMISSION = 9004,    // ���� ����
    MSG_ERROR_SERVER_FULL = 9005,   // ���� ��ȭ ����
    MSG_ERROR_INVALID_USERNAME = 9006  // �߸��� ����ڸ�
} message_type_t;

// =============================================================================
// ���� �ڵ� ����
// =============================================================================

typedef enum {
    RESPONSE_SUCCESS = 0,           // ����
    RESPONSE_ERROR = -1,            // �Ϲ� ����
    RESPONSE_INVALID_INPUT = -2,    // �߸��� �Է�
    RESPONSE_NETWORK_ERROR = -3,    // ��Ʈ��ũ ����
    RESPONSE_AUTH_FAILED = -4,      // ���� ����
    RESPONSE_SERVER_FULL = -5,      // ���� ��ȭ
    RESPONSE_USERNAME_TAKEN = -6,   // ����ڸ� �ߺ�
    RESPONSE_NOT_CONNECTED = -7     // ������� ����
} response_code_t;

// =============================================================================
// �޽��� �켱���� (���� Ȯ���)
// =============================================================================

typedef enum {
    PRIORITY_LOW = 0,       // �Ϲ� ä�� �޽���
    PRIORITY_NORMAL = 1,    // �ý��� �޽���
    PRIORITY_HIGH = 2,      // ����, ���� ����
    PRIORITY_CRITICAL = 3   // ����, ��� �޽���
} message_priority_t;

// =============================================================================
// ��ƿ��Ƽ ��ũ��
// =============================================================================

// �޽��� Ÿ�� ī�װ� Ȯ��
#define IS_SYSTEM_MSG(type)     ((type) >= MSG_SYSTEM_BASE && (type) < MSG_AUTH_BASE)
#define IS_AUTH_MSG(type)       ((type) >= MSG_AUTH_BASE && (type) < MSG_CHAT_BASE)
#define IS_CHAT_MSG(type)       ((type) >= MSG_CHAT_BASE && (type) < MSG_USER_BASE)
#define IS_USER_MSG(type)       ((type) >= MSG_USER_BASE && (type) < MSG_ERROR_BASE)
#define IS_ERROR_MSG(type)      ((type) >= MSG_ERROR_BASE)

// �޽��� Ÿ���� ���ڿ��� ��ȯ (������)
const char* message_type_to_string(message_type_t type);

#endif // PROTOCOL_H