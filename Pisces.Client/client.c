#include "client.h"
#include <stdlib.h>
#include <string.h>

// =============================================================================
// ���� �� ��ƿ��Ƽ �Լ��� - Part 1
// =============================================================================

client_config_t client_get_default_config(void) {
    client_config_t config;

    // ���� ���� ����
    utils_string_copy(config.server_host, sizeof(config.server_host), DEFAULT_SERVER_HOST);
    config.server_port = DEFAULT_SERVER_PORT;
    utils_string_copy(config.username, sizeof(config.username), "");

    // ���� ����
    config.auto_reconnect = 1;                  // �ڵ� �翬�� Ȱ��ȭ
    config.reconnect_interval = CLIENT_RECONNECT_INTERVAL;
    config.connect_timeout = CLIENT_CONNECT_TIMEOUT;
    config.heartbeat_timeout = CLIENT_HEARTBEAT_TIMEOUT;

    // UI ����
    config.show_timestamps = 1;                 // Ÿ�ӽ����� ǥ��
    config.show_system_messages = 1;            // �ý��� �޽��� ǥ��
    config.log_level = LOG_LEVEL_INFO;

    // ��� ����
    config.message_queue_size = MAX_MESSAGE_QUEUE_SIZE;
    config.network_buffer_size = NETWORK_BUFFER_SIZE;

    return config;
}

int client_validate_config(const client_config_t* config) {
    if (!config) {
        LOG_ERROR("Config is NULL");
        return 0;
    }

    // ���� ȣ��Ʈ Ȯ��
    if (utils_string_is_empty(config->server_host)) {
        LOG_ERROR("Server host cannot be empty");
        return 0;
    }

    // ��Ʈ ���� Ȯ��
    if (config->server_port == 0 || config->server_port > 65535) {
        LOG_ERROR("Invalid server port: %d", config->server_port);
        return 0;
    }

    // Ÿ�Ӿƿ� �� Ȯ��
    if (config->connect_timeout < 1 || config->connect_timeout > 60) {
        LOG_ERROR("Invalid connect timeout: %d (must be 1-60 seconds)", config->connect_timeout);
        return 0;
    }

    if (config->heartbeat_timeout < 5 || config->heartbeat_timeout > 300) {
        LOG_ERROR("Invalid heartbeat timeout: %d (must be 5-300 seconds)", config->heartbeat_timeout);
        return 0;
    }

    if (config->auto_reconnect &&
        (config->reconnect_interval < 1 || config->reconnect_interval > 60)) {
        LOG_ERROR("Invalid reconnect interval: %d (must be 1-60 seconds)",
            config->reconnect_interval);
        return 0;
    }

    // ť ũ�� Ȯ��
    if (config->message_queue_size < 10 || config->message_queue_size > 1000) {
        LOG_ERROR("Invalid message queue size: %d (must be 10-1000)",
            config->message_queue_size);
        return 0;
    }

    LOG_DEBUG("Client configuration validated successfully");
    return 1;
}

// =============================================================================
// Ŭ���̾�Ʈ ����������Ŭ �Լ��� - Part 1
// =============================================================================

chat_client_t* client_create(const client_config_t* config) {
    LOG_INFO("Creating client instance...");

    // ��Ʈ��ũ �ʱ�ȭ Ȯ��
    if (!network_is_initialized()) {
        if (network_initialize() != NETWORK_SUCCESS) {
            LOG_ERROR("Failed to initialize network");
            return NULL;
        }
    }

    // Ŭ���̾�Ʈ ����ü �Ҵ�
    chat_client_t* client = (chat_client_t*)calloc(1, sizeof(chat_client_t));
    if (!client) {
        LOG_ERROR("Failed to allocate memory for client");
        return NULL;
    }

    // ���� ���� �� ����
    if (config) {
        client->config = *config;
    }
    else {
        client->config = client_get_default_config();
        LOG_INFO("Using default client configuration");
    }

    if (!client_validate_config(&client->config)) {
        LOG_ERROR("Invalid client configuration");
        free(client);
        return NULL;
    }

    // �α� ���� ����
    utils_set_log_level(client->config.log_level);

    // Ŭ���̾�Ʈ ���� �ʱ�ȭ
    client->current_state = CLIENT_STATE_DISCONNECTED;
    client->server_socket = NULL;
    client->should_shutdown = 0;

    // ������ �ڵ� �ʱ�ȭ
    client->network_thread = NULL;
    client->ui_thread = GetCurrentThread();  // ���� ������

    // ����ȭ ��ü �ʱ�ȭ
    InitializeCriticalSection(&client->state_lock);

    client->network_ready_event = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (!client->network_ready_event) {
        LOG_ERROR("Failed to create network ready event");
        DeleteCriticalSection(&client->state_lock);
        free(client);
        return NULL;
    }

    client->shutdown_event = CreateEvent(NULL, TRUE, FALSE, NULL);  // Manual reset
    if (!client->shutdown_event) {
        LOG_ERROR("Failed to create shutdown event");
        CloseHandle(client->network_ready_event);
        DeleteCriticalSection(&client->state_lock);
        free(client);
        return NULL;
    }

    // ť ����
    client->command_queue = command_queue_create(client->config.message_queue_size);
    if (!client->command_queue) {
        LOG_ERROR("Failed to create command queue");
        CloseHandle(client->shutdown_event);
        CloseHandle(client->network_ready_event);
        DeleteCriticalSection(&client->state_lock);
        free(client);
        return NULL;
    }

    client->event_queue = event_queue_create(client->config.message_queue_size);
    if (!client->event_queue) {
        LOG_ERROR("Failed to create event queue");
        command_queue_destroy(client->command_queue);
        CloseHandle(client->shutdown_event);
        CloseHandle(client->network_ready_event);
        DeleteCriticalSection(&client->state_lock);
        free(client);
        return NULL;
    }

    // ���� ���� �ʱ�ȭ
    client->last_heartbeat = time(NULL);
    client->connection_start_time = 0;
    utils_string_copy(client->last_error, sizeof(client->last_error), "");

    // ��� �ʱ�ȭ
    memset(&client->stats, 0, sizeof(client_statistics_t));

    LOG_INFO("Client instance created successfully");
    LOG_INFO("Configuration: server=%s:%d, auto_reconnect=%s, queue_size=%d",
        client->config.server_host,
        client->config.server_port,
        client->config.auto_reconnect ? "enabled" : "disabled",
        client->config.message_queue_size);

    return client;
}

