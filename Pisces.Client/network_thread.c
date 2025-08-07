#include "network_thread.h"
#include <stdlib.h>
#include <string.h>

// =============================================================================
// 내부 헬퍼 함수 선언들
// =============================================================================

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

// =============================================================================
// 네트워크 스레드 메인 함수
// =============================================================================

DWORD WINAPI client_network_thread_proc(LPVOID param) {
    chat_client_t* client = (chat_client_t*)param;
    if (!client) {
        LOG_ERROR("Network thread: Invalid client parameter");
        return 1;
    }

    LOG_INFO("Network thread started");

    // 스레드 초기화
    if (network_thread_initialize(client) != 0) {
        LOG_ERROR("Failed to initialize network thread");
        return 1;
    }

    // UI 스레드에 준비 완료 신호
    SetEvent(client->network_ready_event);

    time_t last_reconnect_check = time(NULL);

    // 메인 루프
    while (!client->should_shutdown &&
        WaitForSingleObject(client->shutdown_event, 0) != WAIT_OBJECT_0) {

        // UI 명령 처리
        ui_command_t command;
        int command_result = command_queue_pop(client->command_queue, &command, COMMAND_QUEUE_TIMEOUT_MS);

        if (command_result == 0) {
            // 명령이 있음
            if (network_thread_process_ui_command(client, &command) != 0) {
                LOG_DEBUG("Failed to process UI command type %d", command.type);
            }
        }

        // 서버 메시지 수신 처리 (연결된 상태에서만)
        if (client->server_socket && client_is_connected(client)) {
            // 읽을 데이터가 있는지 확인
            if (network_socket_has_data(client->server_socket, 0) == 1) {
                message_t* received_msg = network_socket_recv_message(client->server_socket);

                if (received_msg) {
                    // 메시지 수신 성공
                    client->last_heartbeat = time(NULL);  // 활동 갱신

                    if (network_thread_handle_incoming_message(client, received_msg) != 0) {
                        LOG_DEBUG("Failed to handle incoming message");
                    }

                    message_destroy(received_msg);
                }
                else {
                    // 메시지 수신 실패 - 연결 끊김 가능성
                    if (!network_socket_is_connected(client->server_socket)) {
                        network_thread_handle_connection_lost(client, "Connection lost during message receive");
                    }
                }
            }
        }

        // 재연결 체크 (1초마다)
        time_t current_time = time(NULL);
        if (current_time - last_reconnect_check >= 1) {
            network_thread_check_reconnect(client);
            last_reconnect_check = current_time;
        }

        // CPU 사용률 제어
        Sleep(10);
    }

    // 정리
    network_thread_cleanup(client);

    LOG_INFO("Network thread exiting");
    return 0;
}

// =============================================================================
// 초기화 및 정리
// =============================================================================

static int network_thread_initialize(chat_client_t* client) {
    if (!client) {
        return -1;
    }

    // 네트워크 초기화 확인
    if (!network_is_initialized()) {
        if (network_initialize() != NETWORK_SUCCESS) {
            LOG_ERROR("Failed to initialize network in thread");
            return -1;
        }
    }

    LOG_DEBUG("Network thread initialized");
    return 0;
}

static void network_thread_cleanup(chat_client_t* client) {
    if (!client) {
        return;
    }

    // 서버 연결 해제
    if (client->server_socket) {
        network_socket_close(client->server_socket);
        network_socket_destroy(client->server_socket);
        client->server_socket = NULL;
    }

    LOG_DEBUG("Network thread cleaned up");
}

// =============================================================================
// 연결 관련 함수들
// =============================================================================

