#include "server.h"
#include <stdlib.h>
#include <string.h>
#include <signal.h>

// =============================================================================
// 전역 변수 (신호 처리용)
// =============================================================================

static chat_server_t* g_server_instance = NULL;

static void server_handle_new_connection(chat_server_t* server);
static void server_handle_client_data(chat_server_t* server);
static void server_process_client_message(chat_server_t* server, client_info_t* client, message_t* message);
static void server_handle_connect_request(chat_server_t* server, client_info_t* client, message_t* message);
static void server_handle_chat_message(chat_server_t* server, client_info_t* client, message_t* message);
static void server_handle_user_list_request(chat_server_t* server, client_info_t* client, message_t* message);
static void server_handle_heartbeat_ack(chat_server_t* server, client_info_t* client, message_t* message);
static void server_handle_disconnect_request(chat_server_t* server, client_info_t* client, message_t* message);

// =============================================================================
// 서버 라이프사이클 함수들
// =============================================================================

server_config_t server_get_default_config(void) {
    server_config_t config;

    config.port = DEFAULT_SERVER_PORT;
    utils_string_copy(config.bind_interface, sizeof(config.bind_interface), "");  // 모든 인터페이스
    config.max_clients = MAX_SERVER_CLIENTS;
    config.select_timeout_ms = SERVER_SELECT_TIMEOUT_MS;
    config.heartbeat_interval_sec = HEARTBEAT_INTERVAL_SEC;
    config.client_timeout_sec = CLIENT_TIMEOUT_SEC;
    config.log_level = LOG_LEVEL_INFO;
    config.enable_heartbeat = 1;  // 하트비트 기본 활성화

    return config;
}

int server_validate_config(const server_config_t* config) {
    if (!config) {
        LOG_ERROR("Config is NULL");
        return 0;
    }

    // 포트 범위 확인
    if (config->port == 0 || config->port > 65535) {
        LOG_ERROR("Invalid port number: %d", config->port);
        return 0;
    }

    // 클라이언트 수 제한 확인
    if (config->max_clients <= 0 || config->max_clients > MAX_SERVER_CLIENTS) {
        LOG_ERROR("Invalid max_clients: %d (must be 1-%d)",
            config->max_clients, MAX_SERVER_CLIENTS);
        return 0;
    }

    // 타임아웃 값 확인
    if (config->select_timeout_ms < 10 || config->select_timeout_ms > 10000) {
        LOG_ERROR("Invalid select_timeout_ms: %d (must be 10-10000)",
            config->select_timeout_ms);
        return 0;
    }

    if (config->client_timeout_sec < 30) {
        LOG_ERROR("Invalid client_timeout_sec: %d (must be >= 30)",
            config->client_timeout_sec);
        return 0;
    }

    if (config->enable_heartbeat && config->heartbeat_interval_sec < 10) {
        LOG_ERROR("Invalid heartbeat_interval_sec: %d (must be >= 10)",
            config->heartbeat_interval_sec);
        return 0;
    }

    // 로그 레벨 확인
    if (config->log_level < LOG_LEVEL_DEBUG || config->log_level > LOG_LEVEL_CRITICAL) {
        LOG_ERROR("Invalid log_level: %d", config->log_level);
        return 0;
    }

    LOG_DEBUG("Server configuration validated successfully");
    return 1;
}

chat_server_t* server_create(const server_config_t* config) {
    LOG_INFO("Creating server instance...");

    // 네트워크 초기화 확인
    if (!network_is_initialized()) {
        if (network_initialize() != NETWORK_SUCCESS) {
            LOG_ERROR("Failed to initialize network");
            return NULL;
        }
    }

    // 서버 구조체 할당
    chat_server_t* server = (chat_server_t*)calloc(1, sizeof(chat_server_t));
    if (!server) {
        LOG_ERROR("Failed to allocate memory for server");
        return NULL;
    }

    // 설정 복사 및 검증
    if (config) {
        server->config = *config;
    }
    else {
        server->config = server_get_default_config();
        LOG_INFO("Using default server configuration");
    }

    if (!server_validate_config(&server->config)) {
        LOG_ERROR("Invalid server configuration");
        free(server);
        return NULL;
    }

    // 로그 레벨 설정
    utils_set_log_level(server->config.log_level);

    // 서버 상태 초기화
    server->state = SERVER_STATE_STOPPED;
    server->listen_socket = NULL;
    server->client_count = 0;
    server->next_client_id = 1;  // 0은 무효값
    server->should_shutdown = 0;

    // 클라이언트 배열 초기화
    for (int i = 0; i < MAX_SERVER_CLIENTS; i++) {
        server->clients[i].id = 0;  // 무효 상태
        server->clients[i].socket = NULL;
        server->clients[i].is_active = 0;
        server->clients[i].is_authenticated = 0;
    }

    // FD 셋 초기화
    FD_ZERO(&server->master_read_fds);
    FD_ZERO(&server->working_read_fds);
    server->max_fd = 0;

    // 시간 초기화
    time_t current_time = time(NULL);
    server->last_heartbeat_check = current_time;
    server->last_cleanup = current_time;

    // 통계 초기화
    memset(&server->stats, 0, sizeof(server_statistics_t));
    server->stats.start_time = current_time;

    LOG_INFO("Server instance created successfully");
    LOG_INFO("Configuration: port=%d, max_clients=%d, heartbeat=%s",
        server->config.port,
        server->config.max_clients,
        server->config.enable_heartbeat ? "enabled" : "disabled");

    return server;
}

