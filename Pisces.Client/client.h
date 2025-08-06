#ifndef CLIENT_H
#define CLIENT_H

#include "protocol.h"
#include "message.h"
#include "network.h"
#include "utils.h"
#include <windows.h>
#include <time.h>

// =============================================================================
// 클라이언트 상수 정의
// =============================================================================

#define CLIENT_VERSION              "1.0.0"
#define DEFAULT_SERVER_HOST         "localhost"
#define DEFAULT_SERVER_PORT         8080        // 기본 서버 포트
#define CLIENT_RECONNECT_INTERVAL   5           // 재연결 간격 (초)
#define CLIENT_HEARTBEAT_TIMEOUT    30          // 하트비트 타임아웃 (초)
#define CLIENT_CONNECT_TIMEOUT      10          // 연결 타임아웃 (초)
#define MAX_MESSAGE_QUEUE_SIZE      100         // 메시지 큐 최대 크기
#define MAX_COMMAND_LENGTH          256         // 명령어 최대 길이
#define MAX_CHAT_MESSAGE_LENGTH     1024        // 채팅 메시지 최대 길이

// =============================================================================
// 클라이언트 상태 머신
// =============================================================================

// 클라이언트 연결 상태
typedef enum {
    CLIENT_STATE_DISCONNECTED,      // 연결 안됨
    CLIENT_STATE_CONNECTING,        // 연결 중
    CLIENT_STATE_CONNECTED,         // 연결됨 (인증 전)
    CLIENT_STATE_AUTHENTICATING,    // 인증 중
    CLIENT_STATE_AUTHENTICATED,     // 인증됨 (채팅 가능)
    CLIENT_STATE_RECONNECTING,      // 재연결 중
    CLIENT_STATE_DISCONNECTING,     // 연결 종료 중
    CLIENT_STATE_ERROR              // 오류 상태
} client_state_t;

// 상태 전환 이벤트
typedef enum {
    CLIENT_EVENT_CONNECT_REQUESTED,     // 연결 요청
    CLIENT_EVENT_CONNECTED,             // 연결 완료
    CLIENT_EVENT_CONNECTION_FAILED,     // 연결 실패
    CLIENT_EVENT_AUTH_REQUESTED,        // 인증 요청
    CLIENT_EVENT_AUTH_SUCCESS,          // 인증 성공
    CLIENT_EVENT_AUTH_FAILED,           // 인증 실패
    CLIENT_EVENT_DISCONNECT_REQUESTED,  // 연결 종료 요청
    CLIENT_EVENT_CONNECTION_LOST,       // 연결 끊김
    CLIENT_EVENT_ERROR_OCCURRED,        // 오류 발생
    CLIENT_EVENT_RECONNECT_REQUESTED    // 재연결 요청
} client_event_t;

// =============================================================================
// 클라이언트 설정 구조체
// =============================================================================

typedef struct {
    char server_host[256];              // 서버 호스트
    uint16_t server_port;               // 서버 포트
    char username[MAX_USERNAME_LENGTH]; // 사용자명

    // 동작 설정
    int auto_reconnect;                 // 자동 재연결 여부
    int reconnect_interval;             // 재연결 간격 (초)
    int connect_timeout;                // 연결 타임아웃 (초)
    int heartbeat_timeout;              // 하트비트 타임아웃 (초)

    // UI 설정
    int show_timestamps;                // 타임스탬프 표시
    int show_system_messages;           // 시스템 메시지 표시
    log_level_t log_level;              // 로그 레벨

    // 고급 설정
    int message_queue_size;             // 메시지 큐 크기
    int network_buffer_size;            // 네트워크 버퍼 크기
} client_config_t;

// =============================================================================
// 스레드 간 통신 구조체
// =============================================================================

// UI → Network 스레드 메시지
typedef enum {
    UI_CMD_CONNECT,                     // 서버 연결
    UI_CMD_DISCONNECT,                  // 연결 해제
    UI_CMD_SEND_CHAT,                   // 채팅 메시지 전송
    UI_CMD_REQUEST_USER_LIST,           // 사용자 목록 요청
    UI_CMD_SHUTDOWN                     // 종료
} ui_command_type_t;

