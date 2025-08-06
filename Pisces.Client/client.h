#ifndef CLIENT_H
#define CLIENT_H

#include "protocol.h"
#include "message.h"
#include "network.h"
#include "utils.h"
#include <windows.h>
#include <time.h>

// =============================================================================
// Ŭ���̾�Ʈ ��� ����
// =============================================================================

#define CLIENT_VERSION              "1.0.0"
#define DEFAULT_SERVER_HOST         "localhost"
#define DEFAULT_SERVER_PORT         8080        // �⺻ ���� ��Ʈ
#define CLIENT_RECONNECT_INTERVAL   5           // �翬�� ���� (��)
#define CLIENT_HEARTBEAT_TIMEOUT    30          // ��Ʈ��Ʈ Ÿ�Ӿƿ� (��)
#define CLIENT_CONNECT_TIMEOUT      10          // ���� Ÿ�Ӿƿ� (��)
#define MAX_MESSAGE_QUEUE_SIZE      100         // �޽��� ť �ִ� ũ��
#define MAX_COMMAND_LENGTH          256         // ��ɾ� �ִ� ����
#define MAX_CHAT_MESSAGE_LENGTH     1024        // ä�� �޽��� �ִ� ����

// =============================================================================
// Ŭ���̾�Ʈ ���� �ӽ�
// =============================================================================

// Ŭ���̾�Ʈ ���� ����
typedef enum {
    CLIENT_STATE_DISCONNECTED,      // ���� �ȵ�
    CLIENT_STATE_CONNECTING,        // ���� ��
    CLIENT_STATE_CONNECTED,         // ����� (���� ��)
    CLIENT_STATE_AUTHENTICATING,    // ���� ��
    CLIENT_STATE_AUTHENTICATED,     // ������ (ä�� ����)
    CLIENT_STATE_RECONNECTING,      // �翬�� ��
    CLIENT_STATE_DISCONNECTING,     // ���� ���� ��
    CLIENT_STATE_ERROR              // ���� ����
} client_state_t;

// ���� ��ȯ �̺�Ʈ
typedef enum {
    CLIENT_EVENT_CONNECT_REQUESTED,     // ���� ��û
    CLIENT_EVENT_CONNECTED,             // ���� �Ϸ�
    CLIENT_EVENT_CONNECTION_FAILED,     // ���� ����
    CLIENT_EVENT_AUTH_REQUESTED,        // ���� ��û
    CLIENT_EVENT_AUTH_SUCCESS,          // ���� ����
    CLIENT_EVENT_AUTH_FAILED,           // ���� ����
    CLIENT_EVENT_DISCONNECT_REQUESTED,  // ���� ���� ��û
    CLIENT_EVENT_CONNECTION_LOST,       // ���� ����
    CLIENT_EVENT_ERROR_OCCURRED,        // ���� �߻�
    CLIENT_EVENT_RECONNECT_REQUESTED    // �翬�� ��û
} client_event_t;

// =============================================================================
// Ŭ���̾�Ʈ ���� ����ü
// =============================================================================

typedef struct {
    char server_host[256];              // ���� ȣ��Ʈ
    uint16_t server_port;               // ���� ��Ʈ
    char username[MAX_USERNAME_LENGTH]; // ����ڸ�

    // ���� ����
    int auto_reconnect;                 // �ڵ� �翬�� ����
    int reconnect_interval;             // �翬�� ���� (��)
    int connect_timeout;                // ���� Ÿ�Ӿƿ� (��)
    int heartbeat_timeout;              // ��Ʈ��Ʈ Ÿ�Ӿƿ� (��)

    // UI ����
    int show_timestamps;                // Ÿ�ӽ����� ǥ��
    int show_system_messages;           // �ý��� �޽��� ǥ��
    log_level_t log_level;              // �α� ����

    // ��� ����
    int message_queue_size;             // �޽��� ť ũ��
    int network_buffer_size;            // ��Ʈ��ũ ���� ũ��
} client_config_t;

// =============================================================================
// ������ �� ��� ����ü
// =============================================================================

// UI �� Network ������ �޽���
typedef enum {
    UI_CMD_CONNECT,                     // ���� ����
    UI_CMD_DISCONNECT,                  // ���� ����
    UI_CMD_SEND_CHAT,                   // ä�� �޽��� ����
    UI_CMD_REQUEST_USER_LIST,           // ����� ��� ��û
    UI_CMD_SHUTDOWN                     // ����
} ui_command_type_t;

typedef struct {
    ui_command_type_t type;             // ��� Ÿ��
    char data[MAX_CHAT_MESSAGE_LENGTH]; // ��� ������
} ui_command_t;