void client_destroy(chat_client_t* client) {
    if (!client) {
        return;
    }

    LOG_INFO("Destroying client instance...");

    // Ŭ���̾�Ʈ�� ���� ���� ���̸� ����
    if (client->current_state != CLIENT_STATE_DISCONNECTED) {
        LOG_WARNING("Client still active, forcing shutdown...");
        client_shutdown(client);
    }

    // ���� ���� ����
    if (client->server_socket) {
        network_socket_close(client->server_socket);
        network_socket_destroy(client->server_socket);
        client->server_socket = NULL;
    }

    // ť ����
    if (client->command_queue) {
        command_queue_destroy(client->command_queue);
        client->command_queue = NULL;
    }

    if (client->event_queue) {
        event_queue_destroy(client->event_queue);
        client->event_queue = NULL;
    }

    // ����ȭ ��ü ����
    if (client->network_ready_event) {
        CloseHandle(client->network_ready_event);
        client->network_ready_event = NULL;
    }

    if (client->shutdown_event) {
        CloseHandle(client->shutdown_event);
        client->shutdown_event = NULL;
    }

    DeleteCriticalSection(&client->state_lock);

    // Ŭ���̾�Ʈ ����ü ����
    free(client);

    LOG_INFO("Client instance destroyed");
}

// =============================================================================
// ���� ���� �Լ���
// =============================================================================

int client_change_state(chat_client_t* client, client_state_t new_state, client_event_t event) {
    if (!client) {
        return -1;
    }

    EnterCriticalSection(&client->state_lock);

    client_state_t old_state = client->current_state;

    // ���� ��ȯ ��ȿ�� ���� (������ ����)
    int valid_transition = 1;

    switch (old_state) {
    case CLIENT_STATE_DISCONNECTED:
        if (new_state != CLIENT_STATE_CONNECTING && new_state != CLIENT_STATE_RECONNECTING) {
            valid_transition = 0;
        }
        break;

    case CLIENT_STATE_CONNECTING:
        if (new_state != CLIENT_STATE_CONNECTED &&
            new_state != CLIENT_STATE_DISCONNECTED &&
            new_state != CLIENT_STATE_ERROR) {
            valid_transition = 0;
        }
        break;

    case CLIENT_STATE_CONNECTED:
        if (new_state != CLIENT_STATE_AUTHENTICATING &&
            new_state != CLIENT_STATE_DISCONNECTING &&
            new_state != CLIENT_STATE_ERROR) {
            valid_transition = 0;
        }
        break;

    case CLIENT_STATE_AUTHENTICATING:
        if (new_state != CLIENT_STATE_AUTHENTICATED &&
            new_state != CLIENT_STATE_DISCONNECTING &&
            new_state != CLIENT_STATE_ERROR) {
            valid_transition = 0;
        }
        break;

    case CLIENT_STATE_AUTHENTICATED:
        if (new_state != CLIENT_STATE_DISCONNECTING &&
            new_state != CLIENT_STATE_RECONNECTING &&
            new_state != CLIENT_STATE_ERROR) {
            valid_transition = 0;
        }
        break;

    default:
        // �ٸ� ���µ��� ��κ��� ��ȯ ���
        break;
    }

    if (!valid_transition) {
        LOG_WARNING("Invalid state transition: %s -> %s (event: %s)",
            client_state_to_string(old_state),
            client_state_to_string(new_state),
            client_event_to_string(event));
        LeaveCriticalSection(&client->state_lock);
        return -1;
    }

    // ���� ����
    client->current_state = new_state;

    LeaveCriticalSection(&client->state_lock);

    LOG_DEBUG("State changed: %s -> %s (event: %s)",
        client_state_to_string(old_state),
        client_state_to_string(new_state),
        client_event_to_string(event));

    // UI �����忡 ���� ���� �˸�
    network_event_t state_event = { 0 };
    state_event.type = NET_EVENT_STATE_CHANGED;
    state_event.new_state = new_state;
    state_event.timestamp = time(NULL);
    sprintf_s(state_event.message, sizeof(state_event.message),
        "State changed from %s to %s",
        client_state_to_string(old_state),
        client_state_to_string(new_state));

    event_queue_push(client->event_queue, &state_event);

    return 0;
}

client_state_t client_get_current_state(chat_client_t* client) {
    if (!client) {
        return CLIENT_STATE_ERROR;
    }

    EnterCriticalSection(&client->state_lock);
    client_state_t state = client->current_state;
    LeaveCriticalSection(&client->state_lock);

    return state;
}

const char* client_state_to_string(client_state_t state) {
    switch (state) {
    case CLIENT_STATE_DISCONNECTED:    return "DISCONNECTED";
    case CLIENT_STATE_CONNECTING:      return "CONNECTING";
    case CLIENT_STATE_CONNECTED:       return "CONNECTED";
    case CLIENT_STATE_AUTHENTICATING:  return "AUTHENTICATING";
    case CLIENT_STATE_AUTHENTICATED:   return "AUTHENTICATED";
    case CLIENT_STATE_RECONNECTING:    return "RECONNECTING";
    case CLIENT_STATE_DISCONNECTING:   return "DISCONNECTING";
    case CLIENT_STATE_ERROR:           return "ERROR";
    default:                           return "UNKNOWN";
    }
}