void server_destroy(chat_server_t* server) {
    if (!server) {
        return;
    }

    LOG_INFO("Destroying server instance...");

    // 서버가 아직 실행 중이면 중지
    if (server->state == SERVER_STATE_RUNNING) {
        LOG_WARNING("Server still running, forcing stop...");
        server_stop(server);
    }

    // 모든 클라이언트 연결 해제
    for (int i = 0; i < MAX_SERVER_CLIENTS; i++) {
        if (server->clients[i].is_active && server->clients[i].socket) {
            LOG_DEBUG("Closing client %d connection during destroy", server->clients[i].id);
            network_socket_close(server->clients[i].socket);
            network_socket_destroy(server->clients[i].socket);
            server->clients[i].socket = NULL;
            server->clients[i].is_active = 0;
        }
    }

    // 리스닝 소켓 해제
    if (server->listen_socket) {
        network_socket_close(server->listen_socket);
        network_socket_destroy(server->listen_socket);
        server->listen_socket = NULL;
    }

    // 전역 참조 제거
    if (g_server_instance == server) {
        g_server_instance = NULL;
    }

    // 서버 구조체 해제
    free(server);

    LOG_INFO("Server instance destroyed");
}

// =============================================================================
// 상태 관리 및 유틸리티 함수들
// =============================================================================

const char* server_state_to_string(server_state_t state) {
    switch (state) {
    case SERVER_STATE_STOPPED:    return "STOPPED";
    case SERVER_STATE_STARTING:   return "STARTING";
    case SERVER_STATE_RUNNING:    return "RUNNING";
    case SERVER_STATE_STOPPING:   return "STOPPING";
    case SERVER_STATE_ERROR:      return "ERROR";
    default:                      return "UNKNOWN";
    }
}

int server_get_uptime_seconds(const chat_server_t* server) {
    if (!server) {
        return 0;
    }

    return (int)(time(NULL) - server->stats.start_time);
}

void server_print_status(const chat_server_t* server) {
    if (!server) {
        printf("Server: NULL\n");
        return;
    }

    char time_buffer[TIME_STRING_SIZE];
    utils_time_to_string(server->stats.start_time, time_buffer, sizeof(time_buffer));

    printf("=== Server Status ===\n");
    printf("State: %s\n", server_state_to_string(server->state));
    printf("Port: %d\n", server->config.port);
    printf("Started: %s\n", time_buffer);
    printf("Uptime: %d seconds\n", server_get_uptime_seconds(server));
    printf("Active clients: %d/%d\n", server_get_active_client_count(server), server->config.max_clients);
    printf("Should shutdown: %s\n", server->should_shutdown ? "Yes" : "No");
}

void server_print_statistics(const chat_server_t* server) {
    if (!server) {
        printf("Server statistics: NULL\n");
        return;
    }

    const server_statistics_t* stats = &server->stats;
    char bytes_sent_str[32], bytes_received_str[32];

    utils_bytes_to_human_readable(stats->total_bytes_sent, bytes_sent_str, sizeof(bytes_sent_str));
    utils_bytes_to_human_readable(stats->total_bytes_received, bytes_received_str, sizeof(bytes_received_str));

    printf("=== Server Statistics ===\n");
    printf("Total connections: %u\n", stats->total_connections);
    printf("Current connections: %u\n", stats->current_connections);
    printf("Max concurrent: %u\n", stats->max_concurrent_connections);
    printf("Total messages: %llu\n", stats->total_messages);
    printf("Bytes sent: %s\n", bytes_sent_str);
    printf("Bytes received: %s\n", bytes_received_str);
    printf("Auth failures: %u\n", stats->authentication_failures);
    printf("Protocol errors: %u\n", stats->protocol_errors);
}

void server_print_client_list(const chat_server_t* server) {
    if (!server) {
        printf("Client list: Server is NULL\n");
        return;
    }

    printf("=== Client List ===\n");
    printf("Active clients: %d/%d\n", server_get_active_client_count(server), server->config.max_clients);

    int active_count = 0;
    for (int i = 0; i < MAX_SERVER_CLIENTS; i++) {
        const client_info_t* client = &server->clients[i];
        if (client->is_active && client->socket) {
            char connected_time[TIME_STRING_SIZE];
            utils_time_to_string(client->connected_at, connected_time, sizeof(connected_time));

            printf("Client %u: %s (%s:%d) - Connected: %s - Auth: %s - Messages: %u/%u\n",
                client->id,
                utils_string_is_empty(client->username) ? "[Anonymous]" : client->username,
                client->socket->remote_ip,
                client->socket->remote_port,
                connected_time,
                client->is_authenticated ? "Yes" : "No",
                client->messages_sent,
                client->messages_received);
            active_count++;
        }
    }

    if (active_count == 0) {
        printf("No active clients\n");
    }
}

int server_get_active_client_count(const chat_server_t* server) {
    if (!server) {
        return 0;
    }

    int count = 0;
    for (int i = 0; i < MAX_SERVER_CLIENTS; i++) {
        if (server->clients[i].is_active) {
            count++;
        }
    }

    return count;
}

// =============================================================================
// 신호 처리
// =============================================================================

