#include "client.h"
#include "ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

// =============================================================================
// ���α׷� ����
// =============================================================================

#define CLIENT_PROGRAM_NAME     "Chat Client"
#define CLIENT_VERSION          "1.0.0"
#define CLIENT_AUTHOR           "Pisces Team"
#define CLIENT_DESCRIPTION      "Multi-user TCP chat client with console UI"

// =============================================================================
// ����� �ɼ� ����ü
// =============================================================================

typedef struct {
    int show_help;              // ���� ǥ��
    int show_version;           // ���� ǥ��
    char server_host[256];      // ���� ȣ��Ʈ
    int server_port;            // ���� ��Ʈ (-1�̸� �⺻��)
    char username[MAX_USERNAME_LENGTH]; // ����ڸ�
    int auto_connect;           // �ڵ� ���� ����
    int auto_auth;              // �ڵ� ���� ����
    int verbose;                // �� �α�
    int no_colors;              // ���� ��Ȱ��ȭ
} client_args_t;

// =============================================================================
// ���� ����
// =============================================================================

static chat_client_t* g_client = NULL;

// =============================================================================
// �Լ� ����
// =============================================================================

static void print_client_usage(const char* program_name);
static void print_client_version(void);
static void print_client_banner(void);
static int parse_client_arguments(int argc, char* argv[], client_args_t* args);
static client_config_t create_client_config_from_args(const client_args_t* args);
static void cleanup_and_exit_client(int exit_code);
static void client_signal_handler(int signum);

// =============================================================================
// ���� �Լ�
// =============================================================================

static int handle_user_input(chat_client_t* client, void* user_data) {
    // ����ŷ Ű���� �Է� üũ
    if (_kbhit()) {
        char input_buffer[512];

        // �� �� �Է� �ޱ�
        if (fgets(input_buffer, sizeof(input_buffer), stdin)) {
            // ���� ���� ����
            size_t len = strlen(input_buffer);
            if (len > 0 && input_buffer[len - 1] == '\n') {
                input_buffer[len - 1] = '\0';
            }

            // �� �Է� ����
            if (strlen(input_buffer) == 0) {
                return 0;
            }

            // ��ɾ� ó�� (ui.c�� �Լ� ȣ��)
            return ui_process_user_input(client, input_buffer);
        }
    }

    return 0; // �Է� ���� �Ǵ� ���� ó��
}