// Network �� UI ������ �޽���
typedef enum {
    NET_EVENT_STATE_CHANGED,            // ���� ����
    NET_EVENT_CHAT_RECEIVED,            // ä�� �޽��� ����
    NET_EVENT_USER_LIST_RECEIVED,       // ����� ��� ����
    NET_EVENT_USER_JOINED,              // ����� ����
    NET_EVENT_USER_LEFT,                // ����� ����
    NET_EVENT_ERROR_OCCURRED,           // ���� �߻�
    NET_EVENT_CONNECTION_STATUS         // ���� ���� ������Ʈ
} network_event_type_t;

typedef struct {
    network_event_type_t type;          // �̺�Ʈ Ÿ��
    client_state_t new_state;           // �� ���� (���� ���� ��)
    char message[MAX_CHAT_MESSAGE_LENGTH]; // �޽��� ����
    char username[MAX_USERNAME_LENGTH];    // ����ڸ� (���� �ִ� ���)
    time_t timestamp;                   // Ÿ�ӽ�����
} network_event_t;

// =============================================================================
// �޽��� ť ����ü
// =============================================================================

typedef struct {
    ui_command_t* commands;             // ��� �迭
    int capacity;                       // �뷮
    int size;                          // ���� ũ��
    int front;                         // ť ��
    int rear;                          // ť ��
    CRITICAL_SECTION lock;             // ����ȭ
} command_queue_t;

typedef struct {
    network_event_t* events;           // �̺�Ʈ �迭
    int capacity;                      // �뷮
    int size;                         // ���� ũ��
    int front;                        // ť ��
    int rear;                         // ť ��
    CRITICAL_SECTION lock;            // ����ȭ
} event_queue_t;

// =============================================================================
// Ŭ���̾�Ʈ ��� ����ü
// =============================================================================

typedef struct {
    time_t connected_at;               // ���� �ð�
    time_t last_activity;              // ������ Ȱ��
    uint32_t messages_sent;            // ���� �޽��� ��
    uint32_t messages_received;        // ���� �޽��� ��
    uint64_t bytes_sent;               // ���� ����Ʈ ��
    uint64_t bytes_received;           // ���� ����Ʈ ��
    uint32_t reconnect_count;          // �翬�� Ƚ��
    uint32_t connection_failures;      // ���� ���� Ƚ��
} client_statistics_t;

// =============================================================================
// ���� Ŭ���̾�Ʈ ����ü
// =============================================================================

typedef struct {
    // Ŭ���̾�Ʈ ����
    client_state_t current_state;      // ���� ����
    client_config_t config;            // ����

    // ��Ʈ��ũ
    network_socket_t* server_socket;   // ���� ����

    // ������ ����
    HANDLE network_thread;             // ��Ʈ��ũ ������
    HANDLE ui_thread;                  // UI ������ (����)
    volatile int should_shutdown;      // ���� �÷���

    // ������ �� ���
    command_queue_t* command_queue;    // UI �� Network
    event_queue_t* event_queue;        // Network �� UI

    // ����ȭ
    CRITICAL_SECTION state_lock;       // ���� ��ȣ
    HANDLE network_ready_event;        // ��Ʈ��ũ ������ �غ� �Ϸ�
    HANDLE shutdown_event;             // ���� �̺�Ʈ

    // ���� ����
    time_t last_heartbeat;             // ������ ��Ʈ��Ʈ
    time_t connection_start_time;      // ���� ���� �ð�
    char last_error[256];              // ������ ���� �޽���

    // ���
    client_statistics_t stats;         // Ŭ���̾�Ʈ ���
} chat_client_t;

// =============================================================================
// Ŭ���̾�Ʈ ����������Ŭ �Լ���
// =============================================================================

/**
 * Ŭ���̾�Ʈ �ν��Ͻ� ����
 * @param config Ŭ���̾�Ʈ ���� (NULL�̸� �⺻��)
 * @return ������ Ŭ���̾�Ʈ, ���� �� NULL
 */
chat_client_t* client_create(const client_config_t* config);

/**
 * Ŭ���̾�Ʈ �ν��Ͻ� ����
 * @param client ������ Ŭ���̾�Ʈ
 */
void client_destroy(chat_client_t* client);

/**
 * Ŭ���̾�Ʈ ���� (������� ����)
 * @param client Ŭ���̾�Ʈ �ν��Ͻ�
 * @return ���� �� 0, ���� �� ����
 */
int client_start(chat_client_t* client);