static void signal_handler(int signum) {
    if (signum == SIGINT) {
        LOG_INFO("Received SIGINT (Ctrl+C), initiating graceful shutdown...");
        if (g_server_instance) {
            server_signal_shutdown(g_server_instance);
        }
    }
}

void server_setup_signal_handlers(chat_server_t* server) {
    g_server_instance = server;
    signal(SIGINT, signal_handler);
    LOG_DEBUG("Signal handlers set up");
}

void server_signal_shutdown(chat_server_t* server) {
    if (server) {
        server->should_shutdown = 1;
        LOG_INFO("Shutdown signal sent to server");
    }
}

// =============================================================================
// 설정 파일 처리 (기본 구현 - 추후 확장 예정)
// =============================================================================

int server_load_config_from_file(const char* filename, server_config_t* config) {
    if (!filename || !config) {
        return -1;
    }

    // TODO: 실제 파일 파싱 구현
    LOG_WARNING("Config file loading not implemented yet: %s", filename);
    *config = server_get_default_config();
    return 0;
}

int server_save_pid_file(const chat_server_t* server, const char* pid_filename) {
    if (!server || !pid_filename) {
        return -1;
    }

    // TODO: PID 파일 저장 구현
    LOG_WARNING("PID file saving not implemented yet: %s", pid_filename);
    return 0;
}

// =============================================================================
// 서버 시작/중지 및 실행 로직
// =============================================================================

int server_start(chat_server_t* server) {
    if (!server) {
        LOG_ERROR("Server is NULL");
        return -1;
    }

    if (server->state != SERVER_STATE_STOPPED) {
        LOG_ERROR("Server is not in stopped state (current: %s)",
            server_state_to_string(server->state));
        return -1;
    }

    LOG_INFO("Starting server on port %d...", server->config.port);
    server->state = SERVER_STATE_STARTING;

    // 리스닝 소켓 생성
    server->listen_socket = network_socket_create(SOCKET_TYPE_TCP_SERVER);
    if (!server->listen_socket) {
        LOG_ERROR("Failed to create listen socket");
        server->state = SERVER_STATE_ERROR;
        return -1;
    }

    // 소켓 옵션 설정
    if (network_socket_set_reuse_addr(server->listen_socket, 1) != NETWORK_SUCCESS) {
        LOG_WARNING("Failed to set SO_REUSEADDR (continuing anyway)");
    }

    if (network_socket_set_nonblocking(server->listen_socket) != NETWORK_SUCCESS) {
        LOG_ERROR("Failed to set non-blocking mode");
        network_socket_destroy(server->listen_socket);
        server->listen_socket = NULL;
        server->state = SERVER_STATE_ERROR;
        return -1;
    }

    // 소켓 바인딩
    const char* bind_interface = utils_string_is_empty(server->config.bind_interface) ?
        NULL : server->config.bind_interface;

    network_result_t bind_result = network_socket_bind(server->listen_socket,
        server->config.port,
        bind_interface);
    if (bind_result != NETWORK_SUCCESS) {
        LOG_ERROR("Failed to bind to port %d: %s",
            server->config.port, network_result_to_string(bind_result));
        network_socket_destroy(server->listen_socket);
        server->listen_socket = NULL;
        server->state = SERVER_STATE_ERROR;
        return -1;
    }

    // 리스닝 시작
    if (network_socket_listen(server->listen_socket, MAX_PENDING_CONNECTIONS) != NETWORK_SUCCESS) {
        LOG_ERROR("Failed to start listening");
        network_socket_destroy(server->listen_socket);
        server->listen_socket = NULL;
        server->state = SERVER_STATE_ERROR;
        return -1;
    }

    // FD 셋 초기화
    FD_ZERO(&server->master_read_fds);
    FD_SET(server->listen_socket->handle, &server->master_read_fds);
    server->max_fd = server->listen_socket->handle;

    // 시간 초기화
    time_t current_time = time(NULL);
    server->last_heartbeat_check = current_time;
    server->last_cleanup = current_time;
    server->should_shutdown = 0;

    server->state = SERVER_STATE_RUNNING;
    LOG_INFO("Server started successfully on port %d", server->config.port);

    return 0;
}

int server_stop(chat_server_t* server) {
    if (!server) {
        LOG_ERROR("Server is NULL");
        return -1;
    }

    if (server->state != SERVER_STATE_RUNNING) {
        LOG_WARNING("Server is not running (current state: %s)",
            server_state_to_string(server->state));
        return 0;  // 이미 중지된 상태는 성공으로 간주
    }

    LOG_INFO("Stopping server...");
    server->state = SERVER_STATE_STOPPING;

    // 모든 클라이언트에게 연결 종료 알림
    message_t* disconnect_msg = message_create(MSG_DISCONNECT, NULL, 0);
    if (disconnect_msg) {
        server_broadcast_message(server, disconnect_msg, 0);  // 모든 클라이언트
        message_destroy(disconnect_msg);
    }

    // 잠시 대기하여 클라이언트들이 정상적으로 연결을 끊을 수 있도록 함
    Sleep(1000);

    // 모든 클라이언트 연결 강제 해제
    int closed_clients = 0;
    for (int i = 0; i < MAX_SERVER_CLIENTS; i++) {
        if (server->clients[i].is_active && server->clients[i].socket) {
            LOG_DEBUG("Closing connection to client %d", server->clients[i].id);
            network_socket_close(server->clients[i].socket);
            network_socket_destroy(server->clients[i].socket);
            server->clients[i].socket = NULL;
            server->clients[i].is_active = 0;
            closed_clients++;
        }
    }

    if (closed_clients > 0) {
        LOG_INFO("Closed %d client connections", closed_clients);
    }

    // 리스닝 소켓 해제
    if (server->listen_socket) {
        network_socket_close(server->listen_socket);
        network_socket_destroy(server->listen_socket);
        server->listen_socket = NULL;
    }

    // FD 셋 초기화
    FD_ZERO(&server->master_read_fds);
    FD_ZERO(&server->working_read_fds);
    server->max_fd = 0;
    server->client_count = 0;

    server->state = SERVER_STATE_STOPPED;
    LOG_INFO("Server stopped successfully");

    return 0;
}

