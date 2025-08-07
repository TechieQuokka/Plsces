#include "ui.h"
#include <stdlib.h>
#include <string.h>
#include <windows.h>

// =============================================================================
// 전역 변수
// =============================================================================

ui_state_t* g_ui_state = NULL;
static HANDLE g_console_handle = NULL;
static CONSOLE_SCREEN_BUFFER_INFO g_original_console_info;

// =============================================================================
// UI 초기화 및 정리
// =============================================================================

int client_ui_initialize(chat_client_t* client) {
    if (!client) {
        return -1;
    }

    // 콘솔 핸들 얻기
    g_console_handle = GetStdHandle(STD_OUTPUT_HANDLE);
    if (g_console_handle == INVALID_HANDLE_VALUE) {
        LOG_ERROR("Failed to get console handle");
        return -1;
    }

    // 원래 콘솔 정보 저장
    if (!GetConsoleScreenBufferInfo(g_console_handle, &g_original_console_info)) {
        LOG_ERROR("Failed to get console info");
        return -1;
    }

    // UI 상태 생성
    g_ui_state = ui_state_create();
    if (!g_ui_state) {
        LOG_ERROR("Failed to create UI state");
        return -1;
    }

    // 콘솔 모드 설정 부분을 주석 처리!
    /*
    HANDLE input_handle = GetStdHandle(STD_INPUT_HANDLE);
    DWORD console_mode;
    if (GetConsoleMode(input_handle, &console_mode)) {
        // 라인 입력 모드 비활성화 (문자별 입력을 위해)
        console_mode &= ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT);
        SetConsoleMode(input_handle, console_mode);
    }
    */

    // 화면 초기화
    ui_clear_screen();
    ui_show_welcome_message(client);
    ui_refresh_screen(client, g_ui_state);

    LOG_INFO("Console UI initialized");
    return 0;
}

void client_ui_cleanup(chat_client_t* client) {
    // 원래 콘솔 설정 복원
    if (g_console_handle != INVALID_HANDLE_VALUE) {
        SetConsoleTextAttribute(g_console_handle, g_original_console_info.wAttributes);
    }

    // 콘솔 모드 복원
    HANDLE input_handle = GetStdHandle(STD_INPUT_HANDLE);
    DWORD console_mode;
    if (GetConsoleMode(input_handle, &console_mode)) {
        console_mode |= (ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT);
        SetConsoleMode(input_handle, console_mode);
    }

    ui_clear_screen();
    ui_set_console_color(COLOR_RESET);

    // UI 상태 해제
    if (g_ui_state) {
        ui_state_destroy(g_ui_state);
        g_ui_state = NULL;
    }

    printf("\nThank you for using the chat client!\n");
    LOG_INFO("Console UI cleaned up");
}

ui_state_t* ui_state_create(void) {
    ui_state_t* ui_state = (ui_state_t*)calloc(1, sizeof(ui_state_t));
    if (!ui_state) {
        return NULL;
    }

    // 콘솔 크기 얻기
    ui_get_console_size(&ui_state->console_width, &ui_state->console_height);

    // 레이아웃 계산
    ui_state->chat_area_height = ui_state->console_height - 4;  // 상태 라인, 입력 라인, 여백
    ui_state->status_line = ui_state->console_height - 3;
    ui_state->input_line = ui_state->console_height - 1;

    // 입력 상태 초기화
    ui_state->input_buffer[0] = '\0';
    ui_state->input_pos = 0;
    ui_state->input_active = 1;

    // 히스토리 초기화
    ui_state->history_count = 0;
    ui_state->history_scroll = 0;

    // 상태 초기화
    ui_state->last_update = time(NULL);
    ui_state->need_refresh = 1;

    return ui_state;
}

void ui_state_destroy(ui_state_t* ui_state) {
    if (ui_state) {
        free(ui_state);
    }
}

// =============================================================================
// 화면 출력 함수들
// =============================================================================

void ui_refresh_screen(chat_client_t* client, ui_state_t* ui_state) {
    if (!client || !ui_state) {
        return;
    }

    // 필요한 경우에만 새로고침
    if (!ui_state->need_refresh) {
        return;
    }

    ui_draw_chat_area(client, ui_state);
    ui_draw_status_line(client, ui_state);
    ui_draw_input_line(client, ui_state);

    ui_state->need_refresh = 0;
    ui_state->last_update = time(NULL);
}

