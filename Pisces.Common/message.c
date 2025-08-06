#include "pch.h"
#include "message.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <winsock2.h>  // htonl, ntohl for byte order

// =============================================================================
// �޽��� ����/�Ҹ� �Լ���
// =============================================================================

message_t* message_create(message_type_t type, const void* payload, uint32_t payload_size) {
    // �Է� ��ȿ�� �˻�
    if (payload_size > MAX_MESSAGE_SIZE - sizeof(message_header_t)) {
        return NULL;
    }
    if (payload_size > 0 && payload == NULL) {
        return NULL;
    }

    // �޽��� �޸� �Ҵ�
    message_t* msg = (message_t*)malloc(sizeof(message_t));
    if (!msg) {
        return NULL;
    }

    // ��� �ʱ�ȭ
    msg->header.magic = htonl(PROTOCOL_MAGIC);
    msg->header.version = htons(PROTOCOL_VERSION);
    msg->header.type = htons((uint16_t)type);
    msg->header.payload_size = htonl(payload_size);

    // ���̷ε� ó��
    if (payload_size > 0) {
        msg->payload = (char*)malloc(payload_size);
        if (!msg->payload) {
            free(msg);
            return NULL;
        }
        memcpy(msg->payload, payload, payload_size);
    }
    else {
        msg->payload = NULL;
    }

    return msg;
}

void message_destroy(message_t* msg) {
    if (!msg) return;

    if (msg->payload) {
        free(msg->payload);
    }
    free(msg);
}

message_t* message_clone(const message_t* src) {
    if (!src) return NULL;

    uint32_t payload_size = ntohl(src->header.payload_size);
    return message_create((message_type_t)ntohs(src->header.type),
        src->payload, payload_size);
}

// =============================================================================
// ����ȭ/������ȭ �Լ���
// =============================================================================

int message_serialize(const message_t* msg, char* buffer, size_t buffer_size) {
    if (!msg || !buffer) {
        return -1;
    }

    uint32_t payload_size = ntohl(msg->header.payload_size);
    size_t total_size = sizeof(message_header_t) + payload_size;

    if (buffer_size < total_size) {
        return -1;  // ���۰� �ʹ� ����
    }

    // ��� ����
    memcpy(buffer, &msg->header, sizeof(message_header_t));

    // ���̷ε� ���� (�ִ� ���)
    if (payload_size > 0 && msg->payload) {
        memcpy(buffer + sizeof(message_header_t), msg->payload, payload_size);
    }

    return (int)total_size;
}

message_t* message_deserialize(const char* buffer, size_t buffer_size) {
    if (!buffer || buffer_size < sizeof(message_header_t)) {
        return NULL;
    }

    // ��� �б�
    message_header_t header;
    memcpy(&header, buffer, sizeof(message_header_t));

    // ��� ��ȿ�� ����
    if (!message_validate_header(&header)) {
        return NULL;
    }

    uint32_t payload_size = ntohl(header.payload_size);

    // ��ü �޽��� ũ�� Ȯ��
    if (buffer_size < sizeof(message_header_t) + payload_size) {
        return NULL;
    }

    // ���̷ε� ������ (�ִ� ���)
    const void* payload_ptr = (payload_size > 0) ?
        (buffer + sizeof(message_header_t)) : NULL;

    return message_create((message_type_t)ntohs(header.type),
        payload_ptr, payload_size);
}

int message_deserialize_header(const char* buffer, message_header_t* header) {
    if (!buffer || !header) {
        return -1;
    }

    memcpy(header, buffer, sizeof(message_header_t));
    return message_validate_header(header) ? 0 : -1;
}

// =============================================================================
// �޽��� ���� �Լ���
// =============================================================================

int message_validate(const message_t* msg) {
    if (!msg) return 0;

    if (!message_validate_header(&msg->header)) {
        return 0;
    }

    uint32_t payload_size = ntohl(msg->header.payload_size);

    // ���̷ε� ũ��� ������ �ϰ��� Ȯ��
    if (payload_size > 0 && !msg->payload) {
        return 0;
    }
    if (payload_size == 0 && msg->payload) {
        return 0;
    }

    return 1;
}

int message_validate_header(const message_header_t* header) {
    if (!header) return 0;

    // ���� �ѹ� Ȯ��
    if (ntohl(header->magic) != PROTOCOL_MAGIC) {
        return 0;
    }

    // ���� Ȯ��
    if (ntohs(header->version) != PROTOCOL_VERSION) {
        return 0;
    }

    // ���̷ε� ũ�� Ȯ��
    uint32_t payload_size = ntohl(header->payload_size);
    if (payload_size > MAX_MESSAGE_SIZE - sizeof(message_header_t)) {
        return 0;
    }

    // �޽��� Ÿ�� ��ȿ�� Ȯ�� (�⺻���� ���� üũ)
    uint16_t type = ntohs(header->type);
    if (type < MSG_SYSTEM_BASE || type > MSG_ERROR_INVALID_USERNAME) {
        return 0;
    }

    return 1;
}

// =============================================================================
// ���� �Լ��� (Ư�� Ÿ�� �޽��� ����)
// =============================================================================

