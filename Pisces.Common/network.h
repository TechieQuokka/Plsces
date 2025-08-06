#ifndef NETWORK_H
#define NETWORK_H

#include "protocol.h"
#include "message.h"
#include <stdint.h>

// =============================================================================
// ��Ʈ��ũ ��� ����
// =============================================================================

#define NETWORK_BUFFER_SIZE         8192    // �⺻ ��Ʈ��ũ ���� ũ��
#define MAX_PENDING_CONNECTIONS     10      // ��� ���� ���� ��
#define SOCKET_TIMEOUT_MS          5000     // ���� Ÿ�Ӿƿ� (�и���)
#define MAX_HOSTNAME_LENGTH         256     // �ִ� ȣ��Ʈ�� ����

// ���� �۾� ��� �ڵ�
typedef enum {
    NETWORK_SUCCESS = 0,                    // ����
    NETWORK_ERROR = -1,                     // �Ϲ� ����
    NETWORK_WOULD_BLOCK = -2,               // ���ŷ ��Ȳ (��õ� �ʿ�)
    NETWORK_DISCONNECTED = -3,              // ���� ����
    NETWORK_TIMEOUT = -4,                   // Ÿ�Ӿƿ�
    NETWORK_BUFFER_FULL = -5,               // ���� ������
    NETWORK_INVALID_SOCKET = -6,            // �߸��� ����
    NETWORK_CONNECTION_REFUSED = -7,        // ���� �ź�
    NETWORK_HOST_UNREACHABLE = -8,          // ȣ��Ʈ ���� �Ұ�
    NETWORK_INIT_FAILED = -9                // ��Ʈ��ũ �ʱ�ȭ ����
} network_result_t;

// ���� Ÿ��
typedef enum {
    SOCKET_TYPE_TCP_SERVER,
    SOCKET_TYPE_TCP_CLIENT,
    SOCKET_TYPE_UDP
} socket_type_t;

// ���� ����
typedef enum {
    SOCKET_STATE_CLOSED,
    SOCKET_STATE_LISTENING,
    SOCKET_STATE_CONNECTING,
    SOCKET_STATE_CONNECTED,
    SOCKET_STATE_DISCONNECTING,
    SOCKET_STATE_ERROR
} socket_state_t;

// =============================================================================
// ���� ���� ����ü
// =============================================================================

typedef struct {
    SOCKET handle;                          // ���� ���� �ڵ�
    socket_type_t type;                     // ���� Ÿ��
    socket_state_t state;                   // ���� ����

    // �ּ� ����
    struct sockaddr_in local_addr;          // ���� �ּ�
    struct sockaddr_in remote_addr;         // ���� �ּ�
    char remote_ip[16];                     // ���� IP (�� ǥ���)
    uint16_t remote_port;                   // ���� ��Ʈ

    // ���� ����
    char recv_buffer[NETWORK_BUFFER_SIZE];  // ���� ����
    char send_buffer[NETWORK_BUFFER_SIZE];  // �۽� ����
    int recv_buffer_pos;                    // ���� ���� ���� ��ġ
    int send_buffer_pos;                    // �۽� ���� ���� ��ġ

    // ��� ����
    uint64_t bytes_sent;                    // ������ ����Ʈ ��
    uint64_t bytes_received;                // ���� ����Ʈ ��
    uint32_t messages_sent;                 // ������ �޽��� ��
    uint32_t messages_received;             // ���� �޽��� ��
    time_t created_time;                    // ���� �ð�
    time_t last_activity;                   // ������ Ȱ�� �ð�
} network_socket_t;

// =============================================================================
// ��Ʈ��ũ �ʱ�ȭ/����
// =============================================================================

/**
 * Winsock ���̺귯�� �ʱ�ȭ
 * @return ���� �� NETWORK_SUCCESS, ���� �� NETWORK_INIT_FAILED
 */
network_result_t network_initialize(void);

/**
 * Winsock ���̺귯�� ����
 */
void network_cleanup(void);

/**
 * ��Ʈ��ũ�� �ʱ�ȭ�Ǿ����� Ȯ��
 * @return �ʱ�ȭ�Ǿ����� 1, �ƴϸ� 0
 */
int network_is_initialized(void);

// =============================================================================
// ���� ����/����
// =============================================================================