const char* client_event_to_string(client_event_t event) {
    switch (event) {
    case CLIENT_EVENT_CONNECT_REQUESTED:    return "CONNECT_REQUESTED";
    case CLIENT_EVENT_CONNECTED:            return "CONNECTED";
    case CLIENT_EVENT_CONNECTION_FAILED:    return "CONNECTION_FAILED";
    case CLIENT_EVENT_AUTH_REQUESTED:       return "AUTH_REQUESTED";
    case CLIENT_EVENT_AUTH_SUCCESS:         return "AUTH_SUCCESS";
    case CLIENT_EVENT_AUTH_FAILED:          return "AUTH_FAILED";
    case CLIENT_EVENT_DISCONNECT_REQUESTED: return "DISCONNECT_REQUESTED";
    case CLIENT_EVENT_CONNECTION_LOST:      return "CONNECTION_LOST";
    case CLIENT_EVENT_ERROR_OCCURRED:       return "ERROR_OCCURRED";
    case CLIENT_EVENT_RECONNECT_REQUESTED:  return "RECONNECT_REQUESTED";
    default:                                return "UNKNOWN";
    }
}

// =============================================================================
// ���� ��ȸ �� ��ƿ��Ƽ �Լ���
// =============================================================================

int client_is_connected(const chat_client_t* client) {
    if (!client) {
        return 0;
    }

    client_state_t state = client_get_current_state((chat_client_t*)client);
    return (state == CLIENT_STATE_CONNECTED ||
        state == CLIENT_STATE_AUTHENTICATING ||
        state == CLIENT_STATE_AUTHENTICATED) ? 1 : 0;
}

int client_is_authenticated(const chat_client_t* client) {
    if (!client) {
        return 0;
    }

    client_state_t state = client_get_current_state((chat_client_t*)client);
    return (state == CLIENT_STATE_AUTHENTICATED) ? 1 : 0;
}

void client_set_last_error(chat_client_t* client, const char* error_message) {
    if (!client || !error_message) {
        return;
    }

    EnterCriticalSection(&client->state_lock);
    utils_string_copy(client->last_error, sizeof(client->last_error), error_message);
    LeaveCriticalSection(&client->state_lock);

    LOG_ERROR("Client error: %s", error_message);
}

const char* client_get_last_error(chat_client_t* client, char* buffer, size_t buffer_size) {
    if (!client || !buffer || buffer_size == 0) {
        return NULL;
    }

    EnterCriticalSection(&client->state_lock);
    utils_string_copy(buffer, buffer_size, client->last_error);
    LeaveCriticalSection(&client->state_lock);

    return buffer;
}

// =============================================================================
// ���� ��� �Լ���
// =============================================================================

void client_print_status(const chat_client_t* client) {
    if (!client) {
        printf("Client: NULL\n");
        return;
    }

    client_state_t state = client_get_current_state((chat_client_t*)client);

    printf("=== Client Status ===\n");
    printf("State: %s\n", client_state_to_string(state));
    printf("Server: %s:%d\n", client->config.server_host, client->config.server_port);
    printf("Username: %s\n", utils_string_is_empty(client->config.username) ?
        "[Not set]" : client->config.username);
    printf("Auto reconnect: %s\n", client->config.auto_reconnect ? "Enabled" : "Disabled");

    if (client->connection_start_time > 0) {
        time_t uptime = time(NULL) - client->connection_start_time;
        printf("Connection uptime: %d seconds\n", (int)uptime);
    }

    char error_buffer[256];
    const char* last_error = client_get_last_error((chat_client_t*)client,
        error_buffer, sizeof(error_buffer));
    if (last_error && !utils_string_is_empty(last_error)) {
        printf("Last error: %s\n", last_error);
    }
}

void client_print_statistics(const chat_client_t* client) {
    if (!client) {
        printf("Client statistics: NULL\n");
        return;
    }

    const client_statistics_t* stats = &client->stats;

    printf("=== Client Statistics ===\n");
    printf("Messages sent: %u\n", stats->messages_sent);
    printf("Messages received: %u\n", stats->messages_received);

    char bytes_sent_str[32], bytes_received_str[32];
    utils_bytes_to_human_readable(stats->bytes_sent, bytes_sent_str, sizeof(bytes_sent_str));
    utils_bytes_to_human_readable(stats->bytes_received, bytes_received_str, sizeof(bytes_received_str));

    printf("Bytes sent: %s\n", bytes_sent_str);
    printf("Bytes received: %s\n", bytes_received_str);
    printf("Reconnect count: %u\n", stats->reconnect_count);
    printf("Connection failures: %u\n", stats->connection_failures);

    if (stats->connected_at > 0) {
        char connected_time[TIME_STRING_SIZE];
        utils_time_to_string(stats->connected_at, connected_time, sizeof(connected_time));
        printf("Connected at: %s\n", connected_time);

        time_t session_duration = time(NULL) - stats->connected_at;
        printf("Session duration: %d seconds\n", (int)session_duration);
    }
}

// client.c Part 2 - ������ ���� �� ť �ý���
// Part 1�� �̾����� ����

// =============================================================================
// Ŭ���̾�Ʈ ����������Ŭ �Լ��� - Part 2
// =============================================================================