static int network_thread_connect_to_server(chat_client_t* client, const char* host, uint16_t port) {
    if (!client || utils_string_is_empty(host) || port == 0) {
        return -1;
    }

    LOG_INFO("Attempting to connect to %s:%d", host, port);

    // 기존 소켓 정리
    if (client->server_socket) {
        network_socket_close(client->server_socket);
        network_socket_destroy(client->server_socket);
        client->server_socket = NULL;
    }

    // 새 소켓 생성
    client->server_socket = network_socket_create(SOCKET_TYPE_TCP_CLIENT);
    if (!client->server_socket) {
        LOG_ERROR("Failed to create client socket");
        client_notify_connection_result(client, 0, "Failed to create socket");
        return -1;
    }

    // 논블로킹 모드 설정
    if (network_socket_set_nonblocking(client->server_socket) != NETWORK_SUCCESS) {
        LOG_ERROR("Failed to set non-blocking mode");
        network_socket_destroy(client->server_socket);
        client->server_socket = NULL;
        client_notify_connection_result(client, 0, "Failed to set socket mode");
        return -1;
    }

    // 연결 시도
    network_result_t connect_result = network_socket_connect(client->server_socket, host, port);

    if (connect_result == NETWORK_SUCCESS) {
        // 즉시 연결됨
        LOG_INFO("Connected to %s:%d", host, port);
        client_notify_connection_result(client, 1, NULL);
        return 0;
    }
    else if (connect_result == NETWORK_WOULD_BLOCK) {
        // 논블로킹 연결 진행 중
        LOG_DEBUG("Connection in progress...");

        // 연결 완료까지 대기 (타임아웃 있음)
        time_t start_time = time(NULL);
        time_t timeout = start_time + client->config.connect_timeout;

        while (time(NULL) < timeout && !client->should_shutdown) {
            network_result_t check_result = network_socket_connect_check(client->server_socket);

            if (check_result == NETWORK_SUCCESS) {
                LOG_INFO("Connected to %s:%d", host, port);
                client_notify_connection_result(client, 1, NULL);
                return 0;
            }
            else if (check_result != NETWORK_WOULD_BLOCK) {
                // 연결 실패
                LOG_ERROR("Connection failed during check");
                break;
            }

            Sleep(100);  // 100ms 대기
        }

        // 타임아웃 또는 실패
        LOG_ERROR("Connection timeout or failed");
        client_notify_connection_result(client, 0, "Connection timeout");
    }
    else {
        // 즉시 실패
        LOG_ERROR("Connection failed: %s", network_result_to_string(connect_result));
        client_notify_connection_result(client, 0, network_result_to_string(connect_result));
    }

    // 실패 시 소켓 정리
    network_socket_destroy(client->server_socket);
    client->server_socket = NULL;
    return -1;
}

static int network_thread_send_auth_request(chat_client_t* client, const char* username) {
    if (!client || utils_string_is_empty(username) || !client->server_socket) {
        return -1;
    }

    LOG_INFO("Sending authentication request for '%s'", username);

    // 연결 요청 메시지 생성
    message_t* auth_msg = message_create_connect_request(username);
    if (!auth_msg) {
        LOG_ERROR("Failed to create authentication message");
        client_notify_auth_result(client, 0, "Failed to create auth message");
        return -1;
    }

    // 메시지 전송
    network_result_t send_result = network_socket_send_message(client->server_socket, auth_msg);
    message_destroy(auth_msg);

    if (send_result != NETWORK_SUCCESS) {
        LOG_ERROR("Failed to send authentication message: %s",
            network_result_to_string(send_result));
        client_notify_auth_result(client, 0, "Failed to send auth message");
        return -1;
    }

    LOG_DEBUG("Authentication request sent");
    return 0;
}

// =============================================================================
// 메시지 처리 함수들
// =============================================================================

