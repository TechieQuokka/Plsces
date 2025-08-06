#ifndef NETWORK_THREAD_H
#define NETWORK_THREAD_H

#include "client.h"
#include <windows.h>

// =============================================================================
// ��Ʈ��ũ ������ ���
// =============================================================================

#define NETWORK_THREAD_TIMEOUT_MS   100     // ��Ʈ��ũ Ÿ�Ӿƿ�
#define RECONNECT_TIMEOUT_MS        1000    // �翬�� üũ ����
#define COMMAND_QUEUE_TIMEOUT_MS    50      // ��� ť Ÿ�Ӿƿ�
#define SOCKET_SELECT_TIMEOUT_MS    100     // select Ÿ�Ӿƿ�

// =============================================================================
// ��Ʈ��ũ ������ �Լ���
// =============================================================================

/**
 * ��Ʈ��ũ ������ ���� ������
 * @param param Ŭ���̾�Ʈ �ν��Ͻ� ������ (LPVOID�� ĳ����)
 * @return ������ ���� �ڵ� (DWORD)
 */
DWORD WINAPI client_network_thread_proc(LPVOID param);

// =============================================================================
// ���� ���� �Լ��� (network_thread.c���� static���� ����)
// =============================================================================

// �� �Լ����� �����δ� static���� �����Ǹ�, �ܺο��� ���� ȣ������ ����
// ���⼭�� ����ȭ �������θ� ����

/*
static int network_thread_initialize(chat_client_t* client);
static void network_thread_cleanup(chat_client_t* client);
static int network_thread_connect_to_server(chat_client_t* client, const char* host, uint16_t port);
static int network_thread_send_auth_request(chat_client_t* client, const char* username);
static int network_thread_handle_incoming_message(chat_client_t* client, message_t* message);
static int network_thread_process_ui_command(chat_client_t* client, const ui_command_t* command);
static int network_thread_send_chat_message(chat_client_t* client, const char* message);
static int network_thread_request_user_list(chat_client_t* client);
static int network_thread_send_heartbeat_ack(chat_client_t* client);
static int network_thread_check_reconnect(chat_client_t* client);
static void network_thread_handle_connection_lost(chat_client_t* client, const char* reason);
*/

#endif // NETWORK_THREAD_H