/**
 * ���ο� ��Ʈ��ũ ���� ����
 * @param type ���� Ÿ��
 * @return ������ ���� ������, ���� �� NULL
 */
network_socket_t* network_socket_create(socket_type_t type);

/**
 * ���� ����
 * @param sock ������ ����
 */
void network_socket_destroy(network_socket_t* sock);

/**
 * ������ ����ŷ ���� ����
 * @param sock ��� ����
 * @return ���� �� NETWORK_SUCCESS
 */
network_result_t network_socket_set_nonblocking(network_socket_t* sock);

/**
 * ���� ���� ���� (SO_REUSEADDR)
 * @param sock ��� ����
 * @param enable 1�̸� Ȱ��ȭ, 0�̸� ��Ȱ��ȭ
 * @return ���� �� NETWORK_SUCCESS
 */
network_result_t network_socket_set_reuse_addr(network_socket_t* sock, int enable);

/**
 * ���� Ÿ�Ӿƿ� ����
 * @param sock ��� ����
 * @param timeout_ms Ÿ�Ӿƿ� (�и���)
 * @return ���� �� NETWORK_SUCCESS
 */
network_result_t network_socket_set_timeout(network_socket_t* sock, int timeout_ms);

// =============================================================================
// ���� ���� �Լ���
// =============================================================================

/**
 * ���� ������ Ư�� ��Ʈ�� ���ε�
 * @param sock ���� ����
 * @param port ���ε��� ��Ʈ (0�̸� ���� ��Ʈ)
 * @param interface_ip ���ε��� �������̽� IP (NULL�̸� ��� �������̽�)
 * @return ���� �� NETWORK_SUCCESS
 */
network_result_t network_socket_bind(network_socket_t* sock, uint16_t port, const char* interface_ip);

/**
 * ���� ������ ������ ���� ��ȯ
 * @param sock ���� ����
 * @param backlog ��⿭ ũ�� (�⺻: MAX_PENDING_CONNECTIONS)
 * @return ���� �� NETWORK_SUCCESS
 */
network_result_t network_socket_listen(network_socket_t* sock, int backlog);

/**
 * Ŭ���̾�Ʈ ���� ���� (����ŷ)
 * @param server_sock ���� ����
 * @return �� Ŭ���̾�Ʈ ����, ������ ������ NULL
 */
network_socket_t* network_socket_accept(network_socket_t* server_sock);

// =============================================================================
// Ŭ���̾�Ʈ ���� �Լ���
// =============================================================================

/**
 * ������ ���� (����ŷ)
 * @param sock Ŭ���̾�Ʈ ����
 * @param hostname ���� ȣ��Ʈ�� �Ǵ� IP
 * @param port ���� ��Ʈ
 * @return NETWORK_SUCCESS, NETWORK_WOULD_BLOCK, �Ǵ� ���� �ڵ�
 */
network_result_t network_socket_connect(network_socket_t* sock, const char* hostname, uint16_t port);

/**
 * ������ �Ϸ�Ǿ����� Ȯ�� (����ŷ �����)
 * @param sock Ŭ���̾�Ʈ ����
 * @return ���� �Ϸ� �� NETWORK_SUCCESS, ���� ���̸� NETWORK_WOULD_BLOCK
 */
network_result_t network_socket_connect_check(network_socket_t* sock);

// =============================================================================
// ������ �ۼ���
// =============================================================================

/**
 * ������ ���� (����ŷ)
 * @param sock ����
 * @param data ������ ������
 * @param length ������ ����
 * @param bytes_sent ���� ���۵� ����Ʈ �� (���)
 * @return NETWORK_SUCCESS, NETWORK_WOULD_BLOCK, �Ǵ� ���� �ڵ�
 */
network_result_t network_socket_send(network_socket_t* sock, const void* data,
    int length, int* bytes_sent);

/**
 * ������ ���� (����ŷ)
 * @param sock ����
 * @param buffer ���� ����
 * @param buffer_size ���� ũ��
 * @param bytes_received ���� ���� ����Ʈ �� (���)
 * @return NETWORK_SUCCESS, NETWORK_WOULD_BLOCK, �Ǵ� ���� �ڵ�
 */
network_result_t network_socket_recv(network_socket_t* sock, void* buffer,
    int buffer_size, int* bytes_received);