typedef struct {
    ui_command_type_t type;             // 명령 타입
    char data[MAX_CHAT_MESSAGE_LENGTH]; // 명령 데이터
} ui_command_t;

// Network → UI 스레드 메시지
typedef enum {
    NET_EVENT_STATE_CHANGED,            // 상태 변경
    NET_EVENT_CHAT_RECEIVED,            // 채팅 메시지 수신
    NET_EVENT_USER_LIST_RECEIVED,       // 사용자 목록 수신
    NET_EVENT_USER_JOINED,              // 사용자 입장
    NET_EVENT_USER_LEFT,                // 사용자 퇴장
    NET_EVENT_ERROR_OCCURRED,           // 오류 발생
    NET_EVENT_CONNECTION_STATUS         // 연결 상태 업데이트
} network_event_type_t;

typedef struct {
    network_event_type_t type;          // 이벤트 타입
    client_state_t new_state;           // 새 상태 (상태 변경 시)
    char message[MAX_CHAT_MESSAGE_LENGTH]; // 메시지 내용
    char username[MAX_USERNAME_LENGTH];    // 사용자명 (관련 있는 경우)
    time_t timestamp;                   // 타임스탬프
} network_event_t;

// =============================================================================
// 메시지 큐 구조체
// =============================================================================

typedef struct {
    ui_command_t* commands;             // 명령 배열
    int capacity;                       // 용량
    int size;                          // 현재 크기
    int front;                         // 큐 앞
    int rear;                          // 큐 뒤
    CRITICAL_SECTION lock;             // 동기화
} command_queue_t;

typedef struct {
    network_event_t* events;           // 이벤트 배열
    int capacity;                      // 용량
    int size;                         // 현재 크기
    int front;                        // 큐 앞
    int rear;                         // 큐 뒤
    CRITICAL_SECTION lock;            // 동기화
} event_queue_t;

// =============================================================================
// 클라이언트 통계 구조체
// =============================================================================

typedef struct {
    time_t connected_at;               // 연결 시간
    time_t last_activity;              // 마지막 활동
    uint32_t messages_sent;            // 보낸 메시지 수
    uint32_t messages_received;        // 받은 메시지 수
    uint64_t bytes_sent;               // 보낸 바이트 수
    uint64_t bytes_received;           // 받은 바이트 수
    uint32_t reconnect_count;          // 재연결 횟수
    uint32_t connection_failures;      // 연결 실패 횟수
} client_statistics_t;

// =============================================================================
// 메인 클라이언트 구조체
// =============================================================================

typedef struct {
    // 클라이언트 상태
    client_state_t current_state;      // 현재 상태
    client_config_t config;            // 설정

    // 네트워크
    network_socket_t* server_socket;   // 서버 소켓

    // 스레드 관리
    HANDLE network_thread;             // 네트워크 스레드
    HANDLE ui_thread;                  // UI 스레드 (메인)
    volatile int should_shutdown;      // 종료 플래그

    // 스레드 간 통신
    command_queue_t* command_queue;    // UI → Network
    event_queue_t* event_queue;        // Network → UI

    // 동기화
    CRITICAL_SECTION state_lock;       // 상태 보호
    HANDLE network_ready_event;        // 네트워크 스레드 준비 완료
    HANDLE shutdown_event;             // 종료 이벤트

    // 상태 정보
    time_t last_heartbeat;             // 마지막 하트비트
    time_t connection_start_time;      // 연결 시작 시간
    char last_error[256];              // 마지막 오류 메시지

    // 통계
    client_statistics_t stats;         // 클라이언트 통계
} chat_client_t;

// =============================================================================
// 클라이언트 라이프사이클 함수들
// =============================================================================

/**
 * 클라이언트 인스턴스 생성
 * @param config 클라이언트 설정 (NULL이면 기본값)
 * @return 생성된 클라이언트, 실패 시 NULL
 */
chat_client_t* client_create(const client_config_t* config);

/**
 * 클라이언트 인스턴스 해제
 * @param client 해제할 클라이언트
 */
void client_destroy(chat_client_t* client);