int client_start(chat_client_t* client) {
    if (!client) {
        LOG_ERROR("Client is NULL");
        return -1;
    }

    client_state_t current_state = client_get_current_state(client);
    if (current_state != CLIENT_STATE_DISCONNECTED) {
        LOG_ERROR("Client is not in disconnected state (current: %s)",
            client_state_to_string(current_state));
        return -1;
    }

    LOG_INFO("Starting client threads...");

    // ���� �̺�Ʈ ����
    ResetEvent(client->shutdown_event);
    client->should_shutdown = 0;

    // ��Ʈ��ũ ������ ����
    client->network_thread = CreateThread(
        NULL,                           // �⺻ ���� ����
        0,                              // �⺻ ���� ũ��
        client_network_thread_proc,     // ������ �Լ�
        client,                         // �Ű�����
        0,                              // ��� ����
        NULL                            // ������ ID ���ʿ�
    );

    if (!client->network_thread) {
        LOG_ERROR("Failed to create network thread: %s",
            utils_get_last_error_string(GetLastError(),
                client->last_error,
                sizeof(client->last_error)));
        return -1;
    }

    // ��Ʈ��ũ �����尡 �غ�� ������ ��� (�ִ� 5��)
    DWORD wait_result = WaitForSingleObject(client->network_ready_event, 5000);
    if (wait_result != WAIT_OBJECT_0) {
        LOG_ERROR("Network thread failed to start within timeout");
        client_shutdown(client);
        return -1;
    }

    LOG_INFO("Client threads started successfully");
    return 0;
}

int client_shutdown(chat_client_t* client) {
    if (!client) {
        return -1;
    }

    LOG_INFO("Shutting down client...");

    // ���� ��ȣ ����
    client->should_shutdown = 1;
    SetEvent(client->shutdown_event);

    // ��Ʈ��ũ ������ ���� ��� (�ִ� 10��)
    if (client->network_thread) {
        DWORD wait_result = WaitForSingleObject(client->network_thread, 10000);

        if (wait_result == WAIT_TIMEOUT) {
            LOG_WARNING("Network thread did not terminate gracefully within timeout");

            // TerminateThread ��� �� ������ ���� ��ȣ ����
            // �߰� ���� �õ�
            for (int retry = 0; retry < 3 && wait_result == WAIT_TIMEOUT; retry++) {
                LOG_WARNING("Attempting forced shutdown, retry %d/3", retry + 1);

                // ������ ������ �ݾƼ� ���ŷ�� ��Ʈ��ũ �۾� ����
                if (client->server_socket) {
                    network_socket_close(client->server_socket);
                }

                // �߰� ���
                wait_result = WaitForSingleObject(client->network_thread, 2000);
            }

            if (wait_result == WAIT_TIMEOUT) {
                LOG_ERROR("Network thread failed to terminate gracefully");
                // TerminateThread�� �����ϹǷ� ������� �ʰ� �ڵ鸸 ����
                // ���μ��� ���� �� �ý����� �����带 ������
            }
        }

        CloseHandle(client->network_thread);
        client->network_thread = NULL;
    }

    // ���� ���� ����
    if (client->server_socket) {
        network_socket_close(client->server_socket);
        network_socket_destroy(client->server_socket);
        client->server_socket = NULL;
    }

    // ���� �ʱ�ȭ
    client_change_state(client, CLIENT_STATE_DISCONNECTED, CLIENT_EVENT_DISCONNECT_REQUESTED);

    LOG_INFO("Client shutdown completed");
    return 0;
}

int client_run(chat_client_t* client) {
    if (!client) {
        LOG_ERROR("Client is NULL");
        return -1;
    }

    LOG_INFO("Client main loop started");

    // UI �ʱ�ȭ
    if (client_ui_initialize(client) != 0) {
        LOG_ERROR("Failed to initialize UI");
        return -1;
    }

    // ���� ����
    while (!client->should_shutdown) {
        // ��Ʈ��ũ �̺�Ʈ ó��
        network_event_t event;
        int event_result = event_queue_pop(client->event_queue, &event, 100);  // 100ms Ÿ�Ӿƿ�

        if (event_result == 0) {
            // �̺�Ʈ�� ���� - UI�� ǥ��
            client_ui_display_event(client, &event);
        }
        else if (event_result == -2) {
            // Ÿ�Ӿƿ� - �������� ��Ȳ, ����� �Է� üũ
        }
        else {
            // ���� �Ǵ� ť �������
            Sleep(10);  // ª�� ���
        }

        // ����� �Է� ó�� (����ŷ)
        // ���� ������ ui.c���� ó��

        // �ֱ����� ���� üũ (1�ʸ���)
        static time_t last_status_check = 0;
        time_t current_time = time(NULL);

        if (current_time - last_status_check >= 1) {
            // ���� ���� üũ
            if (client_is_connected(client)) {
                // ��Ʈ��Ʈ Ÿ�Ӿƿ� üũ
                if (current_time - client->last_heartbeat > client->config.heartbeat_timeout) {
                    LOG_WARNING("Heartbeat timeout detected");
                    client_set_last_error(client, "Heartbeat timeout");

                    // ���� �������� ó��
                    ui_command_t disconnect_cmd = { 0 };
                    disconnect_cmd.type = UI_CMD_DISCONNECT;
                    command_queue_push(client->command_queue, &disconnect_cmd);
                }
            }

            last_status_check = current_time;
        }

        // CPU ���� ����
        Sleep(10);
    }

    // UI ����
    client_ui_cleanup(client);

    LOG_INFO("Client main loop exiting");
    return 0;
}

// =============================================================================
// ť ���� �Լ��� - Command Queue
// =============================================================================