void ui_draw_chat_area(chat_client_t* client, ui_state_t* ui_state) {
    if (!client || !ui_state) {
        return;
    }

    // 채팅 영역 지우기
    for (int line = 0; line < ui_state->chat_area_height; line++) {
        ui_set_cursor_position(0, line);
        for (int col = 0; col < ui_state->console_width; col++) {
            printf(" ");
        }
    }

    // 채팅 히스토리 출력
    int start_line = (ui_state->history_count > ui_state->chat_area_height) ?
        ui_state->history_count - ui_state->chat_area_height : 0;

    for (int i = 0; i < ui_state->chat_area_height && start_line + i < ui_state->history_count; i++) {
        ui_set_cursor_position(0, i);
        printf("%s", ui_state->chat_history[start_line + i]);
    }
}

void ui_draw_status_line(chat_client_t* client, ui_state_t* ui_state) {
    if (!client || !ui_state) {
        return;
    }

    // 상태 라인 위치로 이동
    ui_set_cursor_position(0, ui_state->status_line);

    // 구분선 출력
    ui_set_console_color(COLOR_SYSTEM);
    for (int i = 0; i < ui_state->console_width; i++) {
        printf("-");
    }

    // 상태 정보 출력
    ui_set_cursor_position(0, ui_state->status_line);

    client_state_t state = client_get_current_state(client);
    const char* state_str = client_state_to_string(state);

    char status_text[256];
    if (client_is_authenticated(client)) {
        sprintf_s(status_text, sizeof(status_text),
            "Connected as '%s' | %s | Type /help for commands",
            client->config.username, state_str);
    }
    else if (client_is_connected(client)) {
        sprintf_s(status_text, sizeof(status_text),
            "Connected to %s:%d | %s | Authenticating...",
            client->config.server_host, client->config.server_port, state_str);
    }
    else {
        sprintf_s(status_text, sizeof(status_text),
            "Not connected | %s | Type /connect <host> <port> to start",
            state_str);
    }

    // 상태 색상 선택
    int color = COLOR_INFO;
    switch (state) {
    case CLIENT_STATE_AUTHENTICATED: color = COLOR_SUCCESS; break;
    case CLIENT_STATE_ERROR: color = COLOR_ERROR; break;
    case CLIENT_STATE_CONNECTING:
    case CLIENT_STATE_AUTHENTICATING: color = COLOR_SYSTEM; break;
    default: color = COLOR_INFO; break;
    }

    ui_set_console_color(color);
    printf(" %s", status_text);
}

void ui_draw_input_line(chat_client_t* client, ui_state_t* ui_state) {
    if (!client || !ui_state) {
        return;
    }

    // 입력 라인 위치로 이동
    ui_set_cursor_position(0, ui_state->input_line);

    // 입력 라인 지우기
    ui_set_console_color(COLOR_RESET);
    for (int i = 0; i < ui_state->console_width; i++) {
        printf(" ");
    }

    // 프롬프트와 입력 내용 출력
    ui_set_cursor_position(0, ui_state->input_line);
    ui_set_console_color(COLOR_RESET);
    printf("%s%s", INPUT_PROMPT, ui_state->input_buffer);

    // 커서 위치 설정
    ui_set_cursor_position(strlen(INPUT_PROMPT) + ui_state->input_pos, ui_state->input_line);
}

void ui_show_help(chat_client_t* client) {
    if (!client || !g_ui_state) {
        return;
    }

    ui_add_system_message(g_ui_state, "=== Chat Client Commands ===", COLOR_SYSTEM);
    ui_add_system_message(g_ui_state, "/connect <host> <port> - Connect to server", COLOR_INFO);
    ui_add_system_message(g_ui_state, "/auth <username> - Authenticate with username", COLOR_INFO);
    ui_add_system_message(g_ui_state, "/disconnect - Disconnect from server", COLOR_INFO);
    ui_add_system_message(g_ui_state, "/users - Show online users", COLOR_INFO);
    ui_add_system_message(g_ui_state, "/status - Show client status", COLOR_INFO);
    ui_add_system_message(g_ui_state, "/clear - Clear chat history", COLOR_INFO);
    ui_add_system_message(g_ui_state, "/quit - Exit client", COLOR_INFO);
    ui_add_system_message(g_ui_state, "/help - Show this help", COLOR_INFO);
    ui_add_system_message(g_ui_state, "", COLOR_RESET);
    ui_add_system_message(g_ui_state, "Just type a message to chat (after authentication)", COLOR_INFO);

    g_ui_state->need_refresh = 1;
}

