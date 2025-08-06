#ifndef SERVER_H
#define SERVER_H

#include "common_headers.h"
#include "protocol.h"
#include "message.h"
#include "network.h"
#include "utils.h"

#include <time.h>

// =============================================================================
// ���� ��� ����
// =============================================================================

#define DEFAULT_SERVER_PORT         8080        // �⺻ ���� ��Ʈ
#define MAX_SERVER_CLIENTS          64          // �ִ� Ŭ���̾�Ʈ �� (FD_SETSIZE)
#define SERVER_SELECT_TIMEOUT_MS    100         // select Ÿ�Ӿƿ� (�и���)
#define HEARTBEAT_INTERVAL_SEC      30          // ��Ʈ��Ʈ ���� (��)
#define CLIENT_TIMEOUT_SEC          60          // Ŭ���̾�Ʈ Ÿ�Ӿƿ� (��)
#define SERVER_SHUTDOWN_TIMEOUT_MS  5000        // ���� ���� Ÿ�Ӿƿ�

// =============================================================================
// ���� ���� �� ���� ����ü
// =============================================================================

// ���� ���� ����
typedef enum {
    SERVER_STATE_STOPPED,           // ������
    SERVER_STATE_STARTING,          // ���� ��
    SERVER_STATE_RUNNING,           // ���� ��
    SERVER_STATE_STOPPING,          // ���� ��
    SERVER_STATE_ERROR              // ���� ����
} server_state_t;

// ���� ���� ����ü
typedef struct {
    uint16_t port;                  // ���� ��Ʈ
    char bind_interface[16];        // ���ε��� �������̽� IP (�� ���ڿ��̸� ��� �������̽�)
    int max_clients;                // �ִ� Ŭ���̾�Ʈ ��
    int select_timeout_ms;          // select Ÿ�Ӿƿ�
    int heartbeat_interval_sec;     // ��Ʈ��Ʈ ����
    int client_timeout_sec;         // Ŭ���̾�Ʈ Ÿ�Ӿƿ�
    log_level_t log_level;          // �α� ����
    int enable_heartbeat;           // ��Ʈ��Ʈ Ȱ��ȭ ����
} server_config_t;

// Ŭ���̾�Ʈ ���� ����ü
typedef struct {
    uint32_t id;                    // Ŭ���̾�Ʈ ���� ID
    network_socket_t* socket;       // ��Ʈ��ũ ����
    char username[MAX_USERNAME_LENGTH]; // ����ڸ�
    time_t connected_at;            // ���� �ð�
    time_t last_activity;           // ������ Ȱ�� �ð�
    time_t last_heartbeat;          // ������ ��Ʈ��Ʈ �ð�

    // ���� ����
    int is_authenticated;           // ���� �Ϸ� ����
    int is_active;                  // Ȱ�� ���� ����

    // ��� ����
    uint32_t messages_sent;         // ���� �޽��� ��
    uint32_t messages_received;     // ���� �޽��� ��
} client_info_t;

// ���� ��� ����ü
typedef struct {
    time_t start_time;              // ���� ���� �ð�
    uint32_t total_connections;     // �� ���� ��
    uint32_t current_connections;   // ���� ���� ��
    uint32_t max_concurrent_connections; // �ִ� ���� ���� ��
    uint64_t total_messages;        // �� �޽��� ��
    uint64_t total_bytes_sent;      // �� �۽� ����Ʈ
    uint64_t total_bytes_received;  // �� ���� ����Ʈ
    uint32_t authentication_failures; // ���� ���� Ƚ��
    uint32_t protocol_errors;       // �������� ���� Ƚ��
} server_statistics_t;

// =============================================================================
// ���� ���� ����ü
// =============================================================================