command_queue_t* command_queue_create(int capacity) {
    if (capacity <= 0) {
        LOG_ERROR("Invalid queue capacity: %d", capacity);
        return NULL;
    }

    command_queue_t* queue = (command_queue_t*)calloc(1, sizeof(command_queue_t));
    if (!queue) {
        LOG_ERROR("Failed to allocate memory for command queue");
        return NULL;
    }

    queue->commands = (ui_command_t*)calloc(capacity, sizeof(ui_command_t));
    if (!queue->commands) {
        LOG_ERROR("Failed to allocate memory for command queue array");
        free(queue);
        return NULL;
    }

    queue->capacity = capacity;
    queue->size = 0;
    queue->front = 0;
    queue->rear = 0;

    InitializeCriticalSection(&queue->lock);

    LOG_DEBUG("Created command queue with capacity %d", capacity);
    return queue;
}

void command_queue_destroy(command_queue_t* queue) {
    if (!queue) {
        return;
    }

    DeleteCriticalSection(&queue->lock);

    if (queue->commands) {
        free(queue->commands);
    }

    free(queue);
    LOG_DEBUG("Command queue destroyed");
}

int command_queue_push(command_queue_t* queue, const ui_command_t* command) {
    if (!queue || !command) {
        return -1;
    }

    EnterCriticalSection(&queue->lock);

    // ť�� ���� ���� Ȯ��
    if (queue->size >= queue->capacity) {
        LeaveCriticalSection(&queue->lock);
        LOG_DEBUG("Command queue is full");
        return -1;  // ť ������
    }

    // ��� �߰�
    queue->commands[queue->rear] = *command;
    queue->rear = (queue->rear + 1) % queue->capacity;
    queue->size++;

    LeaveCriticalSection(&queue->lock);

    LOG_DEBUG("Command pushed to queue (type: %d, size: %d)", command->type, queue->size);
    return 0;
}

int command_queue_pop(command_queue_t* queue, ui_command_t* command, int timeout_ms) {
    if (!queue || !command) {
        return -1;
    }

    time_t start_time = time(NULL);
    time_t end_time = start_time + (timeout_ms / 1000);

    while (time(NULL) <= end_time || timeout_ms == 0) {
        EnterCriticalSection(&queue->lock);

        if (queue->size > 0) {
            // ��� ��������
            *command = queue->commands[queue->front];
            queue->front = (queue->front + 1) % queue->capacity;
            queue->size--;

            LeaveCriticalSection(&queue->lock);

            LOG_DEBUG("Command popped from queue (type: %d, size: %d)", command->type, queue->size);
            return 0;
        }

        LeaveCriticalSection(&queue->lock);

        if (timeout_ms == 0) {
            break;  // ��� ��ȯ
        }

        Sleep(10);  // ª�� ���
    }

    return (timeout_ms == 0) ? -1 : -2;  // ť ������� �Ǵ� Ÿ�Ӿƿ�
}

// =============================================================================
// ť ���� �Լ��� - Event Queue
// =============================================================================

event_queue_t* event_queue_create(int capacity) {
    if (capacity <= 0) {
        LOG_ERROR("Invalid queue capacity: %d", capacity);
        return NULL;
    }

    event_queue_t* queue = (event_queue_t*)calloc(1, sizeof(event_queue_t));
    if (!queue) {
        LOG_ERROR("Failed to allocate memory for event queue");
        return NULL;
    }

    queue->events = (network_event_t*)calloc(capacity, sizeof(network_event_t));
    if (!queue->events) {
        LOG_ERROR("Failed to allocate memory for event queue array");
        free(queue);
        return NULL;
    }

    queue->capacity = capacity;
    queue->size = 0;
    queue->front = 0;
    queue->rear = 0;

    InitializeCriticalSection(&queue->lock);

    LOG_DEBUG("Created event queue with capacity %d", capacity);
    return queue;
}

void event_queue_destroy(event_queue_t* queue) {
    if (!queue) {
        return;
    }

    DeleteCriticalSection(&queue->lock);

    if (queue->events) {
        free(queue->events);
    }

    free(queue);
    LOG_DEBUG("Event queue destroyed");
}

int event_queue_push(event_queue_t* queue, const network_event_t* event) {
    if (!queue || !event) {
        return -1;
    }

    EnterCriticalSection(&queue->lock);

    // ť�� ���� ���� Ȯ��
    if (queue->size >= queue->capacity) {
        // ť�� ���� ���� ���� ������ �̺�Ʈ ���� (���� �����)
        LOG_DEBUG("Event queue full, overwriting oldest event");
        queue->front = (queue->front + 1) % queue->capacity;
    }
    else {
        queue->size++;
    }

    // �̺�Ʈ �߰�
    queue->events[queue->rear] = *event;
    queue->rear = (queue->rear + 1) % queue->capacity;

    LeaveCriticalSection(&queue->lock);

    LOG_DEBUG("Event pushed to queue (type: %d, size: %d)", event->type, queue->size);
    return 0;
}

int event_queue_pop(event_queue_t* queue, network_event_t* event, int timeout_ms) {
    if (!queue || !event) {
        return -1;
    }

    time_t start_time = time(NULL);
    time_t end_time = start_time + (timeout_ms / 1000);

    while (time(NULL) <= end_time || timeout_ms == 0) {
        EnterCriticalSection(&queue->lock);

        if (queue->size > 0) {
            // �̺�Ʈ ��������
            *event = queue->events[queue->front];
            queue->front = (queue->front + 1) % queue->capacity;
            queue->size--;

            LeaveCriticalSection(&queue->lock);

            LOG_DEBUG("Event popped from queue (type: %d, size: %d)", event->type, queue->size);
            return 0;
        }

        LeaveCriticalSection(&queue->lock);

        if (timeout_ms == 0) {
            break;  // ��� ��ȯ
        }

        Sleep(10);  // ª�� ���
    }

    return (timeout_ms == 0) ? -1 : -2;  // ť ������� �Ǵ� Ÿ�Ӿƿ�
}

