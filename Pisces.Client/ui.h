#ifndef UI_H
#define UI_H

#include "client.h"
#include <stdio.h>
#include <conio.h>  // Windows �ܼ� �Է�

// =============================================================================
// UI ��� ����
// =============================================================================

#define MAX_INPUT_LENGTH        512         // �ִ� �Է� ����
#define UI_REFRESH_INTERVAL_MS  50          // UI ���ΰ�ħ ����
#define CHAT_HISTORY_SIZE       100         // ä�� �����丮 ũ��
#define INPUT_PROMPT           "> "         // �Է� ������Ʈ
#define COMMAND_PREFIX         "/"          // ��ɾ� ���λ�

// =============================================================================
// UI ���� �ڵ� (Windows �ܼ�)
// =============================================================================

#define COLOR_RESET            0x07         // �⺻ ���� (��� ����, ���� ���)
#define COLOR_SYSTEM           0x0E         // �ý��� �޽��� (�����)
#define COLOR_ERROR            0x0C         // ���� �޽��� (������)
#define COLOR_SUCCESS          0x0A         // ���� �޽��� (���)
#define COLOR_INFO             0x0B         // ���� �޽��� (û�ϻ�)
#define COLOR_CHAT             0x07         // �Ϲ� ä�� (���)
#define COLOR_USERNAME         0x0D         // ����ڸ� (��ȫ��)
#define COLOR_TIMESTAMP        0x08         // Ÿ�ӽ����� (ȸ��)

// =============================================================================
// UI ���� ����ü
// =============================================================================

typedef struct {
    char input_buffer[MAX_INPUT_LENGTH];    // ���� �Է� ����
    int input_pos;                          // �Է� Ŀ�� ��ġ
    int input_active;                       // �Է� Ȱ�� ����

    // ȭ�� ���̾ƿ�
    int console_width;                      // �ܼ� �ʺ�
    int console_height;                     // �ܼ� ����
    int chat_area_height;                   // ä�� ���� ����
    int status_line;                        // ���� ���� ��ġ
    int input_line;                         // �Է� ���� ��ġ

    // ä�� �����丮
    char chat_history[CHAT_HISTORY_SIZE][512];  // ä�� �����丮
    int history_count;                      // �����丮 ����
    int history_scroll;                     // ��ũ�� ��ġ

    // ���� ����
    time_t last_update;                     // ������ ������Ʈ �ð�
    int need_refresh;                       // ���ΰ�ħ �ʿ� ����
} ui_state_t;

// =============================================================================
// UI �ʱ�ȭ �� ���� �Լ���
// =============================================================================

/**
 * �ܼ� UI �ʱ�ȭ
 * @param client Ŭ���̾�Ʈ �ν��Ͻ�
 * @return ���� �� 0, ���� �� ����
 */
int client_ui_initialize(chat_client_t* client);

/**
 * �ܼ� UI ����
 * @param client Ŭ���̾�Ʈ �ν��Ͻ�
 */
void client_ui_cleanup(chat_client_t* client);

/**
 * UI ���� �ʱ�ȭ
 * @return ������ UI ����, ���� �� NULL
 */
ui_state_t* ui_state_create(void);

/**
 * UI ���� ����
 * @param ui_state ������ UI ����
 */
void ui_state_destroy(ui_state_t* ui_state);

// =============================================================================
// ȭ�� ��� �Լ���
// =============================================================================

/**
 * ��ü ȭ�� ���ΰ�ħ
 * @param client Ŭ���̾�Ʈ �ν��Ͻ�
 * @param ui_state UI ����
 */
void ui_refresh_screen(chat_client_t* client, ui_state_t* ui_state);

/**
 * ä�� ���� ���
 * @param client Ŭ���̾�Ʈ �ν��Ͻ�
 * @param ui_state UI ����
 */
void ui_draw_chat_area(chat_client_t* client, ui_state_t* ui_state);

/**
 * ���� ���� ���
 * @param client Ŭ���̾�Ʈ �ν��Ͻ�
 * @param ui_state UI ����
 */
void ui_draw_status_line(chat_client_t* client, ui_state_t* ui_state);

/**
 * �Է� ���� ���
 * @param client Ŭ���̾�Ʈ �ν��Ͻ�
 * @param ui_state UI ����
 */
void ui_draw_input_line(chat_client_t* client, ui_state_t* ui_state);

/**
 * ���� ���
 * @param client Ŭ���̾�Ʈ �ν��Ͻ�
 */
void ui_show_help(chat_client_t* client);