/**
 * Ŭ���̾�Ʈ ���� (��� ������ ���� ���)
 * @param client Ŭ���̾�Ʈ �ν��Ͻ�
 * @return ���� �� 0, ���� �� ����
 */
int client_shutdown(chat_client_t* client);

/**
 * Ŭ���̾�Ʈ ���� ���� (UI �����忡�� ����)
 * @param client Ŭ���̾�Ʈ �ν��Ͻ�
 * @return ���� ���� �� 0, ���� �� ����
 */
int client_run(chat_client_t* client);

// =============================================================================
// ���� ���� �Լ���
// =============================================================================

/**
 * Ŭ���̾�Ʈ ���� ���� (������ ����)
 * @param client Ŭ���̾�Ʈ �ν��Ͻ�
 * @param new_state �� ����
 * @param event ���� ���� �̺�Ʈ
 * @return ���� �� 0, ���� �� ����
 */
int client_change_state(chat_client_t* client, client_state_t new_state, client_event_t event);

/**
 * ���� ���� ��ȸ (������ ����)
 * @param client Ŭ���̾�Ʈ �ν��Ͻ�
 * @return ���� ����
 */
client_state_t client_get_current_state(chat_client_t* client);

/**
 * ���¸� ���ڿ��� ��ȯ
 * @param state Ŭ���̾�Ʈ ����
 * @return ���� ���ڿ�
 */
const char* client_state_to_string(client_state_t state);

/**
 * �̺�Ʈ�� ���ڿ��� ��ȯ
 * @param event Ŭ���̾�Ʈ �̺�Ʈ
 * @return �̺�Ʈ ���ڿ�
 */
const char* client_event_to_string(client_event_t event);

// =============================================================================
// ���� �� ���� �Լ���
// =============================================================================

/**
 * ���� ���� ��û
 * @param client Ŭ���̾�Ʈ �ν��Ͻ�
 * @param host ���� ȣ��Ʈ
 * @param port ���� ��Ʈ
 * @return ���� �� 0, ���� �� ����
 */
int client_connect_to_server(chat_client_t* client, const char* host, uint16_t port);

/**
 * ����� ���� ��û
 * @param client Ŭ���̾�Ʈ �ν��Ͻ�
 * @param username ����ڸ�
 * @return ���� �� 0, ���� �� ����
 */
int client_authenticate(chat_client_t* client, const char* username);

/**
 * ���� ���� ����
 * @param client Ŭ���̾�Ʈ �ν��Ͻ�
 * @return ���� �� 0, ���� �� ����
 */
int client_disconnect(chat_client_t* client);

/**
 * ���� ���� Ȯ��
 * @param client Ŭ���̾�Ʈ �ν��Ͻ�
 * @return ����Ǿ� ������ 1, �ƴϸ� 0
 */
int client_is_connected(const chat_client_t* client);

/**
 * ���� ���� Ȯ��
 * @param client Ŭ���̾�Ʈ �ν��Ͻ�
 * @return �����Ǿ� ������ 1, �ƴϸ� 0
 */
int client_is_authenticated(const chat_client_t* client);

// =============================================================================
// �޽��� �ۼ��� �Լ���
// =============================================================================

/**
 * ä�� �޽��� ����
 * @param client Ŭ���̾�Ʈ �ν��Ͻ�
 * @param message ������ �޽���
 * @return ���� �� 0, ���� �� ����
 */
int client_send_chat_message(chat_client_t* client, const char* message);

/**
 * ����� ��� ��û
 * @param client Ŭ���̾�Ʈ �ν��Ͻ�
 * @return ���� �� 0, ���� �� ����
 */
int client_request_user_list(chat_client_t* client);

/**
 * ��Ʈ��Ʈ ���� ����
 * @param client Ŭ���̾�Ʈ �ν��Ͻ�
 * @return ���� �� 0, ���� �� ����
 */
int client_send_heartbeat_ack(chat_client_t* client);

// =============================================================================
// ť ���� �Լ���
// =============================================================================

/**
 * ��� ť ����
 * @param capacity ť �뷮
 * @return ������ ť, ���� �� NULL
 */
command_queue_t* command_queue_create(int capacity);

/**
 * ��� ť ����
 * @param queue ������ ť
 */
void command_queue_destroy(command_queue_t* queue);

/**
 * ��� ť�� �߰� (������ ����)
 * @param queue ť
 * @param command �߰��� ���
 * @return ���� �� 0, ���� �� ���� (ť ������)
 */
int command_queue_push(command_queue_t* queue, const ui_command_t* command);

