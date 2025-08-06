#ifndef NETWORK_THREAD_H
#define NETWORK_THREAD_H

#include "client.h"
#include <windows.h>

// =============================================================================
// 네트워크 스레드 상수
// =============================================================================

#define NETWORK_THREAD_TIMEOUT_MS   100     // 네트워크 타임아웃
#define RECONNECT_TIMEOUT_MS        1000    // 재연결 체크 간격
#define COMMAND_QUEUE_TIMEOUT_MS    50      // 명령 큐 타임아웃
#define SOCKET_SELECT_TIMEOUT_MS    100     // select 타임아웃

// =============================================================================
// 네트워크 스레드 함수들
// =============================================================================

/**
 * 네트워크 스레드 메인 진입점
 * @param param 클라이언트 인스턴스 포인터 (LPVOID로 캐스팅)
 * @return 스레드 종료 코드 (DWORD)
 */
DWORD WINAPI client_network_thread_proc(LPVOID param);

// =============================================================================
// 내부 헬퍼 함수들 (network_thread.c에서 static으로 구현)
// =============================================================================

// 이 함수들은 실제로는 static으로 구현되며, 외부에서 직접 호출하지 않음
// 여기서는 문서화 목적으로만 선언

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