// =============================================================================
// ���� �Լ���
// =============================================================================

static int client_is_shutdown_requested(const chat_client_t* client) {
    return client->should_shutdown ||
        WaitForSingleObject(client->shutdown_event, 0) == WAIT_OBJECT_0;
}

static void client_update_statistics(chat_client_t* client, int messages_sent, int messages_received) {
    if (!client) {
        return;
    }

    client->stats.messages_sent += messages_sent;
    client->stats.messages_received += messages_received;

    if (client->server_socket) {
        client->stats.bytes_sent = client->server_socket->bytes_sent;
        client->stats.bytes_received = client->server_socket->bytes_received;
    }

    client->stats.last_activity = time(NULL);
}

// client.c Part 3 - ���� �� �޽��� ó��
// Part 1, Part 2�� �̾����� ����

// =============================================================================
// ���� �� ���� �Լ��� - Part 3
// =============================================================================

int client_connect_to_server(chat_client_t* client, const char* host, uint16_t port) {
    if (!client || utils_string_is_empty(host) || port == 0) {
        LOG_ERROR("Invalid parameters for client_connect_to_server");
        return -1;
    }

    client_state_t current_state = client_get_current_state(client);
    if (current_state != CLIENT_STATE_DISCONNECTED) {
        LOG_ERROR("Client is not in disconnected state (current: %s)",
            client_state_to_string(current_state));
        return -1;
    }

    LOG_INFO("Connecting to server %s:%d...", host, port);

    // ���� ����
    client_change_state(client, CLIENT_STATE_CONNECTING, CLIENT_EVENT_CONNECT_REQUESTED);

    // ���� ���� �ð� ���
    client->connection_start_time = time(NULL);

    // ���� ���� ������Ʈ
    utils_string_copy(client->config.server_host, sizeof(client->config.server_host), host);
    client->config.server_port = port;

    // ���� ����� ��Ʈ��ũ �����忡 ����
    ui_command_t connect_cmd = { 0 };
    connect_cmd.type = UI_CMD_CONNECT;
    sprintf_s(connect_cmd.data, sizeof(connect_cmd.data), "%s:%d", host, port);

    if (command_queue_push(client->command_queue, &connect_cmd) != 0) {
        LOG_ERROR("Failed to queue connect command");
        client_change_state(client, CLIENT_STATE_DISCONNECTED, CLIENT_EVENT_CONNECTION_FAILED);
        return -1;
    }

    return 0;  // �񵿱� ����, ����� �̺�Ʈ�� ���޵�
}

int client_authenticate(chat_client_t* client, const char* username) {
    if (!client || utils_string_is_empty(username)) {
        LOG_ERROR("Invalid parameters for client_authenticate");
        return -1;
    }

    client_state_t current_state = client_get_current_state(client);
    if (current_state != CLIENT_STATE_CONNECTED) {
        LOG_ERROR("Client is not connected (current: %s)",
            client_state_to_string(current_state));
        return -1;
    }

    // ����ڸ� ��ȿ�� ����
    if (strlen(username) >= MAX_USERNAME_LENGTH) {
        LOG_ERROR("Username too long: %zu characters (max: %d)",
            strlen(username), MAX_USERNAME_LENGTH - 1);
        client_set_last_error(client, "Username too long");
        return -1;
    }

    LOG_INFO("Authenticating as '%s'...", username);

    // ���� ����
    client_change_state(client, CLIENT_STATE_AUTHENTICATING, CLIENT_EVENT_AUTH_REQUESTED);

    // ����ڸ� ����
    utils_string_copy(client->config.username, sizeof(client->config.username), username);

    // ���� ����� ��Ʈ��ũ �����忡 ���� (���� ��Ʈ��ũ ó���� network_thread����)
    // ���⼭�� UI ��ɸ� ť�� �߰�
    ui_command_t auth_cmd = { 0 };
    auth_cmd.type = UI_CMD_CONNECT;  // ���� ������ �Ϻη� ó��
    utils_string_copy(auth_cmd.data, sizeof(auth_cmd.data), username);

    if (command_queue_push(client->command_queue, &auth_cmd) != 0) {
        LOG_ERROR("Failed to queue authentication command");
        client_change_state(client, CLIENT_STATE_CONNECTED, CLIENT_EVENT_AUTH_FAILED);
        return -1;
    }

    return 0;  // �񵿱� ����, ����� �̺�Ʈ�� ���޵�
}

int client_disconnect(chat_client_t* client) {
    if (!client) {
        return -1;
    }

    client_state_t current_state = client_get_current_state(client);

    // �̹� ���� ���� ���̰ų� ������ ���¸� ����
    if (current_state == CLIENT_STATE_DISCONNECTED ||
        current_state == CLIENT_STATE_DISCONNECTING) {
        return 0;
    }

    LOG_INFO("Disconnecting from server...");

    // ���� ����
    client_change_state(client, CLIENT_STATE_DISCONNECTING, CLIENT_EVENT_DISCONNECT_REQUESTED);

    // ���� ���� ����� ��Ʈ��ũ �����忡 ����
    ui_command_t disconnect_cmd = { 0 };
    disconnect_cmd.type = UI_CMD_DISCONNECT;

    if (command_queue_push(client->command_queue, &disconnect_cmd) != 0) {
        LOG_WARNING("Failed to queue disconnect command, forcing local disconnect");

        // ��� ť ���� �� ���� ���� ����
        if (client->server_socket) {
            network_socket_close(client->server_socket);
            network_socket_destroy(client->server_socket);
            client->server_socket = NULL;
        }

        client_change_state(client, CLIENT_STATE_DISCONNECTED, CLIENT_EVENT_CONNECTION_LOST);
    }

    return 0;
}