static int network_thread_handle_incoming_message(chat_client_t* client, message_t* message) {
    if (!client || !message) {
        return -1;
    }

    message_type_t msg_type = (message_type_t)ntohs(message->header.type);

    LOG_DEBUG("Handling incoming message type: %s", message_type_to_string(msg_type));

    switch (msg_type) {
    case MSG_CONNECT_RESPONSE:
    {
        if (ntohl(message->header.payload_size) >= sizeof(connect_response_payload_t)) {
            connect_response_payload_t* response = (connect_response_payload_t*)message->payload;
            response_code_t result = (response_code_t)ntohl(response->result);

            if (result == RESPONSE_SUCCESS) {
                LOG_INFO("Authentication successful");
                client_notify_auth_result(client, 1, response->message);
            }
            else {
                LOG_WARNING("Authentication failed: %s", response->message);
                client_notify_auth_result(client, 0, response->message);
            }
        }
        else {
            LOG_ERROR("Invalid connect response payload size");
            client_notify_auth_result(client, 0, "Invalid server response");
        }
    }
    break;

    case MSG_CHAT_BROADCAST:
    {
        if (ntohl(message->header.payload_size) >= sizeof(chat_message_payload_t)) {
            chat_message_payload_t* chat = (chat_message_payload_t*)message->payload;
            time_t timestamp = (time_t)ntohl(chat->timestamp);

            client_notify_chat_received(client, chat->sender_name, chat->message, timestamp);
        }
        else {
            LOG_WARNING("Invalid chat message payload size");
        }
    }
    break;

    case MSG_USER_LIST_RESPONSE:
    {
        uint32_t payload_size = ntohl(message->header.payload_size);
        if (payload_size > 0) {
            char* user_list = (char*)malloc(payload_size + 1);
            if (user_list) {
                memcpy(user_list, message->payload, payload_size);
                user_list[payload_size] = '\0';

                client_notify_user_list(client, user_list);
                free(user_list);
            }
        }
    }
    break;

    case MSG_USER_JOINED:
    {
        uint32_t payload_size = ntohl(message->header.payload_size);
        if (payload_size > 0 && payload_size < MAX_USERNAME_LENGTH) {
            char username[MAX_USERNAME_LENGTH];
            memcpy(username, message->payload, payload_size);
            username[payload_size] = '\0';

            client_notify_user_joined(client, username);
        }
    }
    break;

    case MSG_USER_LEFT:
    {
        uint32_t payload_size = ntohl(message->header.payload_size);
        if (payload_size > 0 && payload_size < MAX_USERNAME_LENGTH) {
            char username[MAX_USERNAME_LENGTH];
            memcpy(username, message->payload, payload_size);
            username[payload_size] = '\0';

            client_notify_user_left(client, username);
        }
    }
    break;

    case MSG_HEARTBEAT:
    {
        LOG_DEBUG("Received heartbeat, sending ACK");
        network_thread_send_heartbeat_ack(client);
    }
    break;

    case MSG_DISCONNECT:
    {
        LOG_INFO("Server requested disconnect");
        network_thread_handle_connection_lost(client, "Server requested disconnect");
    }
    break;

    case MSG_ERROR_GENERIC:
    case MSG_ERROR_PROTOCOL:
    case MSG_ERROR_AUTH:
    case MSG_ERROR_PERMISSION:
    {
        uint32_t payload_size = ntohl(message->header.payload_size);
        if (payload_size > 0) {
            char error_msg[512];
            size_t copy_size = (payload_size < sizeof(error_msg) - 1) ? payload_size : sizeof(error_msg) - 1;
            memcpy(error_msg, message->payload, copy_size);
            error_msg[copy_size] = '\0';

            LOG_ERROR("Server error (%s): %s",
                message_type_to_string(msg_type), error_msg);

            // 현재 상태에 따라 적절한 알림
            client_state_t state = client_get_current_state(client);
            if (state == CLIENT_STATE_AUTHENTICATING) {
                client_notify_auth_result(client, 0, error_msg);
            }
            else {
                client_set_last_error(client, error_msg);
            }
        }
    }
    break;

    default:
        LOG_WARNING("Unknown message type received: %d", msg_type);
        break;
    }

    return 0;
}

static int network_thread_process_ui_command(chat_client_t* client, const ui_command_t* command) {
    if (!client || !command) {
        return -1;
    }

    LOG_DEBUG("Processing UI command type: %d", command->type);

    switch (command->type) {
    case UI_CMD_CONNECT:
    {
        // 연결 명령 파싱 (host:port 또는 username)
        client_state_t state = client_get_current_state(client);

        if (state == CLIENT_STATE_CONNECTING) {
            // 호스트:포트 파싱
            char host_port[256];
            utils_string_copy(host_port, sizeof(host_port), command->data);

            char* colon_pos = strchr(host_port, ':');
            if (colon_pos) {
                *colon_pos = '\0';
                char* host = host_port;
                uint16_t port = (uint16_t)atoi(colon_pos + 1);

                return network_thread_connect_to_server(client, host, port);
            }
        }
    }
    break;

    case UI_CMD_AUTHENTICATE:
    {
        return network_thread_send_auth_request(client, command->data);
    }
    break;

    case UI_CMD_DISCONNECT:
    {
        if (client->server_socket) {
            // 연결 해제 메시지 전송
            message_t* disconnect_msg = message_create(MSG_DISCONNECT, NULL, 0);
            if (disconnect_msg) {
                network_socket_send_message(client->server_socket, disconnect_msg);
                message_destroy(disconnect_msg);
            }

            network_thread_handle_connection_lost(client, "User requested disconnect");
        }
    }
    break;

    case UI_CMD_SEND_CHAT:
        return network_thread_send_chat_message(client, command->data);

    case UI_CMD_REQUEST_USER_LIST:
        return network_thread_request_user_list(client);

    case UI_CMD_SHUTDOWN:
        LOG_INFO("Shutdown command received");
        client->should_shutdown = 1;
        break;

    default:
        LOG_WARNING("Unknown UI command type: %d", command->type);
        return -1;
    }

    return 0;
}

