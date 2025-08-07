#ifndef UI_H
#define UI_H

#include "client.h"
#include <stdio.h>
#include <conio.h>  // Windows 콘솔 입력

// =============================================================================
// UI 상수 정의
// =============================================================================

#define MAX_INPUT_LENGTH        512         // 최대 입력 길이
#define UI_REFRESH_INTERVAL_MS  50          // UI 새로고침 간격
#define CHAT_HISTORY_SIZE       100         // 채팅 히스토리 크기
#define INPUT_PROMPT           "> "         // 입력 프롬프트
#define COMMAND_PREFIX         "/"          // 명령어 접두사

// =============================================================================
// UI 색상 코드 (Windows 콘솔)
// =============================================================================

#define COLOR_RESET            0x07         // 기본 색상 (흰색 글자, 검은 배경)
#define COLOR_SYSTEM           0x0E         // 시스템 메시지 (노란색)
#define COLOR_ERROR            0x0C         // 오류 메시지 (빨간색)
#define COLOR_SUCCESS          0x0A         // 성공 메시지 (녹색)
#define COLOR_INFO             0x0B         // 정보 메시지 (청록색)
#define COLOR_CHAT             0x07         // 일반 채팅 (흰색)
#define COLOR_USERNAME         0x0D         // 사용자명 (자홍색)
#define COLOR_TIMESTAMP        0x08         // 타임스탬프 (회색)

// =============================================================================
// UI 상태 구조체
// =============================================================================

typedef struct {
    char input_buffer[MAX_INPUT_LENGTH];    // 현재 입력 버퍼
    int input_pos;                          // 입력 커서 위치
    int input_active;                       // 입력 활성 상태

    // 화면 레이아웃
    int console_width;                      // 콘솔 너비
    int console_height;                     // 콘솔 높이
    int chat_area_height;                   // 채팅 영역 높이
    int status_line;                        // 상태 라인 위치
    int input_line;                         // 입력 라인 위치

    // 채팅 히스토리
    char chat_history[CHAT_HISTORY_SIZE][512];  // 채팅 히스토리
    int history_count;                      // 히스토리 개수
    int history_scroll;                     // 스크롤 위치

    // 상태 정보
    time_t last_update;                     // 마지막 업데이트 시간
    int need_refresh;                       // 새로고침 필요 여부
} ui_state_t;

// =============================================================================
// UI 초기화 및 정리 함수들
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
 * UI 상태 초기화
 * @return 생성된 UI 상태, 실패 시 NULL
 */
ui_state_t* ui_state_create(void);

/**
 * UI 상태 해제
 * @param ui_state 해제할 UI 상태
 */
void ui_state_destroy(ui_state_t* ui_state);

// =============================================================================
// 화면 출력 함수들
// =============================================================================

/**
 * 전체 화면 새로고침
 * @param client 클라이언트 인스턴스
 * @param ui_state UI 상태
 */
void ui_refresh_screen(chat_client_t* client, ui_state_t* ui_state);

/**
 * 채팅 영역 출력
 * @param client 클라이언트 인스턴스
 * @param ui_state UI 상태
 */
void ui_draw_chat_area(chat_client_t* client, ui_state_t* ui_state);

/**
 * 상태 라인 출력
 * @param client 클라이언트 인스턴스
 * @param ui_state UI 상태
 */
void ui_draw_status_line(chat_client_t* client, ui_state_t* ui_state);

/**
 * 입력 라인 출력
 * @param client 클라이언트 인스턴스
 * @param ui_state UI 상태
 */
void ui_draw_input_line(chat_client_t* client, ui_state_t* ui_state);

/**
 * 도움말 출력
 * @param client 클라이언트 인스턴스
 */
void ui_show_help(chat_client_t* client);

/**
 * 환영 메시지 출력
 * @param client 클라이언트 인스턴스
 */
void ui_show_welcome_message(chat_client_t* client);

