#ifndef NETWORK_H
#define NETWORK_H

#include "protocol.h"
#include "message.h"
#include <stdint.h>

// =============================================================================
// 네트워크 상수 정의
// =============================================================================

#define NETWORK_BUFFER_SIZE         8192    // 기본 네트워크 버퍼 크기
#define MAX_PENDING_CONNECTIONS     10      // 대기 중인 연결 수
#define SOCKET_TIMEOUT_MS          5000     // 소켓 타임아웃 (밀리초)
#define MAX_HOSTNAME_LENGTH         256     // 최대 호스트명 길이

// 소켓 작업 결과 코드
typedef enum {
    NETWORK_SUCCESS = 0,                    // 성공
    NETWORK_ERROR = -1,                     // 일반 오류
    NETWORK_WOULD_BLOCK = -2,               // 블록킹 상황 (재시도 필요)
    NETWORK_DISCONNECTED = -3,              // 연결 끊김
    NETWORK_TIMEOUT = -4,                   // 타임아웃
    NETWORK_BUFFER_FULL = -5,               // 버퍼 가득참
    NETWORK_INVALID_SOCKET = -6,            // 잘못된 소켓
    NETWORK_CONNECTION_REFUSED = -7,        // 연결 거부
    NETWORK_HOST_UNREACHABLE = -8,          // 호스트 접근 불가
    NETWORK_INIT_FAILED = -9                // 네트워크 초기화 실패
} network_result_t;

// 소켓 타입
typedef enum {
    SOCKET_TYPE_TCP_SERVER,
    SOCKET_TYPE_TCP_CLIENT,
    SOCKET_TYPE_UDP
} socket_type_t;

// 소켓 상태
typedef enum {
    SOCKET_STATE_CLOSED,
    SOCKET_STATE_LISTENING,
    SOCKET_STATE_CONNECTING,
    SOCKET_STATE_CONNECTED,
    SOCKET_STATE_DISCONNECTING,
    SOCKET_STATE_ERROR
} socket_state_t;

// =============================================================================
// 소켓 정보 구조체
// =============================================================================

typedef struct {
    SOCKET handle;                          // 윈속 소켓 핸들
    socket_type_t type;                     // 소켓 타입
    socket_state_t state;                   // 현재 상태

    // 주소 정보
    struct sockaddr_in local_addr;          // 로컬 주소
    struct sockaddr_in remote_addr;         // 원격 주소
    char remote_ip[16];                     // 원격 IP (점 표기법)
    uint16_t remote_port;                   // 원격 포트

    // 버퍼 정보
    char recv_buffer[NETWORK_BUFFER_SIZE];  // 수신 버퍼
    char send_buffer[NETWORK_BUFFER_SIZE];  // 송신 버퍼
    int recv_buffer_pos;                    // 수신 버퍼 현재 위치
    int send_buffer_pos;                    // 송신 버퍼 현재 위치

    // 통계 정보
    uint64_t bytes_sent;                    // 전송한 바이트 수
    uint64_t bytes_received;                // 받은 바이트 수
    uint32_t messages_sent;                 // 전송한 메시지 수
    uint32_t messages_received;             // 받은 메시지 수
    time_t created_time;                    // 생성 시간
    time_t last_activity;                   // 마지막 활동 시간
} network_socket_t;

// =============================================================================
// 네트워크 초기화/정리
// =============================================================================

/**
 * Winsock 라이브러리 초기화
 * @return 성공 시 NETWORK_SUCCESS, 실패 시 NETWORK_INIT_FAILED
 */
network_result_t network_initialize(void);

/**
 * Winsock 라이브러리 정리
 */
void network_cleanup(void);

/**
 * 네트워크가 초기화되었는지 확인
 * @return 초기화되었으면 1, 아니면 0
 */
int network_is_initialized(void);

// =============================================================================
// 소켓 생성/설정
// =============================================================================

/**
 * 새로운 네트워크 소켓 생성
 * @param type 소켓 타입
 * @return 생성된 소켓 포인터, 실패 시 NULL
 */