/**
 * 클라이언트 시작 (스레드들 시작)
 * @param client 클라이언트 인스턴스
 * @return 성공 시 0, 실패 시 음수
 */
int client_start(chat_client_t* client);

/**
 * 클라이언트 종료 (모든 스레드 종료 대기)
 * @param client 클라이언트 인스턴스
 * @return 성공 시 0, 실패 시 음수
 */
int client_shutdown(chat_client_t* client);

/**
 * 클라이언트 메인 루프 (UI 스레드에서 실행)
 * @param client 클라이언트 인스턴스
 * @return 정상 종료 시 0, 오류 시 음수
 */
int client_run(chat_client_t* client);

// =============================================================================
// 상태 관리 함수들
// =============================================================================

/**
 * 클라이언트 상태 변경 (스레드 안전)
 * @param client 클라이언트 인스턴스
 * @param new_state 새 상태
 * @param event 상태 변경 이벤트
 * @return 성공 시 0, 실패 시 음수
 */
int client_change_state(chat_client_t* client, client_state_t new_state, client_event_t event);

/**
 * 현재 상태 조회 (스레드 안전)
 * @param client 클라이언트 인스턴스
 * @return 현재 상태
 */
client_state_t client_get_current_state(chat_client_t* client);

/**
 * 상태를 문자열로 변환
 * @param state 클라이언트 상태
 * @return 상태 문자열
 */
const char* client_state_to_string(client_state_t state);

/**
 * 이벤트를 문자열로 변환
 * @param event 클라이언트 이벤트
 * @return 이벤트 문자열
 */
const char* client_event_to_string(client_event_t event);

// =============================================================================
// 연결 및 인증 함수들
// =============================================================================

/**
 * 서버 연결 요청
 * @param client 클라이언트 인스턴스
 * @param host 서버 호스트
 * @param port 서버 포트
 * @return 성공 시 0, 실패 시 음수
 */
int client_connect_to_server(chat_client_t* client, const char* host, uint16_t port);

/**
 * 사용자 인증 요청
 * @param client 클라이언트 인스턴스
 * @param username 사용자명
 * @return 성공 시 0, 실패 시 음수
 */
int client_authenticate(chat_client_t* client, const char* username);

/**
 * 서버 연결 해제
 * @param client 클라이언트 인스턴스
 * @return 성공 시 0, 실패 시 음수
 */
int client_disconnect(chat_client_t* client);

/**
 * 연결 상태 확인
 * @param client 클라이언트 인스턴스
 * @return 연결되어 있으면 1, 아니면 0
 */
int client_is_connected(const chat_client_t* client);

/**
 * 인증 상태 확인
 * @param client 클라이언트 인스턴스
 * @return 인증되어 있으면 1, 아니면 0
 */
int client_is_authenticated(const chat_client_t* client);

// =============================================================================
// 메시지 송수신 함수들
// =============================================================================

/**
 * 채팅 메시지 전송
 * @param client 클라이언트 인스턴스
 * @param message 전송할 메시지
 * @return 성공 시 0, 실패 시 음수
 */
int client_send_chat_message(chat_client_t* client, const char* message);

/**
 * 사용자 목록 요청
 * @param client 클라이언트 인스턴스
 * @return 성공 시 0, 실패 시 음수
 */
int client_request_user_list(chat_client_t* client);

/**
 * 하트비트 응답 전송
 * @param client 클라이언트 인스턴스
 * @return 성공 시 0, 실패 시 음수
 */
int client_send_heartbeat_ack(chat_client_t* client);

// =============================================================================
// 큐 관리 함수들
// =============================================================================

/**
 * 명령 큐 생성
 * @param capacity 큐 용량
 * @return 생성된 큐, 실패 시 NULL
 */
command_queue_t* command_queue_create(int capacity);

/**
 * 명령 큐 해제
 * @param queue 해제할 큐
 */
void command_queue_destroy(command_queue_t* queue);

/**
 * 명령 큐에 추가 (스레드 안전)
 * @param queue 큐
 * @param command 추가할 명령
 * @return 성공 시 0, 실패 시 음수 (큐 가득참)
 */