int server_run(chat_server_t* server) {
    if (!server) {
        LOG_ERROR("Server is NULL");
        return -1;
    }

    if (server->state != SERVER_STATE_RUNNING) {
        LOG_ERROR("Server is not running (state: %s)", server_state_to_string(server->state));
        return -1;
    }

    LOG_INFO("Server main loop started (PID: %d)", GetCurrentProcessId());

    struct timeval timeout;
    time_t last_log_time = time(NULL);

    // 메인 루프
    while (!server->should_shutdown && server->state == SERVER_STATE_RUNNING) {
        // select용 타임아웃 설정
        timeout.tv_sec = server->config.select_timeout_ms / 1000;
        timeout.tv_usec = (server->config.select_timeout_ms % 1000) * 1000;

        // 작업용 FD 셋 복사
        server->working_read_fds = server->master_read_fds;

        // select 호출
        int activity = select(0, &server->working_read_fds, NULL, NULL, &timeout);

        if (activity == SOCKET_ERROR) {
            int error = WSAGetLastError();
            if (error == WSAEINTR) {
                LOG_DEBUG("Select interrupted by signal");
                continue;
            }
            LOG_ERROR("Select failed: %s", utils_winsock_error_to_string(error));
            server->state = SERVER_STATE_ERROR;
            break;
        }

        time_t current_time = time(NULL);

        if (activity > 0) {
            // 새로운 연결 확인
            if (FD_ISSET(server->listen_socket->handle, &server->working_read_fds)) {
                server_handle_new_connection(server);
            }

            // 기존 클라이언트들의 데이터 확인
            server_handle_client_data(server);
        }

        // 주기적인 유지보수 작업 (5초마다)
        if (current_time - server->last_cleanup >= 5) {
            server_cleanup_inactive_clients(server);
            server->last_cleanup = current_time;
        }

        // 하트비트 확인
        if (server->config.enable_heartbeat &&
            current_time - server->last_heartbeat_check >= server->config.heartbeat_interval_sec) {
            server_check_heartbeats(server);
            server->last_heartbeat_check = current_time;
        }

        // FD 셋 업데이트
        server_update_fd_sets(server);

        // 주기적 상태 로그 (1분마다)
        if (current_time - last_log_time >= 60) {
            LOG_INFO("Server running - Active clients: %d/%d, Uptime: %d seconds",
                server_get_active_client_count(server),
                server->config.max_clients,
                server_get_uptime_seconds(server));
            last_log_time = current_time;
        }
    }

    if (server->should_shutdown) {
        LOG_INFO("Server main loop exiting due to shutdown signal");
    }
    else {
        LOG_ERROR("Server main loop exiting due to error state");
    }

    return (server->state == SERVER_STATE_ERROR) ? -1 : 0;
}

// =============================================================================
// 내부 헬퍼 함수들 (static)
// =============================================================================

static void server_handle_new_connection(chat_server_t* server) {
    if (!server || !server->listen_socket) {
        return;
    }

    // 최대 클라이언트 수 확인
    if (server_get_active_client_count(server) >= server->config.max_clients) {
        // 연결을 받고 즉시 거절 메시지 전송 후 연결 해제
        network_socket_t* temp_socket = network_socket_accept(server->listen_socket);
        if (temp_socket) {
            LOG_WARNING("Server full, rejecting connection from %s:%d",
                temp_socket->remote_ip, temp_socket->remote_port);

            message_t* error_msg = message_create_error(RESPONSE_SERVER_FULL,
                "Server is full. Please try again later.");
            if (error_msg) {
                network_socket_send_message(temp_socket, error_msg);
                message_destroy(error_msg);
            }

            Sleep(100);  // 메시지 전송 시간 확보
            network_socket_close(temp_socket);
            network_socket_destroy(temp_socket);
        }
        return;
    }

    // 새 연결 수락
    network_socket_t* client_socket = network_socket_accept(server->listen_socket);
    if (!client_socket) {
        return;  // 연결이 없거나 오류 (논블로킹 모드에서 정상)
    }

    LOG_INFO("New connection from %s:%d", client_socket->remote_ip, client_socket->remote_port);

    // 클라이언트 소켓을 논블로킹 모드로 설정
    if (network_socket_set_nonblocking(client_socket) != NETWORK_SUCCESS) {
        LOG_ERROR("Failed to set client socket to non-blocking mode");
        network_socket_close(client_socket);
        network_socket_destroy(client_socket);
        return;
    }

    // 서버에 클라이언트 추가
    uint32_t client_id = server_add_client(server, client_socket);
    if (client_id == 0) {
        LOG_ERROR("Failed to add client to server");
        network_socket_close(client_socket);
        network_socket_destroy(client_socket);
        return;
    }

    // 통계 업데이트
    server->stats.total_connections++;
    server->stats.current_connections = server_get_active_client_count(server);
    if (server->stats.current_connections > server->stats.max_concurrent_connections) {
        server->stats.max_concurrent_connections = server->stats.current_connections;
    }

    LOG_INFO("Client %d connected successfully (%d/%d active)",
        client_id, server->stats.current_connections, server->config.max_clients);
}