// =============================================================================
// �޽��� �ۼ��� �Լ���
// =============================================================================

int client_send_chat_message(chat_client_t* client, const char* message) {
    if (!client || utils_string_is_empty(message)) {
        LOG_ERROR("Invalid parameters for client_send_chat_message");
        return -1;
    }

    // ���� ���� Ȯ��
    if (!client_is_authenticated(client)) {
        LOG_ERROR("Client is not authenticated");
        client_set_last_error(client, "Not authenticated");
        return -1;
    }

    // �޽��� ���� Ȯ��
    if (strlen(message) >= MAX_CHAT_MESSAGE_LENGTH) {
        LOG_ERROR("Message too long: %zu characters (max: %d)",
            strlen(message), MAX_CHAT_MESSAGE_LENGTH - 1);
        client_set_last_error(client, "Message too long");
        return -1;
    }

    // �� �޽��� Ȯ��
    char trimmed_message[MAX_CHAT_MESSAGE_LENGTH];
    utils_string_copy(trimmed_message, sizeof(trimmed_message), message);
    utils_string_trim(trimmed_message);

    if (utils_string_is_empty(trimmed_message)) {
        LOG_DEBUG("Empty message after trimming, ignoring");
        return 0;
    }

    LOG_DEBUG("Sending chat message: %s", trimmed_message);

    // ä�� ���� ����� ��Ʈ��ũ �����忡 ����
    ui_command_t chat_cmd = { 0 };
    chat_cmd.type = UI_CMD_SEND_CHAT;
    utils_string_copy(chat_cmd.data, sizeof(chat_cmd.data), trimmed_message);

    if (command_queue_push(client->command_queue, &chat_cmd) != 0) {
        LOG_ERROR("Failed to queue chat message");
        client_set_last_error(client, "Message queue full");
        return -1;
    }

    return 0;
}

int client_request_user_list(chat_client_t* client) {
    if (!client) {
        return -1;
    }

    // ���� ���� Ȯ��
    if (!client_is_authenticated(client)) {
        LOG_ERROR("Client is not authenticated");
        client_set_last_error(client, "Not authenticated");
        return -1;
    }

    LOG_DEBUG("Requesting user list");

    // ����� ��� ��û ����� ��Ʈ��ũ �����忡 ����
    ui_command_t userlist_cmd = { 0 };
    userlist_cmd.type = UI_CMD_REQUEST_USER_LIST;

    if (command_queue_push(client->command_queue, &userlist_cmd) != 0) {
        LOG_ERROR("Failed to queue user list request");
        client_set_last_error(client, "Command queue full");
        return -1;
    }

    return 0;
}

int client_send_heartbeat_ack(chat_client_t* client) {
    if (!client) {
        return -1;
    }

    // ���� ���� Ȯ��
    if (!client_is_connected(client)) {
        LOG_DEBUG("Client not connected, skipping heartbeat ACK");
        return -1;
    }

    LOG_DEBUG("Sending heartbeat ACK");

    // ��Ʈ��Ʈ �ð� ������Ʈ
    client->last_heartbeat = time(NULL);

    // ���� ��Ʈ��Ʈ ACK ������ network_thread���� ���� ó��
    // ���⼭�� �ð��� ������Ʈ

    return 0;
}

// =============================================================================
// ���� ���� �Լ��� (static)
// =============================================================================

static int client_handle_connection_result(chat_client_t* client, int success, const char* error_message) {
    if (!client) {
        return -1;
    }

    if (success) {
        client_change_state(client, CLIENT_STATE_CONNECTED, CLIENT_EVENT_CONNECTED);
        LOG_INFO("Successfully connected to %s:%d",
            client->config.server_host, client->config.server_port);

        // ��� ������Ʈ
        client->stats.connected_at = time(NULL);

        return 0;
    }
    else {
        client_change_state(client, CLIENT_STATE_DISCONNECTED, CLIENT_EVENT_CONNECTION_FAILED);
        client->stats.connection_failures++;

        if (error_message) {
            client_set_last_error(client, error_message);
        }

        LOG_ERROR("Failed to connect to %s:%d: %s",
            client->config.server_host, client->config.server_port,
            error_message ? error_message : "Unknown error");

        // �ڵ� �翬�� �õ�
        if (client->config.auto_reconnect) {
            LOG_INFO("Auto-reconnect enabled, will retry in %d seconds",
                client->config.reconnect_interval);

            // �翬�� Ÿ�̸� ���� (���� ������ network_thread����)
            client->stats.reconnect_count++;
        }

        return -1;
    }
}

static int client_handle_authentication_result(chat_client_t* client, int success, const char* message) {
    if (!client) {
        return -1;
    }

    if (success) {
        client_change_state(client, CLIENT_STATE_AUTHENTICATED, CLIENT_EVENT_AUTH_SUCCESS);
        LOG_INFO("Successfully authenticated as '%s'", client->config.username);

        // ��Ʈ��Ʈ ����
        client->last_heartbeat = time(NULL);

        return 0;
    }
    else {
        client_change_state(client, CLIENT_STATE_CONNECTED, CLIENT_EVENT_AUTH_FAILED);
        client->stats.connection_failures++;

        if (message) {
            client_set_last_error(client, message);
        }

        LOG_ERROR("Authentication failed for '%s': %s",
            client->config.username,
            message ? message : "Unknown error");

        return -1;
    }
}