network_socket_t* network_socket_create(socket_type_t type);

/**
 * 소켓 해제
 * @param sock 해제할 소켓
 */
void network_socket_destroy(network_socket_t* sock);

/**
 * 소켓을 논블로킹 모드로 설정
 * @param sock 대상 소켓
 * @return 성공 시 NETWORK_SUCCESS
 */
network_result_t network_socket_set_nonblocking(network_socket_t* sock);

/**
 * 소켓 재사용 설정 (SO_REUSEADDR)
 * @param sock 대상 소켓
 * @param enable 1이면 활성화, 0이면 비활성화
 * @return 성공 시 NETWORK_SUCCESS
 */
network_result_t network_socket_set_reuse_addr(network_socket_t* sock, int enable);

/**
 * 소켓 타임아웃 설정
 * @param sock 대상 소켓
 * @param timeout_ms 타임아웃 (밀리초)
 * @return 성공 시 NETWORK_SUCCESS
 */
network_result_t network_socket_set_timeout(network_socket_t* sock, int timeout_ms);

// =============================================================================
// 서버 소켓 함수들
// =============================================================================

/**
 * 서버 소켓을 특정 포트에 바인드
 * @param sock 서버 소켓
 * @param port 바인드할 포트 (0이면 임의 포트)
 * @param interface_ip 바인드할 인터페이스 IP (NULL이면 모든 인터페이스)
 * @return 성공 시 NETWORK_SUCCESS
 */
network_result_t network_socket_bind(network_socket_t* sock, uint16_t port, const char* interface_ip);

/**
 * 서버 소켓을 리스닝 모드로 전환
 * @param sock 서버 소켓
 * @param backlog 대기열 크기 (기본: MAX_PENDING_CONNECTIONS)
 * @return 성공 시 NETWORK_SUCCESS
 */
network_result_t network_socket_listen(network_socket_t* sock, int backlog);

/**
 * 클라이언트 연결 수락 (논블로킹)
 * @param server_sock 서버 소켓
 * @return 새 클라이언트 소켓, 연결이 없으면 NULL
 */
network_socket_t* network_socket_accept(network_socket_t* server_sock);

// =============================================================================
// 클라이언트 소켓 함수들
// =============================================================================

/**
 * 서버에 연결 (논블로킹)
 * @param sock 클라이언트 소켓
 * @param hostname 서버 호스트명 또는 IP
 * @param port 서버 포트
 * @return NETWORK_SUCCESS, NETWORK_WOULD_BLOCK, 또는 오류 코드
 */
network_result_t network_socket_connect(network_socket_t* sock, const char* hostname, uint16_t port);

/**
 * 연결이 완료되었는지 확인 (논블로킹 연결용)
 * @param sock 클라이언트 소켓
 * @return 연결 완료 시 NETWORK_SUCCESS, 진행 중이면 NETWORK_WOULD_BLOCK
 */
network_result_t network_socket_connect_check(network_socket_t* sock);

// =============================================================================
// 데이터 송수신
// =============================================================================

/**
 * 데이터 전송 (논블로킹)
 * @param sock 소켓
 * @param data 전송할 데이터
 * @param length 데이터 길이
 * @param bytes_sent 실제 전송된 바이트 수 (출력)
 * @return NETWORK_SUCCESS, NETWORK_WOULD_BLOCK, 또는 오류 코드
 */
network_result_t network_socket_send(network_socket_t* sock, const void* data,
    int length, int* bytes_sent);

/**
 * 데이터 수신 (논블로킹)
 * @param sock 소켓
 * @param buffer 수신 버퍼
 * @param buffer_size 버퍼 크기
 * @param bytes_received 실제 받은 바이트 수 (출력)
 * @return NETWORK_SUCCESS, NETWORK_WOULD_BLOCK, 또는 오류 코드
 */
network_result_t network_socket_recv(network_socket_t* sock, void* buffer,
    int buffer_size, int* bytes_received);

