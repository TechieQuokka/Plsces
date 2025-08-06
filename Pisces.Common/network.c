#include "pch.h"
#include "network.h"
#include "message.h"
#include "utils.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

// =============================================================================
// 전역 변수
// =============================================================================

static int g_network_initialized = 0;

// =============================================================================
// 네트워크 초기화/정리
// =============================================================================

network_result_t network_initialize(void) {
    if (g_network_initialized) {
        return NETWORK_SUCCESS;
    }

    WSADATA wsa_data;
    int result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    if (result != 0) {
        LOG_ERROR("WSAStartup failed with error: %d", result);
        return NETWORK_INIT_FAILED;
    }

    // 요청한 버전 확인
    if (LOBYTE(wsa_data.wVersion) != 2 || HIBYTE(wsa_data.wVersion) != 2) {
        LOG_ERROR("Could not find usable version of Winsock.dll");
        WSACleanup();
        return NETWORK_INIT_FAILED;
    }

    g_network_initialized = 1;
    LOG_INFO("Network initialized successfully (Winsock 2.2)");
    return NETWORK_SUCCESS;
}

void network_cleanup(void) {
    if (g_network_initialized) {
        WSACleanup();
        g_network_initialized = 0;
        LOG_INFO("Network cleaned up");
    }
}

int network_is_initialized(void) {
    return g_network_initialized;
}

// =============================================================================
// 소켓 생성/설정
// =============================================================================

network_socket_t* network_socket_create(socket_type_t type) {
    if (!g_network_initialized) {
        LOG_ERROR("Network not initialized");
        return NULL;
    }

    network_socket_t* sock = (network_socket_t*)calloc(1, sizeof(network_socket_t));
    if (!sock) {
        LOG_ERROR("Failed to allocate memory for socket");
        return NULL;
    }

    // 소켓 생성
    int socket_family = AF_INET;
    int socket_type = (type == SOCKET_TYPE_UDP) ? SOCK_DGRAM : SOCK_STREAM;
    int protocol = (type == SOCKET_TYPE_UDP) ? IPPROTO_UDP : IPPROTO_TCP;

    sock->handle = socket(socket_family, socket_type, protocol);
    if (sock->handle == INVALID_SOCKET) {
        LOG_ERROR("Failed to create socket: %s",
            utils_winsock_error_to_string(WSAGetLastError()));
        free(sock);
        return NULL;
    }

    // 구조체 초기화
    sock->type = type;
    sock->state = SOCKET_STATE_CLOSED;
    sock->recv_buffer_pos = 0;
    sock->send_buffer_pos = 0;
    sock->bytes_sent = 0;
    sock->bytes_received = 0;
    sock->messages_sent = 0;
    sock->messages_received = 0;
    sock->created_time = time(NULL);
    sock->last_activity = sock->created_time;

    LOG_DEBUG("Created socket (handle: %d, type: %d)", (int)sock->handle, type);
    return sock;
}

void network_socket_destroy(network_socket_t* sock) {
    if (!sock) return;

    if (sock->handle != INVALID_SOCKET) {
        closesocket(sock->handle);
        sock->handle = INVALID_SOCKET;
    }

    LOG_DEBUG("Destroyed socket");
    free(sock);
}

network_result_t network_socket_set_nonblocking(network_socket_t* sock) {
    if (!sock || sock->handle == INVALID_SOCKET) {
        return NETWORK_INVALID_SOCKET;
    }

    u_long mode = 1;  // 1 = non-blocking mode
    if (ioctlsocket(sock->handle, FIONBIO, &mode) == SOCKET_ERROR) {
        LOG_ERROR("Failed to set non-blocking mode: %s",
            utils_winsock_error_to_string(WSAGetLastError()));
        return NETWORK_ERROR;
    }

    LOG_DEBUG("Set socket to non-blocking mode");
    return NETWORK_SUCCESS;
}