typedef struct {
    // ���� ����
    server_state_t state;           // ���� ����
    server_config_t config;         // ���� ����

    // ��Ʈ��ũ
    network_socket_t* listen_socket; // ������ ����

    // Ŭ���̾�Ʈ ����
    client_info_t clients[MAX_SERVER_CLIENTS]; // Ŭ���̾�Ʈ �迭
    int client_count;               // ���� Ŭ���̾�Ʈ ��
    uint32_t next_client_id;        // ���� Ŭ���̾�Ʈ ID

    // select�� ���� ��ũ���� ��
    fd_set master_read_fds;         // ������ �б� FD ��
    fd_set working_read_fds;        // �۾��� �б� FD ��
    SOCKET max_fd;                  // �ִ� ���� ��ũ����

    // �ð� ����
    time_t last_heartbeat_check;    // ������ ��Ʈ��Ʈ üũ �ð�
    time_t last_cleanup;            // ������ ���� �۾� �ð�

    // ��� �� ����͸�
    server_statistics_t stats;      // ���� ���

    // ���� ��ȣ
    volatile int should_shutdown;   // ���� ��ȣ �÷���
} chat_server_t;

// =============================================================================
// ���� ����������Ŭ �Լ���
// =============================================================================

/**
 * ���� �ν��Ͻ� ���� �� �ʱ�ȭ
 * @param config ���� ���� (NULL�̸� �⺻�� ���)
 * @return ������ ���� �ν��Ͻ�, ���� �� NULL
 */
chat_server_t* server_create(const server_config_t* config);

/**
 * ���� �ν��Ͻ� ����
 * @param server ������ ���� �ν��Ͻ�
 */
void server_destroy(chat_server_t* server);

/**
 * ���� ���� (���� ���ε�, ������ ����)
 * @param server ���� �ν��Ͻ�
 * @return ���� �� 0, ���� �� ����
 */
int server_start(chat_server_t* server);

/**
 * ���� ���� (��� ���� ����, ���ҽ� ����)
 * @param server ���� �ν��Ͻ�
 * @return ���� �� 0, ���� �� ����
 */
int server_stop(chat_server_t* server);

/**
 * ���� ���� ���� ���� (���ŷ)
 * @param server ���� �ν��Ͻ�
 * @return ���� ���� �� 0, ���� �� ����
 */
int server_run(chat_server_t* server);

/**
 * ������ ���� ��ȣ ���� (�ٸ� �����忡�� ȣ�� ����)
 * @param server ���� �ν��Ͻ�
 */
void server_signal_shutdown(chat_server_t* server);

// =============================================================================
// Ŭ���̾�Ʈ ���� �Լ���
// =============================================================================

/**
 * �� Ŭ���̾�Ʈ �߰�
 * @param server ���� �ν��Ͻ�
 * @param client_socket Ŭ���̾�Ʈ ����
 * @return Ŭ���̾�Ʈ ID, ���� �� 0
 */
uint32_t server_add_client(chat_server_t* server, network_socket_t* client_socket);

/**
 * Ŭ���̾�Ʈ ����
 * @param server ���� �ν��Ͻ�
 * @param client_id Ŭ���̾�Ʈ ID
 * @return ���� �� 0, ���� �� ����
 */
int server_remove_client(chat_server_t* server, uint32_t client_id);

/**
 * Ŭ���̾�Ʈ �˻� (ID��)
 * @param server ���� �ν��Ͻ�
 * @param client_id Ŭ���̾�Ʈ ID
 * @return Ŭ���̾�Ʈ ���� ������, ������ NULL
 */
client_info_t* server_find_client_by_id(chat_server_t* server, uint32_t client_id);

/**
 * Ŭ���̾�Ʈ �˻� (��������)
 * @param server ���� �ν��Ͻ�
 * @param socket ����
 * @return Ŭ���̾�Ʈ ���� ������, ������ NULL
 */
client_info_t* server_find_client_by_socket(chat_server_t* server, network_socket_t* socket);

/**
 * Ŭ���̾�Ʈ �˻� (����ڸ�����)
 * @param server ���� �ν��Ͻ�
 * @param username ����ڸ�
 * @return Ŭ���̾�Ʈ ���� ������, ������ NULL
 */
client_info_t* server_find_client_by_username(chat_server_t* server, const char* username);

/**
 * Ȱ�� Ŭ���̾�Ʈ �� ��ȯ
 * @param server ���� �ν��Ͻ�
 * @return Ȱ�� Ŭ���̾�Ʈ ��
 */
int server_get_active_client_count(const chat_server_t* server);

// =============================================================================
// �޽��� ó�� �Լ���
// =============================================================================

/**
 * Ư�� Ŭ���̾�Ʈ���� �޽��� ����
 * @param server ���� �ν��Ͻ�
 * @param client_id ��� Ŭ���̾�Ʈ ID
 * @param message ������ �޽���
 * @return ���� �� 0, ���� �� ����
 */
