#ifndef SERVER_H
#define SERVER_H

#include "common_headers.h"
#include "protocol.h"
#include "message.h"
#include "network.h"
#include "utils.h"

#include <time.h>

// =============================================================================
// 서버 상수 정의
// =============================================================================

#define DEFAULT_SERVER_PORT         8080        // 기본 서버 포트
#define MAX_SERVER_CLIENTS          64          // 최대 클라이언트 수 (FD_SETSIZE)
#define SERVER_SELECT_TIMEOUT_MS    100         // select 타임아웃 (밀리초)
#define HEARTBEAT_INTERVAL_SEC      30          // 하트비트 간격 (초)
#define CLIENT_TIMEOUT_SEC          60          // 클라이언트 타임아웃 (초)
#define SERVER_SHUTDOWN_TIMEOUT_MS  5000        // 서버 종료 타임아웃

// =============================================================================
// 서버 상태 및 설정 구조체
// =============================================================================

// 서버 실행 상태
typedef enum {
    SERVER_STATE_STOPPED,           // 중지됨
    SERVER_STATE_STARTING,          // 시작 중
    SERVER_STATE_RUNNING,           // 실행 중
    SERVER_STATE_STOPPING,          // 종료 중
    SERVER_STATE_ERROR              // 오류 상태
} server_state_t;

// 서버 설정 구조체
typedef struct {
    uint16_t port;                  // 서버 포트
    char bind_interface[16];        // 바인드할 인터페이스 IP (빈 문자열이면 모든 인터페이스)
    int max_clients;                // 최대 클라이언트 수
    int select_timeout_ms;          // select 타임아웃
    int heartbeat_interval_sec;     // 하트비트 간격
    int client_timeout_sec;         // 클라이언트 타임아웃
    log_level_t log_level;          // 로그 레벨
    int enable_heartbeat;           // 하트비트 활성화 여부
} server_config_t;

// 클라이언트 정보 구조체
typedef struct {
    uint32_t id;                    // 클라이언트 고유 ID
    network_socket_t* socket;       // 네트워크 소켓
    char username[MAX_USERNAME_LENGTH]; // 사용자명
    time_t connected_at;            // 연결 시간
    time_t last_activity;           // 마지막 활동 시간
    time_t last_heartbeat;          // 마지막 하트비트 시간

    // 상태 정보
    int is_authenticated;           // 인증 완료 여부
    int is_active;                  // 활성 상태 여부

    // 통계 정보
    uint32_t messages_sent;         // 보낸 메시지 수
    uint32_t messages_received;     // 받은 메시지 수
} client_info_t;

// 서버 통계 구조체
typedef struct {
    time_t start_time;              // 서버 시작 시간
    uint32_t total_connections;     // 총 연결 수
    uint32_t current_connections;   // 현재 연결 수
    uint32_t max_concurrent_connections; // 최대 동시 연결 수
    uint64_t total_messages;        // 총 메시지 수
    uint64_t total_bytes_sent;      // 총 송신 바이트
    uint64_t total_bytes_received;  // 총 수신 바이트
    uint32_t authentication_failures; // 인증 실패 횟수
    uint32_t protocol_errors;       // 프로토콜 오류 횟수
} server_statistics_t;

// =============================================================================
// 메인 서버 구조체
// =============================================================================

typedef struct {
    // 서버 상태
    server_state_t state;           // 현재 상태
    server_config_t config;         // 서버 설정

    // 네트워크
    network_socket_t* listen_socket; // 리스닝 소켓

    // 클라이언트 관리
    client_info_t clients[MAX_SERVER_CLIENTS]; // 클라이언트 배열
    int client_count;               // 현재 클라이언트 수
    uint32_t next_client_id;        // 다음 클라이언트 ID

    // select용 파일 디스크립터 셋
    fd_set master_read_fds;         // 마스터 읽기 FD 셋
    fd_set working_read_fds;        // 작업용 읽기 FD 셋
    SOCKET max_fd;                  // 최대 파일 디스크립터

    // 시간 관리
    time_t last_heartbeat_check;    // 마지막 하트비트 체크 시간
    time_t last_cleanup;            // 마지막 정리 작업 시간

    // 통계 및 모니터링
    server_statistics_t stats;      // 서버 통계

    // 종료 신호
    volatile int should_shutdown;   // 종료 신호 플래그
} chat_server_t;

// =============================================================================
// 서버 라이프사이클 함수들
// =============================================================================

/**
 * 서버 인스턴스 생성 및 초기화
 * @param config 서버 설정 (NULL이면 기본값 사용)
 * @return 생성된 서버 인스턴스, 실패 시 NULL
 */
chat_server_t* server_create(const server_config_t* config);

/**
 * 서버 인스턴스 해제
 * @param server 해제할 서버 인스턴스
 */
void server_destroy(chat_server_t* server);

/**
 * 서버 시작 (소켓 바인딩, 리스닝 시작)
 * @param server 서버 인스턴스
 * @return 성공 시 0, 실패 시 음수
 */
int server_start(chat_server_t* server);

/**
 * 서버 중지 (모든 연결 종료, 리소스 정리)
 * @param server 서버 인스턴스
 * @return 성공 시 0, 실패 시 음수
 */
int server_stop(chat_server_t* server);

/**
 * 서버 메인 루프 실행 (블로킹)
 * @param server 서버 인스턴스
 * @return 정상 종료 시 0, 오류 시 음수
 */
int server_run(chat_server_t* server);

/**
 * 서버에 종료 신호 전송 (다른 스레드에서 호출 가능)
 * @param server 서버 인스턴스
 */