network_result_t network_socket_set_reuse_addr(network_socket_t* sock, int enable) {
    if (!sock || sock->handle == INVALID_SOCKET) {
        return NETWORK_INVALID_SOCKET;
    }

    int optval = enable ? 1 : 0;
    if (setsockopt(sock->handle, SOL_SOCKET, SO_REUSEADDR,
        (char*)&optval, sizeof(optval)) == SOCKET_ERROR) {
        LOG_ERROR("Failed to set SO_REUSEADDR: %s",
            utils_winsock_error_to_string(WSAGetLastError()));
        return NETWORK_ERROR;
    }

    LOG_DEBUG("Set SO_REUSEADDR to %s", enable ? "enabled" : "disabled");
    return NETWORK_SUCCESS;
}

network_result_t network_socket_set_timeout(network_socket_t* sock, int timeout_ms) {
    if (!sock || sock->handle == INVALID_SOCKET) {
        return NETWORK_INVALID_SOCKET;
    }

    DWORD timeout = (DWORD)timeout_ms;

    // 수신 타임아웃 설정
    if (setsockopt(sock->handle, SOL_SOCKET, SO_RCVTIMEO,
        (char*)&timeout, sizeof(timeout)) == SOCKET_ERROR) {
        LOG_ERROR("Failed to set receive timeout: %s",
            utils_winsock_error_to_string(WSAGetLastError()));
        return NETWORK_ERROR;
    }

    // 송신 타임아웃 설정
    if (setsockopt(sock->handle, SOL_SOCKET, SO_SNDTIMEO,
        (char*)&timeout, sizeof(timeout)) == SOCKET_ERROR) {
        LOG_ERROR("Failed to set send timeout: %s",
            utils_winsock_error_to_string(WSAGetLastError()));
        return NETWORK_ERROR;
    }

    LOG_DEBUG("Set socket timeout to %d ms", timeout_ms);
    return NETWORK_SUCCESS;
}

// =============================================================================
// 서버 소켓 함수들
// =============================================================================