int main(int argc, char* argv[]) {
    // ����� �μ� �Ľ�
    client_args_t args = { 0 };
    if (parse_client_arguments(argc, argv, &args) != 0) {
        return EXIT_FAILURE;
    }

    // ���� �Ǵ� ���� ǥ�� �� ����
    if (args.show_help) {
        print_client_usage(argv[0]);
        return EXIT_SUCCESS;
    }

    if (args.show_version) {
        print_client_version();
        return EXIT_SUCCESS;
    }

    // ���α׷� ��� ���
    print_client_banner();

    // Ŭ���̾�Ʈ ���� ����
    client_config_t config = create_client_config_from_args(&args);

    // Ŭ���̾�Ʈ �ν��Ͻ� ����
    LOG_INFO("Creating client instance...");
    g_client = client_create(&config);
    if (!g_client) {
        LOG_ERROR("Failed to create client instance");
        return EXIT_FAILURE;
    }

    // ��ȣ ó���� ����
    signal(SIGINT, client_signal_handler);

    // Ŭ���̾�Ʈ ������ ����
    LOG_INFO("Starting client threads...");
    if (client_start(g_client) != 0) {
        LOG_ERROR("Failed to start client threads");
        cleanup_and_exit_client(EXIT_FAILURE);
    }

    // �Է� �ڵ鷯 ���
    client_set_input_handler(g_client, handle_user_input, NULL);

    // �ڵ� ���� ó��
    if (args.auto_connect && !utils_string_is_empty(args.server_host)) {
        LOG_INFO("Auto-connecting to %s:%d", args.server_host, args.server_port);
        Sleep(1000);  // ��Ʈ��ũ �����尡 �غ�� �ð�

        if (client_connect_to_server(g_client, args.server_host, (uint16_t)args.server_port) == 0) {
            // �ڵ� ���� ó��
            if (args.auto_auth && !utils_string_is_empty(args.username)) {
                LOG_INFO("Auto-authenticating as '%s'", args.username);
                Sleep(2000);  // ������ �Ϸ�� �ð�
                client_authenticate(g_client, args.username);
            }
        }
    }

    LOG_INFO("Starting user interface...");
    LOG_INFO("");

    // Ŭ���̾�Ʈ ���� ���
    LOG_INFO("=================================================");
    LOG_INFO("Client started successfully!");
    if (!utils_string_is_empty(config.server_host) && config.server_port > 0) {
        LOG_INFO("Default server: %s:%d", config.server_host, config.server_port);
    }
    if (!utils_string_is_empty(config.username)) {
        LOG_INFO("Username: %s", config.username);
    }
    LOG_INFO("Auto reconnect: %s", config.auto_reconnect ? "Enabled" : "Disabled");
    LOG_INFO("=================================================");
    LOG_INFO("Starting user interface...");
    LOG_INFO("");

    // ���� UI ���� ����
    int result = client_run(g_client);

    if (result == 0) {
        LOG_INFO("Client terminated normally");
    }
    else {
        LOG_ERROR("Client terminated with errors");
    }

    // ���� �� ����
    cleanup_and_exit_client(result == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
    return EXIT_SUCCESS;  // �����δ� cleanup_and_exit_client���� exit() ȣ��
}

// =============================================================================
// ���� �� ���� ��� �Լ���
// =============================================================================

static void print_client_usage(const char* program_name) {
    printf("Usage: %s [OPTIONS]\n\n", program_name);
    printf("A multi-user TCP chat client with console interface.\n\n");

    printf("OPTIONS:\n");
    printf("  -s, --server <host>     Default server host (default: %s)\n", DEFAULT_SERVER_HOST);
    printf("  -p, --port <port>       Default server port (default: %d)\n", DEFAULT_SERVER_PORT);
    printf("  -u, --username <name>   Default username\n");
    printf("  -c, --connect           Auto-connect to server on startup\n");
    printf("  -a, --auth              Auto-authenticate with username\n");
    printf("  -v, --verbose           Enable verbose logging (DEBUG level)\n");
    printf("  -q, --quiet             Quiet mode (ERROR level only)\n");
    printf("      --no-colors         Disable colored output\n");
    printf("  -h, --help              Show this help message\n");
    printf("      --version           Show version information\n");
    printf("\n");

    printf("EXAMPLES:\n");
    printf("  %s                          Start client with default settings\n", program_name);
    printf("  %s -s localhost -p 9000     Set default server to localhost:9000\n", program_name);
    printf("  %s -s example.com -u Alice -c -a\n", program_name);
    printf("                              Auto-connect to example.com and login as Alice\n");
    printf("  %s -v                       Start with verbose logging\n", program_name);
    printf("\n");

    printf("CHAT COMMANDS (once connected):\n");
    printf("  /connect <host> <port>      Connect to server\n");
    printf("  /auth <username>            Authenticate with username\n");
    printf("  /disconnect                 Disconnect from server\n");
    printf("  /users                      Show online users\n");
    printf("  /help                       Show chat commands\n");
    printf("  /quit                       Exit client\n");
    printf("  <message>                   Send chat message\n");
    printf("\n");

    printf("For more information, visit: https://github.com/pisces-team/chat-client\n");
}

static void print_client_version(void) {
    printf("%s v%s\n", CLIENT_PROGRAM_NAME, CLIENT_VERSION);
    printf("Author: %s\n", CLIENT_AUTHOR);
    printf("Description: %s\n", CLIENT_DESCRIPTION);
    printf("\n");
    printf("Build info:\n");
    printf("  Compiled: %s %s\n", __DATE__, __TIME__);
    printf("  Platform: Windows (Winsock2 + Console API)\n");
    printf("  Protocol version: %d\n", PROTOCOL_VERSION);
    printf("  Features: Auto-reconnect, Console UI, Multi-threading\n");
}

static void print_client_banner(void) {
    printf("\n");
    printf("  +==========================================+\n");
    printf("  |        %s v%s         |\n", CLIENT_PROGRAM_NAME, CLIENT_VERSION);
    printf("  |      Multi-User Console Chat Client     |\n");
    printf("  +==========================================+\n");
    printf("\n");
}

// =============================================================================
// ����� �μ� �Ľ�
// =============================================================================

static int parse_client_arguments(int argc, char* argv[], client_args_t* args) {
    // �⺻�� ����
    args->show_help = 0;
    args->show_version = 0;
    args->server_host[0] = '\0';
    args->server_port = -1;  // -1�̸� �⺻�� ���
    args->username[0] = '\0';
    args->auto_connect = 0;
    args->auto_auth = 0;
    args->verbose = 0;
    args->no_colors = 0;

    for (int i = 1; i < argc; i++) {
        const char* arg = argv[i];

        // ����
        if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
            args->show_help = 1;
            return 0;
        }

        // ����
        else if (strcmp(arg, "--version") == 0) {
            args->show_version = 1;
            return 0;
        }

        // ���� ȣ��Ʈ
        else if (strcmp(arg, "-s") == 0 || strcmp(arg, "--server") == 0) {
            if (i + 1 >= argc) {
                LOG_ERROR("Option %s requires a hostname", arg);
                return -1;
            }

            utils_string_copy(args->server_host, sizeof(args->server_host), argv[++i]);
        }

        // ��Ʈ
        else if (strcmp(arg, "-p") == 0 || strcmp(arg, "--port") == 0) {
            if (i + 1 >= argc) {
                LOG_ERROR("Option %s requires a port number", arg);
                return -1;
            }

            args->server_port = atoi(argv[++i]);
            if (args->server_port <= 0 || args->server_port > 65535) {
                LOG_ERROR("Invalid port number: %d (must be 1-65535)", args->server_port);
                return -1;
            }
        }

        // ����ڸ�
        else if (strcmp(arg, "-u") == 0 || strcmp(arg, "--username") == 0) {
            if (i + 1 >= argc) {
                LOG_ERROR("Option %s requires a username", arg);
                return -1;
            }

            utils_string_copy(args->username, sizeof(args->username), argv[++i]);

            // ����ڸ� ��ȿ�� ����
            if (strlen(args->username) >= MAX_USERNAME_LENGTH) {
                LOG_ERROR("Username too long (max %d characters)", MAX_USERNAME_LENGTH - 1);
                return -1;
            }
        }

        // �ڵ� ����
        else if (strcmp(arg, "-c") == 0 || strcmp(arg, "--connect") == 0) {
            args->auto_connect = 1;
        }

        // �ڵ� ����
        else if (strcmp(arg, "-a") == 0 || strcmp(arg, "--auth") == 0) {
            args->auto_auth = 1;
        }

        // �� �α�
        else if (strcmp(arg, "-v") == 0 || strcmp(arg, "--verbose") == 0) {
            args->verbose = 1;
        }

        // ������ ���
        else if (strcmp(arg, "-q") == 0 || strcmp(arg, "--quiet") == 0) {
            args->verbose = -1;  // ������ ��� ǥ��
        }

        // ���� ��Ȱ��ȭ
        else if (strcmp(arg, "--no-colors") == 0) {
            args->no_colors = 1;
        }

        // �� �� ���� �ɼ�
        else {
            LOG_ERROR("Unknown option: %s", arg);
            LOG_ERROR("Use -h or --help for usage information");
            return -1;
        }
    }

    // �ڵ� ������ ���ؼ��� �ڵ� ����� ����ڸ��� �ʿ�
    if (args->auto_auth) {
        if (!args->auto_connect) {
            LOG_WARNING("Auto-auth requires auto-connect, enabling auto-connect");
            args->auto_connect = 1;
        }

        if (utils_string_is_empty(args->username)) {
            LOG_ERROR("Auto-auth requires a username (-u option)");
            return -1;
        }
    }

    // �ڵ� ������ ���ؼ��� ���� ȣ��Ʈ�� �ʿ�
    if (args->auto_connect && utils_string_is_empty(args->server_host)) {
        LOG_WARNING("Auto-connect requires server host, using default: %s", DEFAULT_SERVER_HOST);
        utils_string_copy(args->server_host, sizeof(args->server_host), DEFAULT_SERVER_HOST);
    }

    return 0;
}