// =============================================================================
// 사용자 입력 처리 함수들
// =============================================================================

/**
 * 사용자 입력 처리 (메인 루프에서 호출)
 * @param client 클라이언트 인스턴스
 * @param ui_state UI 상태
 * @return 계속 실행하면 0, 종료 요청 시 1
 */
int ui_handle_input(chat_client_t* client, ui_state_t* ui_state);

/**
 * 키보드 입력 처리
 * @param client 클라이언트 인스턴스
 * @param ui_state UI 상태
 * @param key 입력된 키
 * @return 처리 완료 시 0
 */
int ui_process_key(chat_client_t* client, ui_state_t* ui_state, int key);

/**
 * 명령줄 처리
 * @param client 클라이언트 인스턴스
 * @param input 입력 문자열
 * @return 계속 실행하면 0, 종료 요청 시 1
 */
int client_ui_handle_input(chat_client_t* client, const char* input);

/**
 * 명령어 처리
 * @param client 클라이언트 인스턴스
 * @param command 명령어 (/ 접두사 제외)
 * @return 성공 시 0, 실패 시 음수
 */
int ui_process_command(chat_client_t* client, const char* command);

// =============================================================================
// 이벤트 표시 함수들
// =============================================================================

/**
 * 네트워크 이벤트 화면 출력
 * @param client 클라이언트 인스턴스
 * @param event 출력할 이벤트
 */
void client_ui_display_event(chat_client_t* client, const network_event_t* event);

/**
 * 채팅 메시지 추가
 * @param ui_state UI 상태
 * @param username 사용자명 (NULL이면 시스템 메시지)
 * @param message 메시지 내용
 * @param timestamp 타임스탬프
 * @param color 색상 코드
 */
void ui_add_chat_message(ui_state_t* ui_state, const char* username,
    const char* message, time_t timestamp, int color);

/**
 * 시스템 메시지 추가
 * @param ui_state UI 상태
 * @param message 시스템 메시지
 * @param color 색상 코드
 */
void ui_add_system_message(ui_state_t* ui_state, const char* message, int color);

// =============================================================================
// 유틸리티 함수들
// =============================================================================

/**
 * 콘솔 색상 설정
 * @param color Windows 콘솔 색상 코드
 */
void ui_set_console_color(int color);

/**
 * 콘솔 커서 위치 설정
 * @param x X 좌표
 * @param y Y 좌표
 */
void ui_set_cursor_position(int x, int y);

/**
 * 콘솔 화면 지우기
 */
void ui_clear_screen(void);

/**
 * 콘솔 크기 얻기
 * @param width 너비 (출력)
 * @param height 높이 (출력)
 */
void ui_get_console_size(int* width, int* height);

/**
 * 현재 시간을 문자열로 포맷
 * @param timestamp 타임스탬프
 * @param buffer 출력 버퍼
 * @param buffer_size 버퍼 크기
 * @return 포맷된 시간 문자열
 */
const char* ui_format_timestamp(time_t timestamp, char* buffer, size_t buffer_size);

/**
 * 문자열 길이 제한 (말줄임표 추가)
 * @param str 원본 문자열
 * @param max_length 최대 길이
 * @param buffer 출력 버퍼
 * @param buffer_size 버퍼 크기
 * @return 제한된 문자열
 */
const char* ui_truncate_string(const char* str, int max_length,
    char* buffer, size_t buffer_size);

/**
 * 키 입력 확인 (논블로킹)
 * @return 키가 눌려있으면 1, 아니면 0
 */
int ui_kbhit(void);

/**
 * 키 입력 읽기
 * @return 입력된 키 코드
 */
int ui_getch(void);

int ui_process_user_input(chat_client_t* client, const char* input);

// =============================================================================
// 전역 UI 상태 (ui.c에서 정의)
// =============================================================================

extern ui_state_t* g_ui_state;

#endif // UI_H