/**
 * ��� �����͸� ������ ������ �ݺ� (���ŷ)
 * @param sock ����
 * @param data ������ ������
 * @param length ������ ����
 * @return ���� �� NETWORK_SUCCESS
 */
network_result_t network_socket_send_all(network_socket_t* sock, const void* data, int length);

/**
 * ������ ����Ʈ ����ŭ ������ ������ �ݺ� (���ŷ)
 * @param sock ����
 * @param buffer ���� ����
 * @param length ���� ����Ʈ ��
 * @return ���� �� NETWORK_SUCCESS
 */
network_result_t network_socket_recv_all(network_socket_t* sock, void* buffer, int length);

// =============================================================================
// �޽��� ���� �ۼ��� (protocol.h�� ����)
// =============================================================================

/**
 * �޽��� ���� (��� + ���̷ε�)
 * @param sock ����
 * @param msg ������ �޽���
 * @return ���� �� NETWORK_SUCCESS
 */
network_result_t network_socket_send_message(network_socket_t* sock, const message_t* msg);

/**
 * �޽��� ���� (��� ���� �а� ���̷ε� ũ�� Ȯ�� �� ��ü �б�)
 * @param sock ����
 * @return ���ŵ� �޽���, ���� �� NULL
 */
message_t* network_socket_recv_message(network_socket_t* sock);

// =============================================================================
// ���� ����
// =============================================================================

/**
 * ���� ���� ���� (graceful close)
 * @param sock ����
 * @return ���� �� NETWORK_SUCCESS
 */
network_result_t network_socket_close(network_socket_t* sock);

/**
 * ���� ������ ����ִ��� Ȯ��
 * @param sock ����
 * @return ����Ǿ� ������ 1, �ƴϸ� 0
 */
int network_socket_is_connected(const network_socket_t* sock);

/**
 * ���Ͽ��� ���� �����Ͱ� �ִ��� Ȯ�� (select ���)
 * @param sock ����
 * @param timeout_ms Ÿ�Ӿƿ� (�и���, 0�̸� ��� ��ȯ)
 * @return ������ ������ 1, ������ 0, ���� �� -1
 */
int network_socket_has_data(network_socket_t* sock, int timeout_ms);

/**
 * ������ ���� �������� Ȯ�� (select ���)
 * @param sock ����
 * @param timeout_ms Ÿ�Ӿƿ� (�и���)
 * @return ���� �����ϸ� 1, �ƴϸ� 0, ���� �� -1
 */
int network_socket_can_write(network_socket_t* sock, int timeout_ms);

// =============================================================================
// ��ƿ��Ƽ �Լ���
// =============================================================================

/**
 * IP �ּҸ� ���ڿ��� ��ȯ
 * @param addr �ּ� ����ü
 * @param buffer ��� ����
 * @param buffer_size ���� ũ��
 * @return ��ȯ�� IP ���ڿ�
 */
const char* network_addr_to_string(const struct sockaddr_in* addr, char* buffer, size_t buffer_size);

/**
 * ȣ��Ʈ���� IP �ּҷ� ��ȯ
 * @param hostname ȣ��Ʈ��
 * @param ip_buffer IP �ּ� ��� ����
 * @param buffer_size ���� ũ��
 * @return ���� �� NETWORK_SUCCESS
 */
network_result_t network_resolve_hostname(const char* hostname, char* ip_buffer, size_t buffer_size);

/**
 * ��Ʈ��ũ ��� �ڵ带 ���ڿ��� ��ȯ
 * @param result ��� �ڵ�
 * @return ��� ���ڿ�
 */
const char* network_result_to_string(network_result_t result);

/**
 * ���� ���¸� ���ڿ��� ��ȯ
 * @param state ���� ����
 * @return ���� ���ڿ�
 */
const char* network_socket_state_to_string(socket_state_t state);

/**
 * ���� ��� ���� ��� (������)
 * @param sock ����
 */
void network_socket_print_stats(const network_socket_t* sock);

/**
 * ���� IP �ּ� ���
 * @param buffer ��� ����
 * @param buffer_size ���� ũ��
 * @return ���� �� ���� IP ���ڿ�, ���� �� NULL
 */
const char* network_get_local_ip(char* buffer, size_t buffer_size);

#endif // NETWORK_H