int server_send_to_client(chat_server_t* server, uint32_t client_id, const message_t* message);

/**
 * ��� Ŭ���̾�Ʈ���� �޽��� ��ε�ĳ��Ʈ
 * @param server ���� �ν��Ͻ�
 * @param message ��ε�ĳ��Ʈ�� �޽���
 * @param exclude_client_id ������ Ŭ���̾�Ʈ ID (0�̸� ��� Ŭ���̾�Ʈ)
 * @return ���� ������ Ŭ���̾�Ʈ ��
 */
int server_broadcast_message(chat_server_t* server, const message_t* message, uint32_t exclude_client_id);

/**
 * ������ Ŭ���̾�Ʈ���Ը� �޽��� ��ε�ĳ��Ʈ
 * @param server ���� �ν��Ͻ�
 * @param message ��ε�ĳ��Ʈ�� �޽���
 * @param exclude_client_id ������ Ŭ���̾�Ʈ ID
 * @return ���� ������ Ŭ���̾�Ʈ ��
 */
int server_broadcast_to_authenticated(chat_server_t* server, const message_t* message, uint32_t exclude_client_id);

// =============================================================================
// �������� �� ���� �Լ���
// =============================================================================

/**
 * ��Ȱ�� Ŭ���̾�Ʈ ����
 * @param server ���� �ν��Ͻ�
 * @return ������ Ŭ���̾�Ʈ ��
 */
int server_cleanup_inactive_clients(chat_server_t* server);

/**
 * ��Ʈ��Ʈ Ȯ�� �� ����
 * @param server ���� �ν��Ͻ�
 * @return ó���� Ŭ���̾�Ʈ ��
 */
int server_check_heartbeats(chat_server_t* server);

/**
 * FD �� ������Ʈ (select��)
 * @param server ���� �ν��Ͻ�
 */
void server_update_fd_sets(chat_server_t* server);

// =============================================================================
// ���� �� ���� ��ȸ �Լ���
// =============================================================================

/**
 * �⺻ ���� ���� ����
 * @return �⺻ ���� ����ü
 */
server_config_t server_get_default_config(void);

/**
 * ���� ���� ��ȿ�� ����
 * @param config ������ ����
 * @return ��ȿ�ϸ� 1, ��ȿ�ϸ� 0
 */
int server_validate_config(const server_config_t* config);

/**
 * ���� ���¸� ���ڿ��� ��ȯ
 * @param state ���� ����
 * @return ���� ���ڿ�
 */
const char* server_state_to_string(server_state_t state);

/**
 * ���� ���� ���� ���
 * @param server ���� �ν��Ͻ�
 */
void server_print_status(const chat_server_t* server);

/**
 * ���� ��� ���� ���
 * @param server ���� �ν��Ͻ�
 */
void server_print_statistics(const chat_server_t* server);

/**
 * Ŭ���̾�Ʈ ��� ���
 * @param server ���� �ν��Ͻ�
 */
void server_print_client_list(const chat_server_t* server);

/**
 * ���� ���� �ð� ��ȯ (��)
 * @param server ���� �ν��Ͻ�
 * @return ���� �ð� (��)
 */
int server_get_uptime_seconds(const chat_server_t* server);

// =============================================================================
// ��ȣ ó�� �� ��ƿ��Ƽ
// =============================================================================

/**
 * SIGINT ��ȣ �ڵ鷯 ���� (Ctrl+C ó��)
 * @param server ���� �ν��Ͻ� (�������� �����)
 */
void server_setup_signal_handlers(chat_server_t* server);

/**
 * ���� ������ ���Ͽ��� �ε� (���� ���� ����)
 * @param filename ���� ���ϸ�
 * @param config �ε�� ���� ����ü
 * @return ���� �� 0, ���� �� ����
 */
int server_load_config_from_file(const char* filename, server_config_t* config);

/**
 * ���� PID�� ���Ͽ� ���� (���� ���� ����)
 * @param server ���� �ν��Ͻ�
 * @param pid_filename PID ���ϸ�
 * @return ���� �� 0, ���� �� ����
 */
int server_save_pid_file(const chat_server_t* server, const char* pid_filename);

#endif // SERVER_H