void ui_show_welcome_message(chat_client_t* client) {
    if (!client || !g_ui_state) {
        return;
    }

    ui_add_system_message(g_ui_state, "======================================", COLOR_SYSTEM);
    ui_add_system_message(g_ui_state, "    Welcome to Chat Client v1.0      ", COLOR_SYSTEM);
    ui_add_system_message(g_ui_state, "======================================", COLOR_SYSTEM);
    ui_add_system_message(g_ui_state, "", COLOR_RESET);
    ui_add_system_message(g_ui_state, "Type /help for available commands", COLOR_INFO);
    ui_add_system_message(g_ui_state, "Type /connect <host> <port> to start", COLOR_INFO);
    ui_add_system_message(g_ui_state, "", COLOR_RESET);

    g_ui_state->need_refresh = 1;
}

// =============================================================================
// 사용자 입력 처리
// =============================================================================

int ui_handle_input(chat_client_t* client, ui_state_t* ui_state) {
    if (!client || !ui_state) {
        return 0;
    }

    // 키 입력 확인 (논블로킹)
    if (!ui_kbhit()) {
        return 0;
    }

    int key = ui_getch();
    return ui_process_key(client, ui_state, key);
}

int ui_process_key(chat_client_t* client, ui_state_t* ui_state, int key) {
    if (!client || !ui_state) {
        return 0;
    }

    switch (key) {
    case 13:  // Enter
    {
        if (ui_state->input_pos > 0) {
            ui_state->input_buffer[ui_state->input_pos] = '\0';

            // 입력 처리
            int result = client_ui_handle_input(client, ui_state->input_buffer);

            // 입력 버퍼 초기화
            ui_state->input_buffer[0] = '\0';
            ui_state->input_pos = 0;
            ui_state->need_refresh = 1;

            return result;  // 종료 요청 시 1 반환
        }
    }
    break;

    case 8:   // Backspace
        if (ui_state->input_pos > 0) {
            ui_state->input_pos--;
            ui_state->input_buffer[ui_state->input_pos] = '\0';
            ui_state->need_refresh = 1;
        }
        break;

    case 27:  // ESC
        // 입력 취소
        ui_state->input_buffer[0] = '\0';
        ui_state->input_pos = 0;
        ui_state->need_refresh = 1;
        break;

    default:
        // 일반 문자 입력
        if (key >= 32 && key <= 126 && ui_state->input_pos < MAX_INPUT_LENGTH - 1) {
            ui_state->input_buffer[ui_state->input_pos] = (char)key;
            ui_state->input_pos++;
            ui_state->input_buffer[ui_state->input_pos] = '\0';
            ui_state->need_refresh = 1;
        }
        break;
    }

    return 0;
}

int client_ui_handle_input(chat_client_t* client, const char* input) {
    if (!client || utils_string_is_empty(input)) {
        return 0;
    }

    // 입력을 채팅 히스토리에 추가 (에코)
    if (g_ui_state && input[0] != '/') {
        char echo_message[512];
        sprintf_s(echo_message, sizeof(echo_message), "You: %s", input);
        ui_add_system_message(g_ui_state, echo_message, COLOR_CHAT);
    }

    // 명령어 처리
    if (input[0] == '/') {
        return ui_process_command(client, input + 1);  // '/' 제거
    }
    else {
        // 일반 채팅 메시지
        if (client_is_authenticated(client)) {
            return client_send_chat_message(client, input);
        }
        else {
            ui_add_system_message(g_ui_state, "Error: Not authenticated. Use /auth <username> first.", COLOR_ERROR);
            if (g_ui_state) g_ui_state->need_refresh = 1;
            return 0;
        }
    }
}