static void client_handle_chat_received(chat_client_t* client, const char* username, const char* message, time_t timestamp) {
    if (!client || utils_string_is_empty(message)) {
        return;
    }

    LOG_DEBUG("Chat received from %s: %s",
        username ? username : "[Unknown]", message);

    // ��� ������Ʈ
    client->stats.messages_received++;
    client->stats.last_activity = time(NULL);

    // UI �̺�Ʈ ����
    network_event_t event = { 0 };
    event.type = NET_EVENT_CHAT_RECEIVED;
    event.timestamp = timestamp;

    if (username) {
        utils_string_copy(event.username, sizeof(event.username), username);
    }

    utils_string_copy(event.message, sizeof(event.message), message);

    // �̺�Ʈ ť�� �߰�
    if (event_queue_push(client->event_queue, &event) != 0) {
        LOG_WARNING("Event queue full, chat message may be lost");
    }
}

static void client_handle_user_list_received(chat_client_t* client, const char* user_list) {
    if (!client || utils_string_is_empty(user_list)) {
        return;
    }

    LOG_DEBUG("User list received: %s", user_list);

    // UI �̺�Ʈ ����
    network_event_t event = { 0 };
    event.type = NET_EVENT_USER_LIST_RECEIVED;
    event.timestamp = time(NULL);
    utils_string_copy(event.message, sizeof(event.message), user_list);

    // �̺�Ʈ ť�� �߰�
    if (event_queue_push(client->event_queue, &event) != 0) {
        LOG_WARNING("Event queue full, user list may be lost");
    }
}

static void client_handle_user_joined(chat_client_t* client, const char* username) {
    if (!client || utils_string_is_empty(username)) {
        return;
    }

    LOG_INFO("User joined: %s", username);

    // UI �̺�Ʈ ����
    network_event_t event = { 0 };
    event.type = NET_EVENT_USER_JOINED;
    event.timestamp = time(NULL);
    utils_string_copy(event.username, sizeof(event.username), username);
    sprintf_s(event.message, sizeof(event.message), "%s has joined the chat", username);

    // �̺�Ʈ ť�� �߰�
    if (event_queue_push(client->event_queue, &event) != 0) {
        LOG_WARNING("Event queue full, user join notification may be lost");
    }
}

static void client_handle_user_left(chat_client_t* client, const char* username) {
    if (!client || utils_string_is_empty(username)) {
        return;
    }

    LOG_INFO("User left: %s", username);

    // UI �̺�Ʈ ����
    network_event_t event = { 0 };
    event.type = NET_EVENT_USER_LEFT;
    event.timestamp = time(NULL);
    utils_string_copy(event.username, sizeof(event.username), username);
    sprintf_s(event.message, sizeof(event.message), "%s has left the chat", username);

    // �̺�Ʈ ť�� �߰�
    if (event_queue_push(client->event_queue, &event) != 0) {
        LOG_WARNING("Event queue full, user leave notification may be lost");
    }
}

static void client_handle_connection_lost(chat_client_t* client, const char* reason) {
    if (!client) {
        return;
    }

    LOG_WARNING("Connection lost: %s", reason ? reason : "Unknown reason");

    // ���� ����
    client_change_state(client, CLIENT_STATE_DISCONNECTED, CLIENT_EVENT_CONNECTION_LOST);

    // ���� ����
    if (reason) {
        client_set_last_error(client, reason);
    }

    // ���� ���� ����
    if (client->server_socket) {
        network_socket_close(client->server_socket);
        network_socket_destroy(client->server_socket);
        client->server_socket = NULL;
    }

    // UI �̺�Ʈ ����
    network_event_t event = { 0 };
    event.type = NET_EVENT_CONNECTION_STATUS;
    event.new_state = CLIENT_STATE_DISCONNECTED;
    event.timestamp = time(NULL);
    sprintf_s(event.message, sizeof(event.message),
        "Connection lost: %s", reason ? reason : "Unknown reason");

    // �̺�Ʈ ť�� �߰�
    event_queue_push(client->event_queue, &event);

    // �ڵ� �翬�� �õ�
    if (client->config.auto_reconnect && !client->should_shutdown) {
        LOG_INFO("Scheduling reconnection in %d seconds", client->config.reconnect_interval);

        client->stats.reconnect_count++;

        // �翬�� �̺�Ʈ ����
        network_event_t reconnect_event = { 0 };
        reconnect_event.type = NET_EVENT_CONNECTION_STATUS;
        reconnect_event.new_state = CLIENT_STATE_RECONNECTING;
        reconnect_event.timestamp = time(NULL);
        sprintf_s(reconnect_event.message, sizeof(reconnect_event.message),
            "Reconnecting in %d seconds...", client->config.reconnect_interval);

        event_queue_push(client->event_queue, &reconnect_event);
    }
}

// =============================================================================
// ���� API���� ����� �� �ִ� ���� �Լ���
// =============================================================================

void client_notify_connection_result(chat_client_t* client, int success, const char* error_message) {
    client_handle_connection_result(client, success, error_message);
}

void client_notify_auth_result(chat_client_t* client, int success, const char* message) {
    client_handle_authentication_result(client, success, message);
}

void client_notify_chat_received(chat_client_t* client, const char* username, const char* message, time_t timestamp) {
    client_handle_chat_received(client, username, message, timestamp);
}

void client_notify_user_list(chat_client_t* client, const char* user_list) {
    client_handle_user_list_received(client, user_list);
}

void client_notify_user_joined(chat_client_t* client, const char* username) {
    client_handle_user_joined(client, username);
}

void client_notify_user_left(chat_client_t* client, const char* username) {
    client_handle_user_left(client, username);
}

void client_notify_connection_lost(chat_client_t* client, const char* reason) {
    client_handle_connection_lost(client, reason);
}