static void server_handle_client_data(chat_server_t* server) {
    if (!server) {
        return;
    }

    // 모든 활성 클라이언트 확인
    for (int i = 0; i < MAX_SERVER_CLIENTS; i++) {
        client_info_t* client = &server->clients[i];

        if (!client->is_active || !client->socket) {
            continue;
        }

        // 이 클라이언트에게 읽을 데이터가 있는지 확인
        if (FD_ISSET(client->socket->handle, &server->working_read_fds)) {
            // 메시지 수신 시도
            message_t* received_msg = network_socket_recv_message(client->socket);

            if (received_msg) {
                // 메시지 수신 성공
                client->last_activity = time(NULL);
                client->messages_received++;
                server->stats.total_messages++;

                LOG_DEBUG("Received message type %s from client %d",
                    message_type_to_string((message_type_t)ntohs(received_msg->header.type)),
                    client->id);

                // 메시지 처리 (Part 3에서 구현될 message_handler 호출)
                server_process_client_message(server, client, received_msg);

                message_destroy(received_msg);

            }
            else {
                // 메시지 수신 실패 또는 연결 종료
                int socket_state = network_socket_is_connected(client->socket);
                if (!socket_state) {
                    LOG_INFO("Client %d disconnected", client->id);
                    server_remove_client(server, client->id);
                }
                else {
                    // 단순히 데이터가 없는 경우 (논블로킹)
                    LOG_DEBUG("No data available from client %d", client->id);
                }
            }
        }
    }
}

// =============================================================================
// FD 셋 관리 및 유지보수 함수들
// =============================================================================

void server_update_fd_sets(chat_server_t* server) {
    if (!server) {
        return;
    }

    // 마스터 FD 셋 재구성
    FD_ZERO(&server->master_read_fds);
    server->max_fd = 0;

    // 리스닝 소켓 추가
    if (server->listen_socket && server->listen_socket->handle != INVALID_SOCKET) {
        FD_SET(server->listen_socket->handle, &server->master_read_fds);
        if (server->listen_socket->handle > server->max_fd) {
            server->max_fd = server->listen_socket->handle;
        }
    }

    // 모든 활성 클라이언트 소켓 추가
    for (int i = 0; i < MAX_SERVER_CLIENTS; i++) {
        client_info_t* client = &server->clients[i];
        if (client->is_active && client->socket &&
            client->socket->handle != INVALID_SOCKET) {

            FD_SET(client->socket->handle, &server->master_read_fds);
            if (client->socket->handle > server->max_fd) {
                server->max_fd = client->socket->handle;
            }
        }
    }
}

int server_cleanup_inactive_clients(chat_server_t* server) {
    if (!server) {
        return 0;
    }

    time_t current_time = time(NULL);
    int cleaned_count = 0;

    for (int i = 0; i < MAX_SERVER_CLIENTS; i++) {
        client_info_t* client = &server->clients[i];

        if (!client->is_active) {
            continue;
        }

        // 타임아웃 체크
        if (current_time - client->last_activity > server->config.client_timeout_sec) {
            LOG_WARNING("Client %d timed out (last activity: %d seconds ago)",
                client->id, (int)(current_time - client->last_activity));
            server_remove_client(server, client->id);
            cleaned_count++;
            continue;
        }

        // 연결 상태 체크
        if (!network_socket_is_connected(client->socket)) {
            LOG_DEBUG("Client %d connection lost", client->id);
            server_remove_client(server, client->id);
            cleaned_count++;
        }
    }

    if (cleaned_count > 0) {
        LOG_INFO("Cleaned up %d inactive clients", cleaned_count);
        // 통계 업데이트
        server->stats.current_connections = server_get_active_client_count(server);
    }

    return cleaned_count;
}

int server_check_heartbeats(chat_server_t* server) {
    if (!server || !server->config.enable_heartbeat) {
        return 0;
    }

    time_t current_time = time(NULL);
    int heartbeat_count = 0;

    message_t* heartbeat_msg = message_create(MSG_HEARTBEAT, NULL, 0);
    if (!heartbeat_msg) {
        LOG_ERROR("Failed to create heartbeat message");
        return 0;
    }

    for (int i = 0; i < MAX_SERVER_CLIENTS; i++) {
        client_info_t* client = &server->clients[i];

        if (!client->is_active || !client->is_authenticated) {
            continue;
        }

        // 하트비트가 필요한지 확인
        if (current_time - client->last_heartbeat >= server->config.heartbeat_interval_sec) {
            if (server_send_to_client(server, client->id, heartbeat_msg) == 0) {
                client->last_heartbeat = current_time;
                heartbeat_count++;
                LOG_DEBUG("Sent heartbeat to client %d", client->id);
            }
        }
    }

    message_destroy(heartbeat_msg);

    if (heartbeat_count > 0) {
        LOG_DEBUG("Sent heartbeat to %d clients", heartbeat_count);
    }

    return heartbeat_count;
}