/**
 * ȯ�� �޽��� ���
 * @param client Ŭ���̾�Ʈ �ν��Ͻ�
 */
void ui_show_welcome_message(chat_client_t* client);

// =============================================================================
// ����� �Է� ó�� �Լ���
// =============================================================================

/**
 * ����� �Է� ó�� (���� �������� ȣ��)
 * @param client Ŭ���̾�Ʈ �ν��Ͻ�
 * @param ui_state UI ����
 * @return ��� �����ϸ� 0, ���� ��û �� 1
 */
int ui_handle_input(chat_client_t* client, ui_state_t* ui_state);

/**
 * Ű���� �Է� ó��
 * @param client Ŭ���̾�Ʈ �ν��Ͻ�
 * @param ui_state UI ����
 * @param key �Էµ� Ű
 * @return ó�� �Ϸ� �� 0
 */
int ui_process_key(chat_client_t* client, ui_state_t* ui_state, int key);

/**
 * ����� ó��
 * @param client Ŭ���̾�Ʈ �ν��Ͻ�
 * @param input �Է� ���ڿ�
 * @return ��� �����ϸ� 0, ���� ��û �� 1
 */
int client_ui_handle_input(chat_client_t* client, const char* input);

/**
 * ��ɾ� ó��
 * @param client Ŭ���̾�Ʈ �ν��Ͻ�
 * @param command ��ɾ� (/ ���λ� ����)
 * @return ���� �� 0, ���� �� ����
 */
int ui_process_command(chat_client_t* client, const char* command);

// =============================================================================
// �̺�Ʈ ǥ�� �Լ���
// =============================================================================

/**
 * ��Ʈ��ũ �̺�Ʈ ȭ�� ���
 * @param client Ŭ���̾�Ʈ �ν��Ͻ�
 * @param event ����� �̺�Ʈ
 */
void client_ui_display_event(chat_client_t* client, const network_event_t* event);

/**
 * ä�� �޽��� �߰�
 * @param ui_state UI ����
 * @param username ����ڸ� (NULL�̸� �ý��� �޽���)
 * @param message �޽��� ����
 * @param timestamp Ÿ�ӽ�����
 * @param color ���� �ڵ�
 */
void ui_add_chat_message(ui_state_t* ui_state, const char* username,
    const char* message, time_t timestamp, int color);

/**
 * �ý��� �޽��� �߰�
 * @param ui_state UI ����
 * @param message �ý��� �޽���
 * @param color ���� �ڵ�
 */
void ui_add_system_message(ui_state_t* ui_state, const char* message, int color);

// =============================================================================
// ��ƿ��Ƽ �Լ���
// =============================================================================

/**
 * �ܼ� ���� ����
 * @param color Windows �ܼ� ���� �ڵ�
 */
void ui_set_console_color(int color);

/**
 * �ܼ� Ŀ�� ��ġ ����
 * @param x X ��ǥ
 * @param y Y ��ǥ
 */
void ui_set_cursor_position(int x, int y);

/**
 * �ܼ� ȭ�� �����
 */
void ui_clear_screen(void);

/**
 * �ܼ� ũ�� ���
 * @param width �ʺ� (���)
 * @param height ���� (���)
 */
void ui_get_console_size(int* width, int* height);

/**
 * ���� �ð��� ���ڿ��� ����
 * @param timestamp Ÿ�ӽ�����
 * @param buffer ��� ����
 * @param buffer_size ���� ũ��
 * @return ���˵� �ð� ���ڿ�
 */
const char* ui_format_timestamp(time_t timestamp, char* buffer, size_t buffer_size);

/**
 * ���ڿ� ���� ���� (������ǥ �߰�)
 * @param str ���� ���ڿ�
 * @param max_length �ִ� ����
 * @param buffer ��� ����
 * @param buffer_size ���� ũ��
 * @return ���ѵ� ���ڿ�
 */
const char* ui_truncate_string(const char* str, int max_length,
    char* buffer, size_t buffer_size);

/**
 * Ű �Է� Ȯ�� (����ŷ)
 * @return Ű�� ���������� 1, �ƴϸ� 0
 */
int ui_kbhit(void);

/**
 * Ű �Է� �б�
 * @return �Էµ� Ű �ڵ�
 */
int ui_getch(void);

int ui_process_user_input(chat_client_t* client, const char* input);

// =============================================================================
// ���� UI ���� (ui.c���� ����)
// =============================================================================

extern ui_state_t* g_ui_state;

#endif // UI_H