int command_queue_push(command_queue_t* queue, const ui_command_t* command);

/**
 * 명령 큐에서 제거 (스레드 안전)
 * @param queue 큐
 * @param command 가져올 명령 (출력)
 * @param timeout_ms 타임아웃 (밀리초, 0이면 즉시 반환)
 * @return 성공 시 0, 큐 비어있으면 -1, 타임아웃 시 -2
 */
int command_queue_pop(command_queue_t* queue, ui_command_t* command, int timeout_ms);

/**
 * 이벤트 큐 생성
 * @param capacity 큐 용량
 * @return 생성된 큐, 실패 시 NULL
 */
event_queue_t* event_queue_create(int capacity);

/**
 * 이벤트 큐 해제
 * @param queue 해제할 큐
 */
void event_queue_destroy(event_queue_t* queue);

/**
 * 이벤트 큐에 추가 (스레드 안전)
 * @param queue 큐
 * @param event 추가할 이벤트
 * @return 성공 시 0, 실패 시 음수 (큐 가득참)
 */
int event_queue_push(event_queue_t* queue, const network_event_t* event);

/**
 * 이벤트 큐에서 제거 (스레드 안전)
 * @param queue 큐
 * @param event 가져올 이벤트 (출력)
 * @param timeout_ms 타임아웃 (밀리초, 0이면 즉시 반환)
 * @return 성공 시 0, 큐 비어있으면 -1, 타임아웃 시 -2
 */
int event_queue_pop(event_queue_t* queue, network_event_t* event, int timeout_ms);

// =============================================================================
// 설정 및 유틸리티 함수들
// =============================================================================

/**
 * 기본 클라이언트 설정 반환
 * @return 기본 설정
 */
client_config_t client_get_default_config(void);

/**
 * 클라이언트 설정 검증
 * @param config 검증할 설정
 * @return 유효하면 1, 무효하면 0
 */
int client_validate_config(const client_config_t* config);

/**
 * 클라이언트 상태 정보 출력
 * @param client 클라이언트 인스턴스
 */
void client_print_status(const chat_client_t* client);

/**
 * 클라이언트 통계 출력
 * @param client 클라이언트 인스턴스
 */
void client_print_statistics(const chat_client_t* client);

/**
 * 마지막 오류 메시지 설정 (스레드 안전)
 * @param client 클라이언트 인스턴스
 * @param error_message 오류 메시지
 */
void client_set_last_error(chat_client_t* client, const char* error_message);

/**
 * 마지막 오류 메시지 조회 (스레드 안전)
 * @param client 클라이언트 인스턴스
 * @param buffer 오류 메시지 버퍼
 * @param buffer_size 버퍼 크기
 * @return 오류 메시지 문자열
 */
const char* client_get_last_error(chat_client_t* client, char* buffer, size_t buffer_size);

// =============================================================================
// 네트워크 스레드 함수 (별도 파일에서 구현)
// =============================================================================

/**
 * 네트워크 스레드 진입점
 * @param param 클라이언트 인스턴스 포인터
 * @return 스레드 종료 코드
 */
DWORD WINAPI client_network_thread_proc(LPVOID param);

// =============================================================================
// UI 관련 함수들 (별도 파일에서 구현)
// =============================================================================

/**
 * 콘솔 UI 초기화
 * @param client 클라이언트 인스턴스
 * @return 성공 시 0, 실패 시 음수
 */
int client_ui_initialize(chat_client_t* client);

/**
 * 콘솔 UI 정리
 * @param client 클라이언트 인스턴스
 */
void client_ui_cleanup(chat_client_t* client);

/**
 * 사용자 입력 처리
 * @param client 클라이언트 인스턴스
 * @param input 입력 문자열
 * @return 계속 실행하면 0, 종료 요청 시 1
 */
int client_ui_handle_input(chat_client_t* client, const char* input);

/**
 * 네트워크 이벤트 화면 출력
 * @param client 클라이언트 인스턴스
 * @param event 출력할 이벤트
 */
void client_ui_display_event(chat_client_t* client, const network_event_t* event);

#endif // CLIENT_H