network_result_t network_socket_bind(network_socket_t* sock, uint16_t port, const char* interface_ip) {
    if (!sock || sock->handle == INVALID_SOCKET) {
        return NETWORK_INVALID_SOCKET;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (interface_ip && strlen(interface_ip) > 0) {
        if (inet_pton(AF_INET, interface_ip, &addr.sin_addr) != 1) {
            LOG_ERROR("Invalid interface IP address: %s", interface_ip);
            return NETWORK_ERROR;
        }
    }
    else {
        addr.sin_addr.s_addr = INADDR_ANY;
    }

    if (bind(sock->handle, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        int error = WSAGetLastError();
        LOG_ERROR("Failed to bind socket to port %d: %s", port,
            utils_winsock_error_to_string(error));

        if (error == WSAEADDRINUSE) {
            return NETWORK_CONNECTION_REFUSED;  // Port already in use
        }
        return NETWORK_ERROR;
    }

    // 로컬 주소 정보 저장
    sock->local_addr = addr;
    sock->state = SOCKET_STATE_CLOSED;  // 아직 리스닝하지 않음

    LOG_INFO("Socket bound to %s:%d",
        interface_ip ? interface_ip : "0.0.0.0", port);
    return NETWORK_SUCCESS;
}

network_result_t network_socket_listen(network_socket_t* sock, int backlog) {
    if (!sock || sock->handle == INVALID_SOCKET) {
        return NETWORK_INVALID_SOCKET;
    }

    if (backlog <= 0) {
        backlog = MAX_PENDING_CONNECTIONS;
    }

    if (listen(sock->handle, backlog) == SOCKET_ERROR) {
        LOG_ERROR("Failed to listen on socket: %s",
            utils_winsock_error_to_string(WSAGetLastError()));
        return NETWORK_ERROR;
    }

    sock->state = SOCKET_STATE_LISTENING;
    LOG_INFO("Socket listening with backlog %d", backlog);
    return NETWORK_SUCCESS;
}

network_socket_t* network_socket_accept(network_socket_t* server_sock) {
    if (!server_sock || server_sock->handle == INVALID_SOCKET ||
        server_sock->state != SOCKET_STATE_LISTENING) {
        return NULL;
    }

    struct sockaddr_in client_addr;
    int client_addr_len = sizeof(client_addr);

    SOCKET client_handle = accept(server_sock->handle,
        (struct sockaddr*)&client_addr,
        &client_addr_len);

    if (client_handle == INVALID_SOCKET) {
        int error = WSAGetLastError();
        if (error != WSAEWOULDBLOCK) {  // 논블로킹에서 정상적인 상황
            LOG_ERROR("Accept failed: %s", utils_winsock_error_to_string(error));
        }
        return NULL;
    }

    // 새 클라이언트 소켓 생성
    network_socket_t* client_sock = (network_socket_t*)calloc(1, sizeof(network_socket_t));
    if (!client_sock) {
        LOG_ERROR("Failed to allocate memory for client socket");
        closesocket(client_handle);
        return NULL;
    }

    // 클라이언트 소켓 초기화
    client_sock->handle = client_handle;
    client_sock->type = SOCKET_TYPE_TCP_CLIENT;
    client_sock->state = SOCKET_STATE_CONNECTED;
    client_sock->remote_addr = client_addr;
    client_sock->created_time = time(NULL);
    client_sock->last_activity = client_sock->created_time;

    // 원격 주소 정보 저장
    inet_ntop(AF_INET, &client_addr.sin_addr, client_sock->remote_ip, sizeof(client_sock->remote_ip));
    client_sock->remote_port = ntohs(client_addr.sin_port);

    LOG_INFO("Accepted connection from %s:%d", client_sock->remote_ip, client_sock->remote_port);
    return client_sock;
}

// =============================================================================
// 클라이언트 소켓 함수들
// =============================================================================

network_result_t network_socket_connect(network_socket_t* sock, const char* hostname, uint16_t port) {
    if (!sock || sock->handle == INVALID_SOCKET || !hostname) {
        return NETWORK_INVALID_SOCKET;
    }

    // 호스트명을 IP 주소로 변환
    char ip_str[16];
    network_result_t resolve_result = network_resolve_hostname(hostname, ip_str, sizeof(ip_str));
    if (resolve_result != NETWORK_SUCCESS) {
        return resolve_result;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip_str, &server_addr.sin_addr) != 1) {
        LOG_ERROR("Invalid IP address: %s", ip_str);
        return NETWORK_ERROR;
    }

    sock->state = SOCKET_STATE_CONNECTING;
    sock->remote_addr = server_addr;
    utils_string_copy(sock->remote_ip, sizeof(sock->remote_ip), ip_str);
    sock->remote_port = port;

    int result = connect(sock->handle, (struct sockaddr*)&server_addr, sizeof(server_addr));

    if (result == SOCKET_ERROR) {
        int error = WSAGetLastError();
        if (error == WSAEWOULDBLOCK || error == WSAEINPROGRESS) {
            // 논블로킹 모드에서 정상적인 상황
            LOG_DEBUG("Connection in progress to %s:%d", hostname, port);
            return NETWORK_WOULD_BLOCK;
        }
        else {
            LOG_ERROR("Connect failed to %s:%d - %s", hostname, port,
                utils_winsock_error_to_string(error));
            sock->state = SOCKET_STATE_ERROR;

            if (error == WSAECONNREFUSED) {
                return NETWORK_CONNECTION_REFUSED;
            }
            else if (error == WSAEHOSTUNREACH || error == WSAENETUNREACH) {
                return NETWORK_HOST_UNREACHABLE;
            }
            return NETWORK_ERROR;
        }
    }

    // 즉시 연결됨 (논블로킹이 아닌 경우)
    sock->state = SOCKET_STATE_CONNECTED;
    sock->last_activity = time(NULL);
    LOG_INFO("Connected to %s:%d", hostname, port);
    return NETWORK_SUCCESS;
}

network_result_t network_socket_connect_check(network_socket_t* sock) {
    if (!sock || sock->handle == INVALID_SOCKET ||
        sock->state != SOCKET_STATE_CONNECTING) {
        return NETWORK_INVALID_SOCKET;
    }

    // select를 사용하여 연결 완료 확인
    fd_set write_fds, error_fds;
    struct timeval timeout = { 0, 0 };  // 즉시 반환

    FD_ZERO(&write_fds);
    FD_ZERO(&error_fds);
    FD_SET(sock->handle, &write_fds);
    FD_SET(sock->handle, &error_fds);

    int result = select(0, NULL, &write_fds, &error_fds, &timeout);

    if (result == SOCKET_ERROR) {
        LOG_ERROR("Select failed during connection check: %s",
            utils_winsock_error_to_string(WSAGetLastError()));
        sock->state = SOCKET_STATE_ERROR;
        return NETWORK_ERROR;
    }

    if (result == 0) {
        // 아직 연결 진행 중
        return NETWORK_WOULD_BLOCK;
    }

    if (FD_ISSET(sock->handle, &error_fds)) {
        // 연결 오류 발생
        int error;
        int error_len = sizeof(error);
        if (getsockopt(sock->handle, SOL_SOCKET, SO_ERROR, (char*)&error, &error_len) == 0) {
            LOG_ERROR("Connection failed: %s", utils_winsock_error_to_string(error));
        }
        sock->state = SOCKET_STATE_ERROR;
        return NETWORK_ERROR;
    }

    if (FD_ISSET(sock->handle, &write_fds)) {
        // 연결 완료
        sock->state = SOCKET_STATE_CONNECTED;
        sock->last_activity = time(NULL);
        LOG_INFO("Connection completed to %s:%d", sock->remote_ip, sock->remote_port);
        return NETWORK_SUCCESS;
    }

    return NETWORK_WOULD_BLOCK;
}

// =============================================================================
// 데이터 송수신
// =============================================================================

network_result_t network_socket_send(network_socket_t* sock, const void* data,
    int length, int* bytes_sent) {
    if (!sock || sock->handle == INVALID_SOCKET || !data || length <= 0) {
        return NETWORK_INVALID_SOCKET;
    }

    if (bytes_sent) *bytes_sent = 0;

    if (sock->state != SOCKET_STATE_CONNECTED) {
        return NETWORK_DISCONNECTED;
    }

    int result = send(sock->handle, (const char*)data, length, 0);

    if (result == SOCKET_ERROR) {
        int error = WSAGetLastError();
        if (error == WSAEWOULDBLOCK) {
            return NETWORK_WOULD_BLOCK;
        }
        else if (error == WSAECONNRESET || error == WSAECONNABORTED) {
            sock->state = SOCKET_STATE_DISCONNECTING;
            return NETWORK_DISCONNECTED;
        }
        else {
            LOG_ERROR("Send failed: %s", utils_winsock_error_to_string(error));
            return NETWORK_ERROR;
        }
    }

    if (bytes_sent) *bytes_sent = result;
    sock->bytes_sent += result;
    sock->last_activity = time(NULL);

    LOG_DEBUG("Sent %d bytes", result);
    return NETWORK_SUCCESS;
}

network_result_t network_socket_recv(network_socket_t* sock, void* buffer,
    int buffer_size, int* bytes_received) {
    if (!sock || sock->handle == INVALID_SOCKET || !buffer || buffer_size <= 0) {
        return NETWORK_INVALID_SOCKET;
    }

    if (bytes_received) *bytes_received = 0;

    if (sock->state != SOCKET_STATE_CONNECTED) {
        return NETWORK_DISCONNECTED;
    }

    int result = recv(sock->handle, (char*)buffer, buffer_size, 0);

    if (result == SOCKET_ERROR) {
        int error = WSAGetLastError();
        if (error == WSAEWOULDBLOCK) {
            return NETWORK_WOULD_BLOCK;
        }
        else if (error == WSAECONNRESET || error == WSAECONNABORTED) {
            sock->state = SOCKET_STATE_DISCONNECTING;
            return NETWORK_DISCONNECTED;
        }
        else {
            LOG_ERROR("Receive failed: %s", utils_winsock_error_to_string(error));
            return NETWORK_ERROR;
        }
    }

    if (result == 0) {
        // 정상적인 연결 종료
        sock->state = SOCKET_STATE_DISCONNECTING;
        LOG_DEBUG("Connection closed by peer");
        return NETWORK_DISCONNECTED;
    }

    if (bytes_received) *bytes_received = result;
    sock->bytes_received += result;
    sock->last_activity = time(NULL);

    LOG_DEBUG("Received %d bytes", result);
    return NETWORK_SUCCESS;
}

network_result_t network_socket_send_all(network_socket_t* sock, const void* data, int length) {
    if (!sock || !data || length <= 0) {
        return NETWORK_INVALID_SOCKET;
    }

    const char* ptr = (const char*)data;
    int total_sent = 0;

    while (total_sent < length) {
        int bytes_sent;
        network_result_t result = network_socket_send(sock, ptr + total_sent,
            length - total_sent, &bytes_sent);

        if (result == NETWORK_SUCCESS) {
            total_sent += bytes_sent;
        }
        else if (result == NETWORK_WOULD_BLOCK) {
            // 잠시 대기 후 재시도
            Sleep(1);
            continue;
        }
        else {
            return result;
        }
    }

    return NETWORK_SUCCESS;
}

network_result_t network_socket_recv_all(network_socket_t* sock, void* buffer, int length) {
    if (!sock || !buffer || length <= 0) {
        return NETWORK_INVALID_SOCKET;
    }

    char* ptr = (char*)buffer;
    int total_received = 0;

    while (total_received < length) {
        int bytes_received;
        network_result_t result = network_socket_recv(sock, ptr + total_received,
            length - total_received, &bytes_received);

        if (result == NETWORK_SUCCESS) {
            total_received += bytes_received;
        }
        else if (result == NETWORK_WOULD_BLOCK) {
            // 잠시 대기 후 재시도
            Sleep(1);
            continue;
        }
        else {
            return result;
        }
    }

    return NETWORK_SUCCESS;
}

// =============================================================================
// 메시지 레벨 송수신
// =============================================================================

network_result_t network_socket_send_message(network_socket_t* sock, const message_t* msg) {
    if (!sock || !msg) {
        return NETWORK_INVALID_SOCKET;
    }

    // 메시지를 버퍼에 직렬화
    char send_buffer[MAX_MESSAGE_SIZE];
    int serialized_size = message_serialize(msg, send_buffer, sizeof(send_buffer));

    if (serialized_size < 0) {
        LOG_ERROR("Failed to serialize message");
        return NETWORK_ERROR;
    }

    // 전체 메시지 전송
    network_result_t result = network_socket_send_all(sock, send_buffer, serialized_size);

    if (result == NETWORK_SUCCESS) {
        sock->messages_sent++;
        LOG_DEBUG("Sent message type %s (%d bytes)",
            message_type_to_string((message_type_t)ntohs(msg->header.type)),
            serialized_size);
    }

    return result;
}

message_t* network_socket_recv_message(network_socket_t* sock) {
    if (!sock) {
        return NULL;
    }

    // 먼저 헤더 수신
    message_header_t header;
    network_result_t result = network_socket_recv_all(sock, &header, sizeof(header));

    if (result != NETWORK_SUCCESS) {
        if (result != NETWORK_WOULD_BLOCK) {
            LOG_DEBUG("Failed to receive message header: %s",
                network_result_to_string(result));
        }
        return NULL;
    }

    // 헤더 유효성 검증
    if (!message_validate_header(&header)) {
        LOG_ERROR("Invalid message header received");
        return NULL;
    }

    uint32_t payload_size = ntohl(header.payload_size);

    // 전체 메시지 버퍼 할당
    size_t total_size = sizeof(message_header_t) + payload_size;
    char* message_buffer = (char*)malloc(total_size);
    if (!message_buffer) {
        LOG_ERROR("Failed to allocate memory for message (%zu bytes)", total_size);
        return NULL;
    }

    // 헤더 복사
    memcpy(message_buffer, &header, sizeof(header));

    // 페이로드 수신 (있는 경우)
    if (payload_size > 0) {
        result = network_socket_recv_all(sock, message_buffer + sizeof(header), payload_size);

        if (result != NETWORK_SUCCESS) {
            LOG_ERROR("Failed to receive message payload: %s",
                network_result_to_string(result));
            free(message_buffer);
            return NULL;
        }
    }

    // 메시지 역직렬화
    message_t* msg = message_deserialize(message_buffer, total_size);
    free(message_buffer);

    if (!msg) {
        LOG_ERROR("Failed to deserialize received message");
        return NULL;
    }

    sock->messages_received++;
    LOG_DEBUG("Received message type %s (%u bytes payload)",
        message_type_to_string((message_type_t)ntohs(msg->header.type)),
        payload_size);

    return msg;
}

// =============================================================================
// 연결 관리
// =============================================================================

network_result_t network_socket_close(network_socket_t* sock) {
    if (!sock || sock->handle == INVALID_SOCKET) {
        return NETWORK_INVALID_SOCKET;
    }

    if (sock->state == SOCKET_STATE_CONNECTED) {
        // Graceful shutdown
        shutdown(sock->handle, SD_BOTH);
        sock->state = SOCKET_STATE_DISCONNECTING;
    }

    if (closesocket(sock->handle) == SOCKET_ERROR) {
        LOG_ERROR("Failed to close socket: %s",
            utils_winsock_error_to_string(WSAGetLastError()));
        return NETWORK_ERROR;
    }

    sock->handle = INVALID_SOCKET;
    sock->state = SOCKET_STATE_CLOSED;

    LOG_DEBUG("Socket closed");
    return NETWORK_SUCCESS;
}

int network_socket_is_connected(const network_socket_t* sock) {
    return (sock && sock->handle != INVALID_SOCKET &&
        sock->state == SOCKET_STATE_CONNECTED) ? 1 : 0;
}

int network_socket_has_data(network_socket_t* sock, int timeout_ms) {
    if (!sock || sock->handle == INVALID_SOCKET) {
        return -1;
    }

    fd_set read_fds;
    struct timeval timeout;

    FD_ZERO(&read_fds);
    FD_SET(sock->handle, &read_fds);

    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;

    int result = select(0, &read_fds, NULL, NULL, &timeout);

    if (result == SOCKET_ERROR) {
        LOG_ERROR("Select failed in has_data: %s",
            utils_winsock_error_to_string(WSAGetLastError()));
        return -1;
    }

    return (result > 0 && FD_ISSET(sock->handle, &read_fds)) ? 1 : 0;
}

int network_socket_can_write(network_socket_t* sock, int timeout_ms) {
    if (!sock || sock->handle == INVALID_SOCKET) {
        return -1;
    }

    fd_set write_fds;
    struct timeval timeout;

    FD_ZERO(&write_fds);
    FD_SET(sock->handle, &write_fds);

    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;

    int result = select(0, NULL, &write_fds, NULL, &timeout);

    if (result == SOCKET_ERROR) {
        LOG_ERROR("Select failed in can_write: %s",
            utils_winsock_error_to_string(WSAGetLastError()));
        return -1;
    }

    return (result > 0 && FD_ISSET(sock->handle, &write_fds)) ? 1 : 0;
}

// =============================================================================
// 유틸리티 함수들
// =============================================================================

const char* network_addr_to_string(const struct sockaddr_in* addr, char* buffer, size_t buffer_size) {
    if (!addr || !buffer || buffer_size < 16) {
        return NULL;
    }

    if (inet_ntop(AF_INET, &addr->sin_addr, buffer, (socklen_t)buffer_size) == NULL) {
        LOG_ERROR("Failed to convert address to string");
        return NULL;
    }

    return buffer;
}

network_result_t network_resolve_hostname(const char* hostname, char* ip_buffer, size_t buffer_size) {
    if (!hostname || !ip_buffer || buffer_size < 16) {
        return NETWORK_ERROR;
    }

    // IP 주소인지 먼저 확인
    struct sockaddr_in addr;
    if (inet_pton(AF_INET, hostname, &addr.sin_addr) == 1) {
        utils_string_copy(ip_buffer, buffer_size, hostname);
        return NETWORK_SUCCESS;
    }

    // 호스트명 해결
    struct addrinfo hints, * result;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    int ret = getaddrinfo(hostname, NULL, &hints, &result);
    if (ret != 0) {
        LOG_ERROR("Failed to resolve hostname %s: %s", hostname, gai_strerror(ret));
        return NETWORK_HOST_UNREACHABLE;
    }

    struct sockaddr_in* addr_in = (struct sockaddr_in*)result->ai_addr;
    if (inet_ntop(AF_INET, &addr_in->sin_addr, ip_buffer, (socklen_t)buffer_size) == NULL) {
        LOG_ERROR("Failed to convert resolved address to string");
        freeaddrinfo(result);
        return NETWORK_ERROR;
    }

    freeaddrinfo(result);
    LOG_DEBUG("Resolved %s to %s", hostname, ip_buffer);
    return NETWORK_SUCCESS;
}

const char* network_result_to_string(network_result_t result) {
    switch (result) {
    case NETWORK_SUCCESS:              return "Success";
    case NETWORK_ERROR:                return "General error";
    case NETWORK_WOULD_BLOCK:          return "Would block";
    case NETWORK_DISCONNECTED:         return "Disconnected";
    case NETWORK_TIMEOUT:              return "Timeout";
    case NETWORK_BUFFER_FULL:          return "Buffer full";
    case NETWORK_INVALID_SOCKET:       return "Invalid socket";
    case NETWORK_CONNECTION_REFUSED:   return "Connection refused";
    case NETWORK_HOST_UNREACHABLE:     return "Host unreachable";
    case NETWORK_INIT_FAILED:          return "Initialization failed";
    default:                           return "Unknown error";
    }
}

const char* network_socket_state_to_string(socket_state_t state) {
    switch (state) {
    case SOCKET_STATE_CLOSED:         return "Closed";
    case SOCKET_STATE_LISTENING:      return "Listening";
    case SOCKET_STATE_CONNECTING:     return "Connecting";
    case SOCKET_STATE_CONNECTED:      return "Connected";
    case SOCKET_STATE_DISCONNECTING:  return "Disconnecting";
    case SOCKET_STATE_ERROR:          return "Error";
    default:                          return "Unknown";
    }
}

void network_socket_print_stats(const network_socket_t* sock) {
    if (!sock) {
        printf("Socket: NULL\n");
        return;
    }

    char size_buffer[32];
    time_t uptime = time(NULL) - sock->created_time;

    printf("=== Socket Statistics ===\n");
    printf("Handle: %d\n", (int)sock->handle);
    printf("State: %s\n", network_socket_state_to_string(sock->state));
    printf("Remote: %s:%d\n", sock->remote_ip, sock->remote_port);
    printf("Bytes sent: %s\n", utils_bytes_to_human_readable(sock->bytes_sent, size_buffer, sizeof(size_buffer)));
    printf("Bytes received: %s\n", utils_bytes_to_human_readable(sock->bytes_received, size_buffer, sizeof(size_buffer)));
    printf("Messages sent: %u\n", sock->messages_sent);
    printf("Messages received: %u\n", sock->messages_received);
    printf("Uptime: %d seconds\n", (int)uptime);
    printf("Last activity: %d seconds ago\n", (int)(time(NULL) - sock->last_activity));
}

const char* network_get_local_ip(char* buffer, size_t buffer_size) {
    if (!buffer || buffer_size < 16) {
        return NULL;
    }

    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == SOCKET_ERROR) {
        LOG_ERROR("Failed to get hostname: %s",
            utils_winsock_error_to_string(WSAGetLastError()));
        return NULL;
    }

    return network_resolve_hostname(hostname, buffer, buffer_size) == NETWORK_SUCCESS ? buffer : NULL;
}