int ui_process_command(chat_client_t* client, const char* command) {
    if (!client || utils_string_is_empty(command)) {
        return 0;
    }

    // 명령어 파싱
    char cmd_buffer[256];
    utils_string_copy(cmd_buffer, sizeof(cmd_buffer), command);
    utils_string_trim(cmd_buffer);

    char* cmd = strtok(cmd_buffer, " ");
    if (!cmd) {
        return 0;
    }

    // 명령어 처리
    if (strcmp(cmd, "help") == 0 || strcmp(cmd, "h") == 0) {
        ui_show_help(client);
        return 0;
    }

    if (strcmp(cmd, "quit") == 0 || strcmp(cmd, "exit") == 0 || strcmp(cmd, "q") == 0) {
        ui_add_system_message(g_ui_state, "Goodbye!", COLOR_SYSTEM);
        if (g_ui_state) g_ui_state->need_refresh = 1;
        return 1;  // 종료 요청
    }

    if (strcmp(cmd, "connect") == 0) {
        char* host = strtok(NULL, " ");
        char* port_str = strtok(NULL, " ");

        if (!host || !port_str) {
            ui_add_system_message(g_ui_state, "Usage: /connect <host> <port>", COLOR_ERROR);
        }
        else {
            uint16_t port = (uint16_t)atoi(port_str);
            if (port > 0) {
                client_connect_to_server(client, host, port);
                ui_add_system_message(g_ui_state, "Connecting...", COLOR_SYSTEM);
            }
            else {
                ui_add_system_message(g_ui_state, "Invalid port number", COLOR_ERROR);
            }
        }
        if (g_ui_state) g_ui_state->need_refresh = 1;
        return 0;
    }

    if (strcmp(cmd, "auth") == 0 || strcmp(cmd, "login") == 0) {
        char* username = strtok(NULL, " ");

        if (!username) {
            ui_add_system_message(g_ui_state, "Usage: /auth <username>", COLOR_ERROR);
        }
        else if (!client_is_connected(client)) {
            ui_add_system_message(g_ui_state, "Error: Not connected to server", COLOR_ERROR);
        }
        else {
            client_authenticate(client, username);
            ui_add_system_message(g_ui_state, "Authenticating...", COLOR_SYSTEM);
        }
        if (g_ui_state) g_ui_state->need_refresh = 1;
        return 0;
    }

    if (strcmp(cmd, "disconnect") == 0) {
        client_disconnect(client);
        ui_add_system_message(g_ui_state, "Disconnecting...", COLOR_SYSTEM);
        if (g_ui_state) g_ui_state->need_refresh = 1;
        return 0;
    }

    if (strcmp(cmd, "users") == 0 || strcmp(cmd, "who") == 0) {
        if (!client_is_authenticated(client)) {
            ui_add_system_message(g_ui_state, "Error: Not authenticated", COLOR_ERROR);
        }
        else {
            client_request_user_list(client);
            ui_add_system_message(g_ui_state, "Requesting user list...", COLOR_SYSTEM);
        }
        if (g_ui_state) g_ui_state->need_refresh = 1;
        return 0;
    }

    if (strcmp(cmd, "status") == 0 || strcmp(cmd, "info") == 0) {
        client_print_status(client);
        ui_add_system_message(g_ui_state, "Status printed to console", COLOR_INFO);
        if (g_ui_state) g_ui_state->need_refresh = 1;
        return 0;
    }

    if (strcmp(cmd, "clear") == 0 || strcmp(cmd, "cls") == 0) {
        // 채팅 히스토리 지우기
        if (g_ui_state) {
            g_ui_state->history_count = 0;
            g_ui_state->need_refresh = 1;
        }
        ui_add_system_message(g_ui_state, "Chat history cleared", COLOR_SYSTEM);
        return 0;
    }

    // 알 수 없는 명령어
    char error_msg[256];
    sprintf_s(error_msg, sizeof(error_msg), "Unknown command: /%s (type /help for commands)", cmd);
    ui_add_system_message(g_ui_state, error_msg, COLOR_ERROR);
    if (g_ui_state) g_ui_state->need_refresh = 1;

    return 0;
}

// =============================================================================
// 이벤트 표시 함수들
// =============================================================================