// =============================================================================
// 클라이언트 관리 함수들
// =============================================================================

uint32_t server_add_client(chat_server_t* server, network_socket_t* client_socket) {
    if (!server || !client_socket) {
        LOG_ERROR("Invalid parameters for server_add_client");
        return 0;
    }

    // 빈 슬롯 찾기
    int slot_index = -1;
    for (int i = 0; i < MAX_SERVER_CLIENTS; i++) {
        if (!server->clients[i].is_active) {
            slot_index = i;
            break;
        }
    }

    if (slot_index == -1) {
        LOG_ERROR("No available client slots");
        return 0;
    }

    // 클라이언트 정보 초기화
    client_info_t* client = &server->clients[slot_index];
    memset(client, 0, sizeof(client_info_t));

    client->id = server->next_client_id++;
    client->socket = client_socket;
    client->is_active = 1;
    client->is_authenticated = 0;  // 아직 인증되지 않음

    time_t current_time = time(NULL);
    client->connected_at = current_time;
    client->last_activity = current_time;
    client->last_heartbeat = current_time;

    client->messages_sent = 0;
    client->messages_received = 0;

    // 사용자명은 인증 시 설정됨
    utils_string_copy(client->username, sizeof(client->username), "");

    server->client_count++;

    // FD 셋에 추가 (다음 루프에서 server_update_fd_sets 호출됨)

    LOG_DEBUG("Added client %d to slot %d", client->id, slot_index);
    return client->id;
}

int server_remove_client(chat_server_t* server, uint32_t client_id) {
    if (!server || client_id == 0) {
        return -1;
    }

    client_info_t* client = server_find_client_by_id(server, client_id);
    if (!client) {
        LOG_DEBUG("Client %d not found for removal", client_id);
        return -1;
    }

    if (!client->is_active) {
        LOG_DEBUG("Client %d is already inactive", client_id);
        return -1;
    }

    LOG_INFO("Removing client %d (%s)", client_id,
        utils_string_is_empty(client->username) ? "[Anonymous]" : client->username);

    // 다른 클라이언트들에게 사용자 퇴장 알림 (인증된 사용자인 경우)
    if (client->is_authenticated && !utils_string_is_empty(client->username)) {
        // 사용자 퇴장 메시지 생성
        char leave_message[256];
        sprintf_s(leave_message, sizeof(leave_message), "%s has left the chat", client->username);

        message_t* leave_msg = message_create_chat(0, "System", leave_message);
        if (leave_msg) {
            server_broadcast_to_authenticated(server, leave_msg, client_id);
            message_destroy(leave_msg);
        }

        // 사용자 퇴장 시스템 메시지
        message_t* user_left_msg = message_create(MSG_USER_LEFT, client->username,
            strlen(client->username));
        if (user_left_msg) {
            server_broadcast_to_authenticated(server, user_left_msg, client_id);
            message_destroy(user_left_msg);
        }
    }

    // 소켓 연결 종료 및 해제
    if (client->socket) {
        network_socket_close(client->socket);
        network_socket_destroy(client->socket);
        client->socket = NULL;
    }

    // 클라이언트 정보 초기화
    memset(client, 0, sizeof(client_info_t));
    client->is_active = 0;

    server->client_count--;

    // 통계 업데이트
    server->stats.current_connections = server_get_active_client_count(server);

    LOG_DEBUG("Client %d removed successfully", client_id);
    return 0;
}

client_info_t* server_find_client_by_id(chat_server_t* server, uint32_t client_id) {
    if (!server || client_id == 0) {
        return NULL;
    }

    for (int i = 0; i < MAX_SERVER_CLIENTS; i++) {
        if (server->clients[i].is_active && server->clients[i].id == client_id) {
            return &server->clients[i];
        }
    }

    return NULL;
}

client_info_t* server_find_client_by_socket(chat_server_t* server, network_socket_t* socket) {
    if (!server || !socket) {
        return NULL;
    }

    for (int i = 0; i < MAX_SERVER_CLIENTS; i++) {
        if (server->clients[i].is_active && server->clients[i].socket == socket) {
            return &server->clients[i];
        }
    }

    return NULL;
}

client_info_t* server_find_client_by_username(chat_server_t* server, const char* username) {
    if (!server || utils_string_is_empty(username)) {
        return NULL;
    }

    for (int i = 0; i < MAX_SERVER_CLIENTS; i++) {
        client_info_t* client = &server->clients[i];
        if (client->is_active && client->is_authenticated &&
            strcmp(client->username, username) == 0) {
            return client;
        }
    }

    return NULL;
}

// =============================================================================
// 메시지 처리 함수들
// =============================================================================