static int network_thread_send_chat_message(chat_client_t* client, const char* message) {
    if (!client || utils_string_is_empty(message) || !client->server_socket) {
        return -1;
    }

    if (!client_is_authenticated(client)) {
        LOG_ERROR("Cannot send chat message: not authenticated");
        return -1;
    }

    LOG_DEBUG("Sending chat message: %s", message);

    // 채팅 메시지 생성 (간단한 텍스트 페이로드)
    message_t* chat_msg = message_create(MSG_CHAT_SEND, message, strlen(message));
    if (!chat_msg) {
        LOG_ERROR("Failed to create chat message");
        return -1;
    }

    // 메시지 전송
    network_result_t send_result = network_socket_send_message(client->server_socket, chat_msg);
    message_destroy(chat_msg);

    if (send_result != NETWORK_SUCCESS) {
        LOG_ERROR("Failed to send chat message: %s", network_result_to_string(send_result));

        if (send_result == NETWORK_DISCONNECTED) {
            network_thread_handle_connection_lost(client, "Connection lost during chat send");
        }
        return -1;
    }

    return 0;
}

static int network_thread_request_user_list(chat_client_t* client) {
    if (!client || !client->server_socket) {
        return -1;
    }

    if (!client_is_authenticated(client)) {
        LOG_ERROR("Cannot request user list: not authenticated");
        return -1;
    }

    LOG_DEBUG("Requesting user list");

    // 사용자 목록 요청 메시지 생성
    message_t* userlist_msg = message_create(MSG_USER_LIST_REQUEST, NULL, 0);
    if (!userlist_msg) {
        LOG_ERROR("Failed to create user list request message");
        return -1;
    }

    // 메시지 전송
    network_result_t send_result = network_socket_send_message(client->server_socket, userlist_msg);
    message_destroy(userlist_msg);

    if (send_result != NETWORK_SUCCESS) {
        LOG_ERROR("Failed to send user list request: %s", network_result_to_string(send_result));

        if (send_result == NETWORK_DISCONNECTED) {
            network_thread_handle_connection_lost(client, "Connection lost during user list request");
        }
        return -1;
    }

    return 0;
}

static int network_thread_send_heartbeat_ack(chat_client_t* client) {
    if (!client || !client->server_socket) {
        return -1;
    }

    // 하트비트 ACK 메시지 생성
    message_t* heartbeat_ack = message_create(MSG_HEARTBEAT_ACK, NULL, 0);
    if (!heartbeat_ack) {
        LOG_ERROR("Failed to create heartbeat ACK message");
        return -1;
    }

    // 메시지 전송
    network_result_t send_result = network_socket_send_message(client->server_socket, heartbeat_ack);
    message_destroy(heartbeat_ack);

    if (send_result == NETWORK_SUCCESS) {
        client->last_heartbeat = time(NULL);
        LOG_DEBUG("Heartbeat ACK sent");
        return 0;
    }
    else {
        LOG_DEBUG("Failed to send heartbeat ACK: %s", network_result_to_string(send_result));

        if (send_result == NETWORK_DISCONNECTED) {
            network_thread_handle_connection_lost(client, "Connection lost during heartbeat ACK");
        }
        return -1;
    }
}

// =============================================================================
// 재연결 및 연결 상태 관리
// =============================================================================

static int network_thread_check_reconnect(chat_client_t* client) {
    if (!client || !client->config.auto_reconnect) {
        return 0;
    }

    client_state_t state = client_get_current_state(client);

    // 재연결이 필요한 상태인지 확인
    if (state == CLIENT_STATE_RECONNECTING) {
        static time_t last_reconnect_attempt = 0;
        time_t current_time = time(NULL);

        if (current_time - last_reconnect_attempt >= client->config.reconnect_interval) {
            LOG_INFO("Attempting to reconnect...");
            last_reconnect_attempt = current_time;

            // 재연결 시도
            int result = network_thread_connect_to_server(client,
                client->config.server_host,
                client->config.server_port);
            if (result == 0) {
                // 연결 성공 - 인증도 시도
                if (!utils_string_is_empty(client->config.username)) {
                    Sleep(100);  // 잠시 대기
                    network_thread_send_auth_request(client, client->config.username);
                }
            }
        }
    }

    return 0;
}

static void network_thread_handle_connection_lost(chat_client_t* client, const char* reason) {
    if (!client) {
        return;
    }

    LOG_WARNING("Handling connection lost: %s", reason ? reason : "Unknown");

    // 연결 끊김 알림
    client_notify_connection_lost(client, reason);

    // 자동 재연결이 활성화된 경우 재연결 상태로 전환
    if (client->config.auto_reconnect && !client->should_shutdown) {
        client_change_state(client, CLIENT_STATE_RECONNECTING, CLIENT_EVENT_RECONNECT_REQUESTED);
    }
}