void client_ui_display_event(chat_client_t* client, const network_event_t* event) {
    if (!client || !event || !g_ui_state) {
        return;
    }

    char timestamp_buf[32];
    const char* timestamp_str = ui_format_timestamp(event->timestamp, timestamp_buf, sizeof(timestamp_buf));

    switch (event->type) {
    case NET_EVENT_STATE_CHANGED:
    {
        char state_msg[256];
        sprintf_s(state_msg, sizeof(state_msg), "[%s] %s", timestamp_str, event->message);
        ui_add_system_message(g_ui_state, state_msg, COLOR_SYSTEM);
    }
    break;

    case NET_EVENT_CHAT_RECEIVED:
    {
        if (!utils_string_is_empty(event->username)) {
            ui_add_chat_message(g_ui_state, event->username, event->message,
                event->timestamp, COLOR_CHAT);
        }
        else {
            // 시스템 채팅 메시지
            char chat_msg[512];
            sprintf_s(chat_msg, sizeof(chat_msg), "[%s] %s", timestamp_str, event->message);
            ui_add_system_message(g_ui_state, chat_msg, COLOR_SYSTEM);
        }
    }
    break;

    case NET_EVENT_USER_LIST_RECEIVED:
    {
        char list_msg[512];
        sprintf_s(list_msg, sizeof(list_msg), "[%s] Online users: %s",
            timestamp_str, event->message);
        ui_add_system_message(g_ui_state, list_msg, COLOR_INFO);
    }
    break;

    case NET_EVENT_USER_JOINED:
    {
        char join_msg[256];
        sprintf_s(join_msg, sizeof(join_msg), "[%s] %s joined the chat",
            timestamp_str, event->username);
        ui_add_system_message(g_ui_state, join_msg, COLOR_SUCCESS);
    }
    break;

    case NET_EVENT_USER_LEFT:
    {
        char leave_msg[256];
        sprintf_s(leave_msg, sizeof(leave_msg), "[%s] %s left the chat",
            timestamp_str, event->username);
        ui_add_system_message(g_ui_state, leave_msg, COLOR_SYSTEM);
    }
    break;

    case NET_EVENT_ERROR_OCCURRED:
    {
        char error_msg[512];
        sprintf_s(error_msg, sizeof(error_msg), "[%s] Error: %s",
            timestamp_str, event->message);
        ui_add_system_message(g_ui_state, error_msg, COLOR_ERROR);
    }
    break;

    case NET_EVENT_CONNECTION_STATUS:
    {
        char status_msg[512];
        sprintf_s(status_msg, sizeof(status_msg), "[%s] %s",
            timestamp_str, event->message);

        int color = COLOR_INFO;
        if (event->new_state == CLIENT_STATE_AUTHENTICATED) {
            color = COLOR_SUCCESS;
        }
        else if (event->new_state == CLIENT_STATE_ERROR ||
            event->new_state == CLIENT_STATE_DISCONNECTED) {
            color = COLOR_ERROR;
        }

        ui_add_system_message(g_ui_state, status_msg, color);
    }
    break;

    default:
        LOG_DEBUG("Unknown event type: %d", event->type);
        break;
    }

    g_ui_state->need_refresh = 1;
}

void ui_add_chat_message(ui_state_t* ui_state, const char* username,
    const char* message, time_t timestamp, int color) {
    if (!ui_state || utils_string_is_empty(message)) {
        return;
    }

    char timestamp_buf[32];
    const char* timestamp_str = ui_format_timestamp(timestamp, timestamp_buf, sizeof(timestamp_buf));

    char formatted_message[512];
    if (username && !utils_string_is_empty(username)) {
        sprintf_s(formatted_message, sizeof(formatted_message),
            "[%s] <%s> %s", timestamp_str, username, message);
    }
    else {
        sprintf_s(formatted_message, sizeof(formatted_message),
            "[%s] %s", timestamp_str, message);
    }

    // 히스토리에 추가
    if (ui_state->history_count < CHAT_HISTORY_SIZE) {
        utils_string_copy(ui_state->chat_history[ui_state->history_count],
            sizeof(ui_state->chat_history[0]), formatted_message);
        ui_state->history_count++;
    }
    else {
        // 히스토리가 가득 찬 경우 오래된 것 제거
        for (int i = 0; i < CHAT_HISTORY_SIZE - 1; i++) {
            utils_string_copy(ui_state->chat_history[i],
                sizeof(ui_state->chat_history[0]),
                ui_state->chat_history[i + 1]);
        }
        utils_string_copy(ui_state->chat_history[CHAT_HISTORY_SIZE - 1],
            sizeof(ui_state->chat_history[0]), formatted_message);
    }

    ui_state->need_refresh = 1;
}