/**
 * 모든 데이터를 전송할 때까지 반복 (블로킹)
 * @param sock 소켓
 * @param data 전송할 데이터
 * @param length 데이터 길이
 * @return 성공 시 NETWORK_SUCCESS
 */
network_result_t network_socket_send_all(network_socket_t* sock, const void* data, int length);

/**
 * 지정된 바이트 수만큼 수신할 때까지 반복 (블로킹)
 * @param sock 소켓
 * @param buffer 수신 버퍼
 * @param length 받을 바이트 수
 * @return 성공 시 NETWORK_SUCCESS
 */
network_result_t network_socket_recv_all(network_socket_t* sock, void* buffer, int length);

// =============================================================================
// 메시지 레벨 송수신 (protocol.h와 연동)
// =============================================================================

/**
 * 메시지 전송 (헤더 + 페이로드)
 * @param sock 소켓
 * @param msg 전송할 메시지
 * @return 성공 시 NETWORK_SUCCESS
 */
network_result_t network_socket_send_message(network_socket_t* sock, const message_t* msg);

/**
 * 메시지 수신 (헤더 먼저 읽고 페이로드 크기 확인 후 전체 읽기)
 * @param sock 소켓
 * @return 수신된 메시지, 실패 시 NULL
 */
message_t* network_socket_recv_message(network_socket_t* sock);

// =============================================================================
// 연결 관리
// =============================================================================

/**
 * 소켓 연결 종료 (graceful close)
 * @param sock 소켓
 * @return 성공 시 NETWORK_SUCCESS
 */
network_result_t network_socket_close(network_socket_t* sock);

/**
 * 소켓 연결이 살아있는지 확인
 * @param sock 소켓
 * @return 연결되어 있으면 1, 아니면 0
 */
int network_socket_is_connected(const network_socket_t* sock);

/**
 * 소켓에서 읽을 데이터가 있는지 확인 (select 사용)
 * @param sock 소켓
 * @param timeout_ms 타임아웃 (밀리초, 0이면 즉시 반환)
 * @return 데이터 있으면 1, 없으면 0, 오류 시 -1
 */
int network_socket_has_data(network_socket_t* sock, int timeout_ms);

/**
 * 소켓이 쓰기 가능한지 확인 (select 사용)
 * @param sock 소켓
 * @param timeout_ms 타임아웃 (밀리초)
 * @return 쓰기 가능하면 1, 아니면 0, 오류 시 -1
 */
int network_socket_can_write(network_socket_t* sock, int timeout_ms);

// =============================================================================
// 유틸리티 함수들
// =============================================================================

/**
 * IP 주소를 문자열로 변환
 * @param addr 주소 구조체
 * @param buffer 출력 버퍼
 * @param buffer_size 버퍼 크기
 * @return 변환된 IP 문자열
 */
const char* network_addr_to_string(const struct sockaddr_in* addr, char* buffer, size_t buffer_size);

/**
 * 호스트명을 IP 주소로 변환
 * @param hostname 호스트명
 * @param ip_buffer IP 주소 출력 버퍼
 * @param buffer_size 버퍼 크기
 * @return 성공 시 NETWORK_SUCCESS
 */
network_result_t network_resolve_hostname(const char* hostname, char* ip_buffer, size_t buffer_size);

/**
 * 네트워크 결과 코드를 문자열로 변환
 * @param result 결과 코드
 * @return 결과 문자열
 */
const char* network_result_to_string(network_result_t result);

/**
 * 소켓 상태를 문자열로 변환
 * @param state 소켓 상태
 * @return 상태 문자열
 */
const char* network_socket_state_to_string(socket_state_t state);

/**
 * 소켓 통계 정보 출력 (디버깅용)
 * @param sock 소켓
 */
void network_socket_print_stats(const network_socket_t* sock);

/**
 * 로컬 IP 주소 얻기
 * @param buffer 출력 버퍼
 * @param buffer_size 버퍼 크기
 * @return 성공 시 로컬 IP 문자열, 실패 시 NULL
 */
const char* network_get_local_ip(char* buffer, size_t buffer_size);

#endif // NETWORK_H