// =============================================================================
// Ŭ���̾�Ʈ ���� ����
// =============================================================================

static client_config_t create_client_config_from_args(const client_args_t* args) {
    client_config_t config = client_get_default_config();

    // ����� �μ��� ���� �����
    if (!utils_string_is_empty(args->server_host)) {
        utils_string_copy(config.server_host, sizeof(config.server_host), args->server_host);
    }

    if (args->server_port != -1) {
        config.server_port = (uint16_t)args->server_port;
    }

    if (!utils_string_is_empty(args->username)) {
        utils_string_copy(config.username, sizeof(config.username), args->username);
    }

    // �α� ���� ����
    if (args->verbose == 1) {
        config.log_level = LOG_LEVEL_DEBUG;
    }
    else if (args->verbose == -1) {
        config.log_level = LOG_LEVEL_ERROR;
    }

    // �ڵ� �翬�� �⺻ Ȱ��ȭ (����ٿ��� ��Ȱ��ȭ �ɼ� ����)
    config.auto_reconnect = 1;

    return config;
}

// =============================================================================
// ���� �� ����
// =============================================================================

static void cleanup_and_exit_client(int exit_code) {
    LOG_INFO("Cleaning up and shutting down client...");

    // Ŭ���̾�Ʈ ����
    if (g_client) {
        // Ŭ���̾�Ʈ�� ���� ���� ���̸� ����
        client_state_t state = client_get_current_state(g_client);
        if (state != CLIENT_STATE_DISCONNECTED) {
            LOG_INFO("Shutting down active client...");
            client_shutdown(g_client);
        }

        // ���� ��� ���
        LOG_INFO("Final client statistics:");
        client_print_statistics(g_client);

        // Ŭ���̾�Ʈ �ν��Ͻ� ����
        client_destroy(g_client);
        g_client = NULL;
    }

    // ��Ʈ��ũ ����
    network_cleanup();

    // ���� �޽���
    if (exit_code == EXIT_SUCCESS) {
        LOG_INFO("Client shutdown completed successfully");
    }
    else {
        LOG_ERROR("Client shutdown completed with errors");
    }

    printf("\nThank you for using %s!\n", CLIENT_PROGRAM_NAME);

    // ���α׷� ����
    exit(exit_code);
}