void ui_add_system_message(ui_state_t* ui_state, const char* message, int color) {
    if (!ui_state || utils_string_is_empty(message)) {
        return;
    }

    // 시스템 메시지는 타임스탬프 없이 추가
    if (ui_state->history_count < CHAT_HISTORY_SIZE) {
        utils_string_copy(ui_state->chat_history[ui_state->history_count],
            sizeof(ui_state->chat_history[0]), message);
        ui_state->history_count++;
    }
    else {
        // 히스토리가 가득 찬 경우 오래된 것 제거
        for (int i = 0; i < CHAT_HISTORY_SIZE - 1; i++) {
            utils_string_copy(ui_state->chat_history[i],
                sizeof(ui_state->chat_history[0]),
                ui_state->chat_history[i + 1]);
        }
        utils_string_copy(ui_state->chat_history[CHAT_HISTORY_SIZE - 1],
            sizeof(ui_state->chat_history[0]), message);
    }

    ui_state->need_refresh = 1;
}

// =============================================================================
// 유틸리티 함수들
// =============================================================================

void ui_set_console_color(int color) {
    if (g_console_handle != INVALID_HANDLE_VALUE) {
        SetConsoleTextAttribute(g_console_handle, (WORD)color);
    }
}

void ui_set_cursor_position(int x, int y) {
    if (g_console_handle != INVALID_HANDLE_VALUE) {
        COORD coord = { (SHORT)x, (SHORT)y };
        SetConsoleCursorPosition(g_console_handle, coord);
    }
}

void ui_clear_screen(void) {
    if (g_console_handle != INVALID_HANDLE_VALUE) {
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        DWORD chars_written;
        COORD home_coord = { 0, 0 };

        if (GetConsoleScreenBufferInfo(g_console_handle, &csbi)) {
            DWORD console_size = csbi.dwSize.X * csbi.dwSize.Y;

            FillConsoleOutputCharacter(g_console_handle, ' ', console_size,
                home_coord, &chars_written);
            FillConsoleOutputAttribute(g_console_handle, csbi.wAttributes, console_size,
                home_coord, &chars_written);
            SetConsoleCursorPosition(g_console_handle, home_coord);
        }
    }
}

void ui_get_console_size(int* width, int* height) {
    if (!width || !height) {
        return;
    }

    *width = 80;   // 기본값
    *height = 25;  // 기본값

    if (g_console_handle != INVALID_HANDLE_VALUE) {
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        if (GetConsoleScreenBufferInfo(g_console_handle, &csbi)) {
            *width = csbi.srWindow.Right - csbi.srWindow.Left + 1;
            *height = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
        }
    }
}

const char* ui_format_timestamp(time_t timestamp, char* buffer, size_t buffer_size) {
    if (!buffer || buffer_size < 10) {
        return "";
    }

    struct tm local_time;
    if (localtime_s(&local_time, &timestamp) == 0) {
        strftime(buffer, buffer_size, "%H:%M:%S", &local_time);
    }
    else {
        utils_string_copy(buffer, buffer_size, "??:??:??");
    }

    return buffer;
}

const char* ui_truncate_string(const char* str, int max_length,
    char* buffer, size_t buffer_size) {
    if (!str || !buffer || buffer_size == 0 || max_length <= 0) {
        return "";
    }

    int str_len = (int)strlen(str);
    if (str_len <= max_length) {
        utils_string_copy(buffer, buffer_size, str);
    }
    else {
        int copy_len = max_length - 3;  // "..." 위한 공간
        if (copy_len > 0) {
            strncpy_s(buffer, buffer_size, str, copy_len);
            strcat_s(buffer, buffer_size, "...");
        }
        else {
            utils_string_copy(buffer, buffer_size, "...");
        }
    }

    return buffer;
}

int ui_kbhit(void) {
    return _kbhit();
}

int ui_getch(void) {
    return _getch();
}

// 콜백에서 호출할 함수
int ui_process_user_input(chat_client_t* client, const char* input) {
    if (!client || !input || strlen(input) == 0) {
        return 0;
    }

    // 디버그 로그 추가
    LOG_INFO("Processing user input: %s", input);

    // 기존 client_ui_handle_input 함수 그대로 사용
    int result = client_ui_handle_input(client, input);

    // 강제 화면 새로고침 추가!
    if (g_ui_state) {
        g_ui_state->need_refresh = 1;
        ui_refresh_screen(client, g_ui_state);
    }

    return result;
}