int server_send_to_client(chat_server_t* server, uint32_t client_id, const message_t* message) {
    if (!server || client_id == 0 || !message) {
        return -1;
    }

    client_info_t* client = server_find_client_by_id(server, client_id);
    if (!client || !client->socket) {
        LOG_DEBUG("Client %d not found or has no socket", client_id);
        return -1;
    }

    network_result_t result = network_socket_send_message(client->socket, message);

    if (result == NETWORK_SUCCESS) {
        client->messages_sent++;
        client->last_activity = time(NULL);
        server->stats.total_bytes_sent += client->socket->bytes_sent;
        return 0;
    }
    else {
        LOG_DEBUG("Failed to send message to client %d: %s",
            client_id, network_result_to_string(result));

        if (result == NETWORK_DISCONNECTED) {
            // 연결이 끊어진 경우 클라이언트 제거
            server_remove_client(server, client_id);
        }
        return -1;
    }
}

int server_broadcast_message(chat_server_t* server, const message_t* message, uint32_t exclude_client_id) {
    if (!server || !message) {
        return 0;
    }

    int sent_count = 0;

    for (int i = 0; i < MAX_SERVER_CLIENTS; i++) {
        client_info_t* client = &server->clients[i];

        if (!client->is_active || client->id == exclude_client_id) {
            continue;
        }

        if (server_send_to_client(server, client->id, message) == 0) {
            sent_count++;
        }
    }

    LOG_DEBUG("Broadcast message to %d clients", sent_count);
    return sent_count;
}

int server_broadcast_to_authenticated(chat_server_t* server, const message_t* message, uint32_t exclude_client_id) {
    if (!server || !message) {
        return 0;
    }

    int sent_count = 0;

    for (int i = 0; i < MAX_SERVER_CLIENTS; i++) {
        client_info_t* client = &server->clients[i];

        if (!client->is_active || !client->is_authenticated || client->id == exclude_client_id) {
            continue;
        }

        if (server_send_to_client(server, client->id, message) == 0) {
            sent_count++;
        }
    }

    LOG_DEBUG("Broadcast message to %d authenticated clients", sent_count);
    return sent_count;
}

// =============================================================================
// 기본 메시지 처리 로직
// =============================================================================

static void server_process_client_message(chat_server_t* server, client_info_t* client, message_t* message) {
    if (!server || !client || !message) {
        return;
    }

    message_type_t msg_type = (message_type_t)ntohs(message->header.type);

    LOG_DEBUG("Processing message type %s from client %d",
        message_type_to_string(msg_type), client->id);

    switch (msg_type) {
    case MSG_CONNECT_REQUEST:
        server_handle_connect_request(server, client, message);
        break;

    case MSG_CHAT_SEND:
        server_handle_chat_message(server, client, message);
        break;

    case MSG_USER_LIST_REQUEST:
        server_handle_user_list_request(server, client, message);
        break;

    case MSG_HEARTBEAT_ACK:
        server_handle_heartbeat_ack(server, client, message);
        break;

    case MSG_DISCONNECT:
        server_handle_disconnect_request(server, client, message);
        break;

    default:
        LOG_WARNING("Unknown message type %d from client %d", msg_type, client->id);
        // 프로토콜 오류 응답
        message_t* error_msg = message_create_error(RESPONSE_ERROR,
            "Unknown message type");
        if (error_msg) {
            server_send_to_client(server, client->id, error_msg);
            message_destroy(error_msg);
        }
        server->stats.protocol_errors++;
        break;
    }
}

static void server_handle_connect_request(chat_server_t* server, client_info_t* client, message_t* message) {
    if (!server || !client || !message) {
        return;
    }

    // 이미 인증된 클라이언트인지 확인
    if (client->is_authenticated) {
        LOG_WARNING("Client %d already authenticated", client->id);
        message_t* error_msg = message_create_error(RESPONSE_ERROR,
            "Already authenticated");
        if (error_msg) {
            server_send_to_client(server, client->id, error_msg);
            message_destroy(error_msg);
        }
        return;
    }

    uint32_t payload_size = ntohl(message->header.payload_size);
    if (payload_size < sizeof(connect_request_payload_t)) {
        LOG_ERROR("Invalid connect request payload size from client %d", client->id);
        server->stats.protocol_errors++;

        message_t* error_msg = message_create_error(RESPONSE_INVALID_INPUT,
            "Invalid request format");
        if (error_msg) {
            server_send_to_client(server, client->id, error_msg);
            message_destroy(error_msg);
        }
        return;
    }

    connect_request_payload_t* request = (connect_request_payload_t*)message->payload;

    // 사용자명 유효성 검증
    char username[MAX_USERNAME_LENGTH];
    utils_string_copy(username, sizeof(username), request->username);
    utils_string_trim(username);

    if (utils_string_is_empty(username)) {
        LOG_WARNING("Empty username from client %d", client->id);
        message_t* error_msg = message_create_error(RESPONSE_INVALID_INPUT,
            "Username cannot be empty");
        if (error_msg) {
            server_send_to_client(server, client->id, error_msg);
            message_destroy(error_msg);
        }
        return;
    }

    // 사용자명 중복 확인
    if (server_find_client_by_username(server, username)) {
        LOG_WARNING("Username '%s' already taken (client %d)", username, client->id);
        message_t* error_msg = message_create_error(RESPONSE_USERNAME_TAKEN,
            "Username is already taken");
        if (error_msg) {
            server_send_to_client(server, client->id, error_msg);
            message_destroy(error_msg);
        }
        server->stats.authentication_failures++;
        return;
    }

    // 인증 성공
    utils_string_copy(client->username, sizeof(client->username), username);
    client->is_authenticated = 1;
    client->last_activity = time(NULL);

    LOG_INFO("Client %d authenticated as '%s'", client->id, username);

    // 성공 응답 전송
    message_t* response = message_create_connect_response(RESPONSE_SUCCESS,
        "Welcome to the chat server!",
        client->id);
    if (response) {
        server_send_to_client(server, client->id, response);
        message_destroy(response);
    }

    // 다른 사용자들에게 입장 알림
    char join_message[256];
    sprintf_s(join_message, sizeof(join_message), "%s has joined the chat", username);

    message_t* join_msg = message_create_chat(0, "System", join_message);
    if (join_msg) {
        server_broadcast_to_authenticated(server, join_msg, client->id);
        message_destroy(join_msg);
    }

    // 사용자 입장 시스템 메시지
    message_t* user_joined_msg = message_create(MSG_USER_JOINED, username, strlen(username));
    if (user_joined_msg) {
        server_broadcast_to_authenticated(server, user_joined_msg, client->id);
        message_destroy(user_joined_msg);
    }
}