static void client_signal_handler(int signum) {
    if (signum == SIGINT) {
        printf("\n");  // �� �ٷ� ^C ������ ���
        LOG_INFO("Received SIGINT (Ctrl+C), initiating graceful shutdown...");

        if (g_client) {
            // UI�� ���� �޽��� ǥ��
            if (g_ui_state) {
                ui_add_system_message(g_ui_state, "Shutting down... (Ctrl+C pressed)", COLOR_SYSTEM);
                g_ui_state->need_refresh = 1;
                ui_refresh_screen(g_client, g_ui_state);
            }

            // ���� ��ȣ ����
            ui_command_t shutdown_cmd = { 0 };
            shutdown_cmd.type = UI_CMD_SHUTDOWN;
            command_queue_push(g_client->command_queue, &shutdown_cmd);

            // Ŭ���̾�Ʈ ����
            g_client->should_shutdown = 1;
        }
        else {
            // Ŭ���̾�Ʈ�� ������ ��� ����
            cleanup_and_exit_client(EXIT_SUCCESS);
        }
    }
}

// =============================================================================
// Windows ���� �߰� ���
// =============================================================================

#ifdef _WIN32

// �ܼ� â �ݱ� �̺�Ʈ ó��
static BOOL WINAPI console_ctrl_handler(DWORD dwCtrlType) {
    switch (dwCtrlType) {
    case CTRL_C_EVENT:
        // Ctrl+C�� signal handler���� ó��
        return FALSE;

    case CTRL_BREAK_EVENT:
    case CTRL_CLOSE_EVENT:
        LOG_INFO("Console close event received, shutting down...");
        if (g_client) {
            g_client->should_shutdown = 1;
        }
        cleanup_and_exit_client(EXIT_SUCCESS);
        return TRUE;

    case CTRL_LOGOFF_EVENT:
    case CTRL_SHUTDOWN_EVENT:
        LOG_WARNING("System shutdown event received");
        if (g_client) {
            g_client->should_shutdown = 1;
        }
        return FALSE;  // �ý����� ó���ϵ��� ��

    default:
        return FALSE;
    }
}

// ���α׷� ���� �� Windows ���� �ʱ�ȭ
static void windows_client_init(void) {
    // �ܼ� �̺�Ʈ �ڵ鷯 ���
    SetConsoleCtrlHandler(console_ctrl_handler, TRUE);

    // �ܼ� â ���� ���� - UNICODE/ANSI ȣȯ�� ó��
#ifdef UNICODE
    wchar_t title[256];
    swprintf_s(title, sizeof(title) / sizeof(wchar_t), L"%hs v%hs", CLIENT_PROGRAM_NAME, CLIENT_VERSION);
    SetConsoleTitle(title);
#else
    char title[256];
    sprintf_s(title, sizeof(title), "%s v%s", CLIENT_PROGRAM_NAME, CLIENT_VERSION);
    SetConsoleTitle(title);
#endif
}