/**
 * ��� ť���� ���� (������ ����)
 * @param queue ť
 * @param command ������ ��� (���)
 * @param timeout_ms Ÿ�Ӿƿ� (�и���, 0�̸� ��� ��ȯ)
 * @return ���� �� 0, ť ��������� -1, Ÿ�Ӿƿ� �� -2
 */
int command_queue_pop(command_queue_t* queue, ui_command_t* command, int timeout_ms);

/**
 * �̺�Ʈ ť ����
 * @param capacity ť �뷮
 * @return ������ ť, ���� �� NULL
 */
event_queue_t* event_queue_create(int capacity);

/**
 * �̺�Ʈ ť ����
 * @param queue ������ ť
 */
void event_queue_destroy(event_queue_t* queue);

/**
 * �̺�Ʈ ť�� �߰� (������ ����)
 * @param queue ť
 * @param event �߰��� �̺�Ʈ
 * @return ���� �� 0, ���� �� ���� (ť ������)
 */
int event_queue_push(event_queue_t* queue, const network_event_t* event);

/**
 * �̺�Ʈ ť���� ���� (������ ����)
 * @param queue ť
 * @param event ������ �̺�Ʈ (���)
 * @param timeout_ms Ÿ�Ӿƿ� (�и���, 0�̸� ��� ��ȯ)
 * @return ���� �� 0, ť ��������� -1, Ÿ�Ӿƿ� �� -2
 */
int event_queue_pop(event_queue_t* queue, network_event_t* event, int timeout_ms);

// =============================================================================
// ���� �� ��ƿ��Ƽ �Լ���
// =============================================================================

/**
 * �⺻ Ŭ���̾�Ʈ ���� ��ȯ
 * @return �⺻ ����
 */
client_config_t client_get_default_config(void);

/**
 * Ŭ���̾�Ʈ ���� ����
 * @param config ������ ����
 * @return ��ȿ�ϸ� 1, ��ȿ�ϸ� 0
 */
int client_validate_config(const client_config_t* config);

/**
 * Ŭ���̾�Ʈ ���� ���� ���
 * @param client Ŭ���̾�Ʈ �ν��Ͻ�
 */
void client_print_status(const chat_client_t* client);

/**
 * Ŭ���̾�Ʈ ��� ���
 * @param client Ŭ���̾�Ʈ �ν��Ͻ�
 */
void client_print_statistics(const chat_client_t* client);

/**
 * ������ ���� �޽��� ���� (������ ����)
 * @param client Ŭ���̾�Ʈ �ν��Ͻ�
 * @param error_message ���� �޽���
 */
void client_set_last_error(chat_client_t* client, const char* error_message);

/**
 * ������ ���� �޽��� ��ȸ (������ ����)
 * @param client Ŭ���̾�Ʈ �ν��Ͻ�
 * @param buffer ���� �޽��� ����
 * @param buffer_size ���� ũ��
 * @return ���� �޽��� ���ڿ�
 */
const char* client_get_last_error(chat_client_t* client, char* buffer, size_t buffer_size);

// =============================================================================
// ��Ʈ��ũ ������ �Լ� (���� ���Ͽ��� ����)
// =============================================================================

/**
 * ��Ʈ��ũ ������ ������
 * @param param Ŭ���̾�Ʈ �ν��Ͻ� ������
 * @return ������ ���� �ڵ�
 */
DWORD WINAPI client_network_thread_proc(LPVOID param);

// =============================================================================
// UI ���� �Լ��� (���� ���Ͽ��� ����)
// =============================================================================

/**
 * �ܼ� UI �ʱ�ȭ
 * @param client Ŭ���̾�Ʈ �ν��Ͻ�
 * @return ���� �� 0, ���� �� ����
 */
int client_ui_initialize(chat_client_t* client);

/**
 * �ܼ� UI ����
 * @param client Ŭ���̾�Ʈ �ν��Ͻ�
 */
void client_ui_cleanup(chat_client_t* client);

/**
 * ����� �Է� ó��
 * @param client Ŭ���̾�Ʈ �ν��Ͻ�
 * @param input �Է� ���ڿ�
 * @return ��� �����ϸ� 0, ���� ��û �� 1
 */
int client_ui_handle_input(chat_client_t* client, const char* input);

/**
 * ��Ʈ��ũ �̺�Ʈ ȭ�� ���
 * @param client Ŭ���̾�Ʈ �ν��Ͻ�
 * @param event ����� �̺�Ʈ
 */
void client_ui_display_event(chat_client_t* client, const network_event_t* event);

#endif // CLIENT_H