static void server_handle_chat_message(chat_server_t* server, client_info_t* client, message_t* message) {
    if (!server || !client || !message) {
        return;
    }

    // 인증 확인
    if (!client->is_authenticated) {
        LOG_WARNING("Unauthenticated client %d tried to send chat message", client->id);
        message_t* error_msg = message_create_error(RESPONSE_AUTH_FAILED,
            "Authentication required");
        if (error_msg) {
            server_send_to_client(server, client->id, error_msg);
            message_destroy(error_msg);
        }
        return;
    }

    uint32_t payload_size = ntohl(message->header.payload_size);
    if (payload_size == 0) {
        LOG_WARNING("Empty chat message from client %d", client->id);
        return;
    }

    // 메시지 내용 추출 (간단히 전체 페이로드를 텍스트로 처리)
    char* chat_text = (char*)malloc(payload_size + 1);
    if (!chat_text) {
        LOG_ERROR("Failed to allocate memory for chat message");
        return;
    }

    memcpy(chat_text, message->payload, payload_size);
    chat_text[payload_size] = '\0';

    // 메시지 유효성 검증
    utils_string_trim(chat_text);
    if (utils_string_is_empty(chat_text)) {
        LOG_DEBUG("Empty chat message from client %d after trimming", client->id);
        free(chat_text);
        return;
    }

    LOG_INFO("Chat from %s: %s", client->username, chat_text);

    // 브로드캐스트용 채팅 메시지 생성
    message_t* broadcast_msg = message_create_chat(client->id, client->username, chat_text);
    if (broadcast_msg) {
        int sent_count = server_broadcast_to_authenticated(server, broadcast_msg, 0);  // 본인 포함 전송
        LOG_DEBUG("Chat message broadcast to %d clients", sent_count);
        message_destroy(broadcast_msg);
    }

    free(chat_text);
}

static void server_handle_user_list_request(chat_server_t* server, client_info_t* client, message_t* message) {
    if (!server || !client || !message) {
        return;
    }

    // 인증 확인
    if (!client->is_authenticated) {
        message_t* error_msg = message_create_error(RESPONSE_AUTH_FAILED,
            "Authentication required");
        if (error_msg) {
            server_send_to_client(server, client->id, error_msg);
            message_destroy(error_msg);
        }
        return;
    }

    // 인증된 사용자 목록 생성
    char user_list[MAX_MESSAGE_SIZE - sizeof(message_header_t)];
    int offset = 0;
    int user_count = 0;

    for (int i = 0; i < MAX_SERVER_CLIENTS && offset < sizeof(user_list) - MAX_USERNAME_LENGTH - 2; i++) {
        client_info_t* other_client = &server->clients[i];
        if (other_client->is_active && other_client->is_authenticated) {
            if (user_count > 0) {
                offset += sprintf_s(user_list + offset, sizeof(user_list) - offset, ",");
            }
            offset += sprintf_s(user_list + offset, sizeof(user_list) - offset, "%s", other_client->username);
            user_count++;
        }
    }

    if (user_count == 0) {
        utils_string_copy(user_list, sizeof(user_list), "No other users online");
    }

    LOG_DEBUG("Sending user list to client %d: %d users", client->id, user_count);

    message_t* response = message_create(MSG_USER_LIST_RESPONSE, user_list, strlen(user_list));
    if (response) {
        server_send_to_client(server, client->id, response);
        message_destroy(response);
    }
}

static void server_handle_heartbeat_ack(chat_server_t* server, client_info_t* client, message_t* message) {
    if (!server || !client || !message) {
        return;
    }

    client->last_heartbeat = time(NULL);
    client->last_activity = time(NULL);
    LOG_DEBUG("Received heartbeat ACK from client %d", client->id);
}

static void server_handle_disconnect_request(chat_server_t* server, client_info_t* client, message_t* message) {
    if (!server || !client || !message) {
        return;
    }

    LOG_INFO("Client %d requested disconnect", client->id);

    // 연결 종료 응답
    message_t* response = message_create(MSG_DISCONNECT, NULL, 0);
    if (response) {
        server_send_to_client(server, client->id, response);
        message_destroy(response);
    }

    // 클라이언트 제거 (사용자 퇴장 알림 포함)
    server_remove_client(server, client->id);
}