// WinMain ���� - SAL �ּ� �߰��� ��� �ذ�
#ifdef UNICODE
int WINAPI wWinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR lpCmdLine,
    _In_ int nCmdShow
) {
#else
int WINAPI WinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPSTR lpCmdLine,
    _In_ int nCmdShow
) {
#endif
    // �̻�� �Ű����� ��� ����
    UNREFERENCED_PARAMETER(hInstance);
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    UNREFERENCED_PARAMETER(nCmdShow);

    // �ܼ� �Ҵ� (GUI���� ����� ���)
    if (AllocConsole()) {
        FILE* pCout;
        FILE* pCerr;
        FILE* pCin;

        // freopen_s�� ��ȯ�� Ȯ��
        if (freopen_s(&pCout, "CONOUT$", "w", stdout) != 0 ||
            freopen_s(&pCerr, "CONOUT$", "w", stderr) != 0 ||
            freopen_s(&pCin, "CONIN$", "r", stdin) != 0) {
            // �ܼ� �翬�� ���� �� ó��
            OutputDebugStringA("Failed to redirect console streams\n");
        }
    }

    // ����� �μ� �Ľ� ����
    LPSTR cmdLine = GetCommandLineA();

    // CommandLineToArgvW ����Ͽ� ��Ȯ�� �Ľ�
    int argc = 0;
    char* argv[32] = { 0 };

    // ������ ���� (���� ���δ��ǿ����� CommandLineToArgvW ��� ����)
    argv[0] = "client.exe";  // �⺻ ���α׷���
    argc = 1;

    // �߰� �μ� �Ľ��� �ʿ��� ��� ���⿡ ����
    // ����� �⺻���� ���

    // Windows �ʱ�ȭ �� main �Լ� ȣ��
    windows_client_init();
    return main(argc, argv);
}

#else
// Windows�� �ƴ� �÷��������� �� �Լ�
static void windows_client_init(void) {}
#endif

// =============================================================================
// �߰� ��������: ������ ������ ���Ḧ ���� ���� �Լ�
// =============================================================================

// ��Ʈ��ũ �����忡�� ����� ���� Ȯ�� �Լ�
static int client_should_exit_thread(const chat_client_t * client) {
    if (!client) {
        return 1;  // ������ ���� ����
    }

    return client->should_shutdown ||
        (WaitForSingleObject(client->shutdown_event, 0) == WAIT_OBJECT_0);
}

// ��Ʈ��ũ ������ ���� �Լ����� �ֱ������� ȣ���ؾ� �ϴ� ���� üũ
// network_thread_proc �Լ����� ������ ���� ���:
//
// while (!client_should_exit_thread(client)) {
//     // ��Ʈ��ũ �۾� ����
//     
//     // ���ŷ �۾� �� Ÿ�Ӿƿ� ����
//     // ��: select(), recv() � Ÿ�Ӿƿ� ����
// }

// =============================================================================
// �߰� ����: ���� ó�� �� �α� ����
// =============================================================================

// TerminateThread ��� ���� ������ ���Ḧ �����ϴ� ���:
// 1. ��� ���ŷ ��Ʈ��ũ �۾��� Ÿ�Ӿƿ� ����
// 2. �ֱ����� ���� ��ȣ Ȯ��
// 3. ���� ���� ����� ���ŷ ����
// 4. ���� �������θ� ���μ��� ���� �� �ý��� ������ ����

#ifdef _DEBUG
// ����� ��忡�� �߰� ����
static void debug_verify_thread_cleanup(const chat_client_t * client) {
    if (!client || !client->network_thread) {
        return;
    }

    DWORD exit_code;
    if (GetExitCodeThread(client->network_thread, &exit_code)) {
        if (exit_code == STILL_ACTIVE) {
            LOG_WARNING("Network thread still active during cleanup");
        }
        else {
            LOG_DEBUG("Network thread exited with code: %lu", exit_code);
        }
    }
}
#else
#define debug_verify_thread_cleanup(client) ((void)0)
#endif