message_t* message_create_connect_request(const char* username) {
    if (!username || strlen(username) >= MAX_USERNAME_LENGTH) {
        return NULL;
    }

    connect_request_payload_t payload = { 0 };
    strncpy_s(payload.username, sizeof(payload.username), username, _TRUNCATE);
    payload.client_version = htonl(PROTOCOL_VERSION);

    return message_create(MSG_CONNECT_REQUEST, &payload, sizeof(payload));
}

message_t* message_create_connect_response(response_code_t result, const char* message, uint32_t user_id) {
    connect_response_payload_t payload = { 0 };
    payload.result = htonl((uint32_t)result);
    payload.user_id = htonl(user_id);

    if (message) {
        strncpy_s(payload.message, sizeof(payload.message), message, _TRUNCATE);
    }

    return message_create(MSG_CONNECT_RESPONSE, &payload, sizeof(payload));
}

message_t* message_create_chat(uint32_t sender_id, const char* sender_name, const char* content) {
    if (!sender_name || !content) {
        return NULL;
    }
    if (strlen(sender_name) >= MAX_USERNAME_LENGTH) {
        return NULL;
    }

    chat_message_payload_t payload = { 0 };
    payload.sender_id = htonl(sender_id);
    payload.timestamp = htonl((uint32_t)time(NULL));

    strncpy_s(payload.sender_name, sizeof(payload.sender_name), sender_name, _TRUNCATE);
    strncpy_s(payload.message, sizeof(payload.message), content, _TRUNCATE);

    return message_create(MSG_CHAT_BROADCAST, &payload, sizeof(payload));
}

message_t* message_create_error(response_code_t error_code, const char* error_message) {
    error_payload_t payload = { 0 };
    payload.error_code = htonl((uint32_t)error_code);
    payload.error_context = 0;  // �⺻��

    if (error_message) {
        strncpy_s(payload.error_message, sizeof(payload.error_message),
            error_message, _TRUNCATE);
    }

    return message_create(MSG_ERROR_GENERIC, &payload, sizeof(payload));
}

// =============================================================================
// ����� ���� �Լ���
// =============================================================================

const char* message_type_to_string(message_type_t type) {
    switch (type) {
        // �ý��� �޽���
    case MSG_CONNECT_REQUEST:    return "CONNECT_REQUEST";
    case MSG_CONNECT_RESPONSE:   return "CONNECT_RESPONSE";
    case MSG_DISCONNECT:         return "DISCONNECT";
    case MSG_HEARTBEAT:          return "HEARTBEAT";
    case MSG_HEARTBEAT_ACK:      return "HEARTBEAT_ACK";

        // ���� �޽���
    case MSG_AUTH_REQUEST:       return "AUTH_REQUEST";
    case MSG_AUTH_RESPONSE:      return "AUTH_RESPONSE";
    case MSG_AUTH_FAILED:        return "AUTH_FAILED";

        // ä�� �޽���
    case MSG_CHAT_SEND:          return "CHAT_SEND";
    case MSG_CHAT_BROADCAST:     return "CHAT_BROADCAST";
    case MSG_CHAT_PRIVATE:       return "CHAT_PRIVATE";

        // ����� ����
    case MSG_USER_LIST_REQUEST:  return "USER_LIST_REQUEST";
    case MSG_USER_LIST_RESPONSE: return "USER_LIST_RESPONSE";
    case MSG_USER_JOINED:        return "USER_JOINED";
    case MSG_USER_LEFT:          return "USER_LEFT";

        // ���� �޽���
    case MSG_ERROR_GENERIC:      return "ERROR_GENERIC";
    case MSG_ERROR_PROTOCOL:     return "ERROR_PROTOCOL";
    case MSG_ERROR_AUTH:         return "ERROR_AUTH";
    case MSG_ERROR_PERMISSION:   return "ERROR_PERMISSION";
    case MSG_ERROR_SERVER_FULL:  return "ERROR_SERVER_FULL";
    case MSG_ERROR_INVALID_USERNAME: return "ERROR_INVALID_USERNAME";

    default:                     return "UNKNOWN";
    }
}

void message_print_debug(const message_t* msg) {
    if (!msg) {
        printf("[DEBUG] Message: NULL\n");
        return;
    }

    message_type_t type = (message_type_t)ntohs(msg->header.type);
    uint32_t payload_size = ntohl(msg->header.payload_size);

    printf("[DEBUG] Message Type: %s (%d)\n", message_type_to_string(type), type);
    printf("[DEBUG] Payload Size: %u bytes\n", payload_size);
    printf("[DEBUG] Total Size: %zu bytes\n", message_get_total_size(msg));

    if (payload_size > 0 && msg->payload) {
        printf("[DEBUG] Payload Preview: ");
        for (uint32_t i = 0; i < (payload_size < 32 ? payload_size : 32); i++) {
            if (msg->payload[i] >= 32 && msg->payload[i] <= 126) {
                printf("%c", msg->payload[i]);
            }
            else {
                printf(".");
            }
        }
        if (payload_size > 32) printf("...");
        printf("\n");
    }
}

size_t message_get_total_size(const message_t* msg) {
    if (!msg) return 0;
    return sizeof(message_header_t) + ntohl(msg->header.payload_size);
}