void server_signal_shutdown(chat_server_t* server);

// =============================================================================
// 클라이언트 관리 함수들
// =============================================================================

/**
 * 새 클라이언트 추가
 * @param server 서버 인스턴스
 * @param client_socket 클라이언트 소켓
 * @return 클라이언트 ID, 실패 시 0
 */
uint32_t server_add_client(chat_server_t* server, network_socket_t* client_socket);

/**
 * 클라이언트 제거
 * @param server 서버 인스턴스
 * @param client_id 클라이언트 ID
 * @return 성공 시 0, 실패 시 음수
 */
int server_remove_client(chat_server_t* server, uint32_t client_id);

/**
 * 클라이언트 검색 (ID로)
 * @param server 서버 인스턴스
 * @param client_id 클라이언트 ID
 * @return 클라이언트 정보 포인터, 없으면 NULL
 */
client_info_t* server_find_client_by_id(chat_server_t* server, uint32_t client_id);

/**
 * 클라이언트 검색 (소켓으로)
 * @param server 서버 인스턴스
 * @param socket 소켓
 * @return 클라이언트 정보 포인터, 없으면 NULL
 */
client_info_t* server_find_client_by_socket(chat_server_t* server, network_socket_t* socket);

/**
 * 클라이언트 검색 (사용자명으로)
 * @param server 서버 인스턴스
 * @param username 사용자명
 * @return 클라이언트 정보 포인터, 없으면 NULL
 */
client_info_t* server_find_client_by_username(chat_server_t* server, const char* username);

/**
 * 활성 클라이언트 수 반환
 * @param server 서버 인스턴스
 * @return 활성 클라이언트 수
 */
int server_get_active_client_count(const chat_server_t* server);

// =============================================================================
// 메시지 처리 함수들
// =============================================================================

/**
 * 특정 클라이언트에게 메시지 전송
 * @param server 서버 인스턴스
 * @param client_id 대상 클라이언트 ID
 * @param message 전송할 메시지
 * @return 성공 시 0, 실패 시 음수
 */
int server_send_to_client(chat_server_t* server, uint32_t client_id, const message_t* message);

/**
 * 모든 클라이언트에게 메시지 브로드캐스트
 * @param server 서버 인스턴스
 * @param message 브로드캐스트할 메시지
 * @param exclude_client_id 제외할 클라이언트 ID (0이면 모든 클라이언트)
 * @return 전송 성공한 클라이언트 수
 */
int server_broadcast_message(chat_server_t* server, const message_t* message, uint32_t exclude_client_id);

/**
 * 인증된 클라이언트에게만 메시지 브로드캐스트
 * @param server 서버 인스턴스
 * @param message 브로드캐스트할 메시지
 * @param exclude_client_id 제외할 클라이언트 ID
 * @return 전송 성공한 클라이언트 수
 */
int server_broadcast_to_authenticated(chat_server_t* server, const message_t* message, uint32_t exclude_client_id);

// =============================================================================
// 유지보수 및 관리 함수들
// =============================================================================

/**
 * 비활성 클라이언트 정리
 * @param server 서버 인스턴스
 * @return 정리된 클라이언트 수
 */
int server_cleanup_inactive_clients(chat_server_t* server);

/**
 * 하트비트 확인 및 전송
 * @param server 서버 인스턴스
 * @return 처리된 클라이언트 수
 */
int server_check_heartbeats(chat_server_t* server);

/**
 * FD 셋 업데이트 (select용)
 * @param server 서버 인스턴스
 */
void server_update_fd_sets(chat_server_t* server);

// =============================================================================
// 설정 및 상태 조회 함수들
// =============================================================================

/**
 * 기본 서버 설정 생성
 * @return 기본 설정 구조체
 */
server_config_t server_get_default_config(void);

/**
 * 서버 설정 유효성 검증
 * @param config 검증할 설정
 * @return 유효하면 1, 무효하면 0
 */
int server_validate_config(const server_config_t* config);

/**
 * 서버 상태를 문자열로 변환
 * @param state 서버 상태
 * @return 상태 문자열
 */
const char* server_state_to_string(server_state_t state);

/**
 * 서버 상태 정보 출력
 * @param server 서버 인스턴스
 */
void server_print_status(const chat_server_t* server);

/**
 * 서버 통계 정보 출력
 * @param server 서버 인스턴스
 */
void server_print_statistics(const chat_server_t* server);

/**
 * 클라이언트 목록 출력
 * @param server 서버 인스턴스
 */
void server_print_client_list(const chat_server_t* server);

/**
 * 서버 실행 시간 반환 (초)
 * @param server 서버 인스턴스
 * @return 실행 시간 (초)
 */
int server_get_uptime_seconds(const chat_server_t* server);

// =============================================================================
// 신호 처리 및 유틸리티
// =============================================================================

/**
 * SIGINT 신호 핸들러 설정 (Ctrl+C 처리)
 * @param server 서버 인스턴스 (전역으로 저장됨)
 */
void server_setup_signal_handlers(chat_server_t* server);

/**
 * 서버 설정을 파일에서 로드 (추후 구현 예정)
 * @param filename 설정 파일명
 * @param config 로드될 설정 구조체
 * @return 성공 시 0, 실패 시 음수
 */
int server_load_config_from_file(const char* filename, server_config_t* config);

/**
 * 서버 PID를 파일에 저장 (추후 구현 예정)
 * @param server 서버 인스턴스
 * @param pid_filename PID 파일명
 * @return 성공 시 0, 실패 시 음수
 */
int server_save_pid_file(const chat_server_t* server, const char* pid_filename);

#endif // SERVER_H