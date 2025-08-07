#include "client.h"
#include "ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

// =============================================================================
// 프로그램 정보
// =============================================================================

#define CLIENT_PROGRAM_NAME     "Chat Client"
#define CLIENT_VERSION          "1.0.0"
#define CLIENT_AUTHOR           "Pisces Team"
#define CLIENT_DESCRIPTION      "Multi-user TCP chat client with console UI"

// =============================================================================
// 명령줄 옵션 구조체
// =============================================================================

typedef struct {
    int show_help;              // 도움말 표시
    int show_version;           // 버전 표시
    char server_host[256];      // 서버 호스트
    int server_port;            // 서버 포트 (-1이면 기본값)
    char username[MAX_USERNAME_LENGTH]; // 사용자명
    int auto_connect;           // 자동 연결 여부
    int auto_auth;              // 자동 인증 여부
    int verbose;                // 상세 로그
    int no_colors;              // 색상 비활성화
} client_args_t;

// =============================================================================
// 전역 변수
// =============================================================================

static chat_client_t* g_client = NULL;

// =============================================================================
// 함수 선언
// =============================================================================

static void print_client_usage(const char* program_name);
static void print_client_version(void);
static void print_client_banner(void);
static int parse_client_arguments(int argc, char* argv[], client_args_t* args);
static client_config_t create_client_config_from_args(const client_args_t* args);
static void cleanup_and_exit_client(int exit_code);
static void client_signal_handler(int signum);

// =============================================================================
// 메인 함수
// =============================================================================

static int handle_user_input(chat_client_t* client, void* user_data) {
    // 논블로킹 키보드 입력 체크
    if (_kbhit()) {
        char input_buffer[512];

        // 한 줄 입력 받기
        if (fgets(input_buffer, sizeof(input_buffer), stdin)) {
            // 개행 문자 제거
            size_t len = strlen(input_buffer);
            if (len > 0 && input_buffer[len - 1] == '\n') {
                input_buffer[len - 1] = '\0';
            }

            // 빈 입력 무시
            if (strlen(input_buffer) == 0) {
                return 0;
            }

            // 명령어 처리 (ui.c의 함수 호출)
            return ui_process_user_input(client, input_buffer);
        }
    }

    return 0; // 입력 없음 또는 정상 처리
}

int main(int argc, char* argv[]) {
    // 명령줄 인수 파싱
    client_args_t args = { 0 };
    if (parse_client_arguments(argc, argv, &args) != 0) {
        return EXIT_FAILURE;
    }

    // 도움말 또는 버전 표시 후 종료
    if (args.show_help) {
        print_client_usage(argv[0]);
        return EXIT_SUCCESS;
    }

    if (args.show_version) {
        print_client_version();
        return EXIT_SUCCESS;
    }

    // 프로그램 배너 출력
    print_client_banner();

    // 클라이언트 설정 생성
    client_config_t config = create_client_config_from_args(&args);

    // 클라이언트 인스턴스 생성
    LOG_INFO("Creating client instance...");
    g_client = client_create(&config);
    if (!g_client) {
        LOG_ERROR("Failed to create client instance");
        return EXIT_FAILURE;
    }

    // 신호 처리기 설정
    signal(SIGINT, client_signal_handler);

    // 클라이언트 스레드 시작
    LOG_INFO("Starting client threads...");
    if (client_start(g_client) != 0) {
        LOG_ERROR("Failed to start client threads");
        cleanup_and_exit_client(EXIT_FAILURE);
    }

    // 입력 핸들러 등록
    client_set_input_handler(g_client, handle_user_input, NULL);

    // 자동 연결 처리
    if (args.auto_connect && !utils_string_is_empty(args.server_host)) {
        LOG_INFO("Auto-connecting to %s:%d", args.server_host, args.server_port);
        Sleep(1000);  // 네트워크 스레드가 준비될 시간

        if (client_connect_to_server(g_client, args.server_host, (uint16_t)args.server_port) == 0) {
            // 자동 인증 처리
            if (args.auto_auth && !utils_string_is_empty(args.username)) {
                LOG_INFO("Auto-authenticating as '%s'", args.username);
                Sleep(2000);  // 연결이 완료될 시간
                client_authenticate(g_client, args.username);
            }
        }
    }

    LOG_INFO("Starting user interface...");
    LOG_INFO("");

    // 클라이언트 정보 출력
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

    // 메인 UI 루프 실행
    int result = client_run(g_client);

    if (result == 0) {
        LOG_INFO("Client terminated normally");
    }
    else {
        LOG_ERROR("Client terminated with errors");
    }

    // 정리 및 종료
    cleanup_and_exit_client(result == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
    return EXIT_SUCCESS;  // 실제로는 cleanup_and_exit_client에서 exit() 호출
}

// =============================================================================
// 도움말 및 정보 출력 함수들
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
// 명령줄 인수 파싱
// =============================================================================

static int parse_client_arguments(int argc, char* argv[], client_args_t* args) {
    // 기본값 설정
    args->show_help = 0;
    args->show_version = 0;
    args->server_host[0] = '\0';
    args->server_port = -1;  // -1이면 기본값 사용
    args->username[0] = '\0';
    args->auto_connect = 0;
    args->auto_auth = 0;
    args->verbose = 0;
    args->no_colors = 0;

    for (int i = 1; i < argc; i++) {
        const char* arg = argv[i];

        // 도움말
        if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
            args->show_help = 1;
            return 0;
        }

        // 버전
        else if (strcmp(arg, "--version") == 0) {
            args->show_version = 1;
            return 0;
        }

        // 서버 호스트
        else if (strcmp(arg, "-s") == 0 || strcmp(arg, "--server") == 0) {
            if (i + 1 >= argc) {
                LOG_ERROR("Option %s requires a hostname", arg);
                return -1;
            }

            utils_string_copy(args->server_host, sizeof(args->server_host), argv[++i]);
        }

        // 포트
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

        // 사용자명
        else if (strcmp(arg, "-u") == 0 || strcmp(arg, "--username") == 0) {
            if (i + 1 >= argc) {
                LOG_ERROR("Option %s requires a username", arg);
                return -1;
            }

            utils_string_copy(args->username, sizeof(args->username), argv[++i]);

            // 사용자명 유효성 검증
            if (strlen(args->username) >= MAX_USERNAME_LENGTH) {
                LOG_ERROR("Username too long (max %d characters)", MAX_USERNAME_LENGTH - 1);
                return -1;
            }
        }

        // 자동 연결
        else if (strcmp(arg, "-c") == 0 || strcmp(arg, "--connect") == 0) {
            args->auto_connect = 1;
        }

        // 자동 인증
        else if (strcmp(arg, "-a") == 0 || strcmp(arg, "--auth") == 0) {
            args->auto_auth = 1;
        }

        // 상세 로그
        else if (strcmp(arg, "-v") == 0 || strcmp(arg, "--verbose") == 0) {
            args->verbose = 1;
        }

        // 조용한 모드
        else if (strcmp(arg, "-q") == 0 || strcmp(arg, "--quiet") == 0) {
            args->verbose = -1;  // 조용한 모드 표시
        }

        // 색상 비활성화
        else if (strcmp(arg, "--no-colors") == 0) {
            args->no_colors = 1;
        }

        // 알 수 없는 옵션
        else {
            LOG_ERROR("Unknown option: %s", arg);
            LOG_ERROR("Use -h or --help for usage information");
            return -1;
        }
    }

    // 자동 인증을 위해서는 자동 연결과 사용자명이 필요
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

    // 자동 연결을 위해서는 서버 호스트가 필요
    if (args->auto_connect && utils_string_is_empty(args->server_host)) {
        LOG_WARNING("Auto-connect requires server host, using default: %s", DEFAULT_SERVER_HOST);
        utils_string_copy(args->server_host, sizeof(args->server_host), DEFAULT_SERVER_HOST);
    }

    return 0;
}

// =============================================================================
// 클라이언트 설정 생성
// =============================================================================

static client_config_t create_client_config_from_args(const client_args_t* args) {
    client_config_t config = client_get_default_config();

    // 명령줄 인수로 설정 덮어쓰기
    if (!utils_string_is_empty(args->server_host)) {
        utils_string_copy(config.server_host, sizeof(config.server_host), args->server_host);
    }

    if (args->server_port != -1) {
        config.server_port = (uint16_t)args->server_port;
    }

    if (!utils_string_is_empty(args->username)) {
        utils_string_copy(config.username, sizeof(config.username), args->username);
    }

    // 로그 레벨 설정
    if (args->verbose == 1) {
        config.log_level = LOG_LEVEL_DEBUG;
    }
    else if (args->verbose == -1) {
        config.log_level = LOG_LEVEL_ERROR;
    }

    // 자동 재연결 기본 활성화 (명령줄에서 비활성화 옵션 없음)
    config.auto_reconnect = 1;

    return config;
}

// =============================================================================
// 정리 및 종료
// =============================================================================

static void cleanup_and_exit_client(int exit_code) {
    LOG_INFO("Cleaning up and shutting down client...");

    // 클라이언트 종료
    if (g_client) {
        // 클라이언트가 아직 실행 중이면 종료
        client_state_t state = client_get_current_state(g_client);
        if (state != CLIENT_STATE_DISCONNECTED) {
            LOG_INFO("Shutting down active client...");
            client_shutdown(g_client);
        }

        // 최종 통계 출력
        LOG_INFO("Final client statistics:");
        client_print_statistics(g_client);

        // 클라이언트 인스턴스 해제
        client_destroy(g_client);
        g_client = NULL;
    }

    // 네트워크 정리
    network_cleanup();

    // 종료 메시지
    if (exit_code == EXIT_SUCCESS) {
        LOG_INFO("Client shutdown completed successfully");
    }
    else {
        LOG_ERROR("Client shutdown completed with errors");
    }

    printf("\nThank you for using %s!\n", CLIENT_PROGRAM_NAME);

    // 프로그램 종료
    exit(exit_code);
}

static void client_signal_handler(int signum) {
    if (signum == SIGINT) {
        printf("\n");  // 새 줄로 ^C 다음에 출력
        LOG_INFO("Received SIGINT (Ctrl+C), initiating graceful shutdown...");

        if (g_client) {
            // UI에 종료 메시지 표시
            if (g_ui_state) {
                ui_add_system_message(g_ui_state, "Shutting down... (Ctrl+C pressed)", COLOR_SYSTEM);
                g_ui_state->need_refresh = 1;
                ui_refresh_screen(g_client, g_ui_state);
            }

            // 종료 신호 전송
            ui_command_t shutdown_cmd = { 0 };
            shutdown_cmd.type = UI_CMD_SHUTDOWN;
            command_queue_push(g_client->command_queue, &shutdown_cmd);

            // 클라이언트 종료
            g_client->should_shutdown = 1;
        }
        else {
            // 클라이언트가 없으면 즉시 종료
            cleanup_and_exit_client(EXIT_SUCCESS);
        }
    }
}

// =============================================================================
// Windows 전용 추가 기능
// =============================================================================

#ifdef _WIN32

// 콘솔 창 닫기 이벤트 처리
static BOOL WINAPI console_ctrl_handler(DWORD dwCtrlType) {
    switch (dwCtrlType) {
    case CTRL_C_EVENT:
        // Ctrl+C는 signal handler에서 처리
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
        return FALSE;  // 시스템이 처리하도록 함

    default:
        return FALSE;
    }
}

// 프로그램 시작 시 Windows 전용 초기화
static void windows_client_init(void) {
    // 콘솔 이벤트 핸들러 등록
    SetConsoleCtrlHandler(console_ctrl_handler, TRUE);

    // 콘솔 창 제목 설정 - UNICODE/ANSI 호환성 처리
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

// WinMain 지원 - SAL 주석 추가로 경고 해결
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
    // 미사용 매개변수 경고 방지
    UNREFERENCED_PARAMETER(hInstance);
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    UNREFERENCED_PARAMETER(nCmdShow);

    // 콘솔 할당 (GUI에서 실행된 경우)
    if (AllocConsole()) {
        FILE* pCout;
        FILE* pCerr;
        FILE* pCin;

        // freopen_s의 반환값 확인
        if (freopen_s(&pCout, "CONOUT$", "w", stdout) != 0 ||
            freopen_s(&pCerr, "CONOUT$", "w", stderr) != 0 ||
            freopen_s(&pCin, "CONIN$", "r", stdin) != 0) {
            // 콘솔 재연결 실패 시 처리
            OutputDebugStringA("Failed to redirect console streams\n");
        }
    }

    // 명령줄 인수 파싱 개선
    LPSTR cmdLine = GetCommandLineA();

    // CommandLineToArgvW 사용하여 정확한 파싱
    int argc = 0;
    char* argv[32] = { 0 };

    // 간단한 구현 (실제 프로덕션에서는 CommandLineToArgvW 사용 권장)
    argv[0] = "client.exe";  // 기본 프로그램명
    argc = 1;

    // 추가 인수 파싱이 필요한 경우 여기에 구현
    // 현재는 기본값만 사용

    // Windows 초기화 및 main 함수 호출
    windows_client_init();
    return main(argc, argv);
}

#else
// Windows가 아닌 플랫폼에서는 빈 함수
static void windows_client_init(void) {}
#endif

// =============================================================================
// 추가 개선사항: 안전한 스레드 종료를 위한 헬퍼 함수
// =============================================================================

// 네트워크 스레드에서 사용할 종료 확인 함수
static int client_should_exit_thread(const chat_client_t * client) {
    if (!client) {
        return 1;  // 안전을 위해 종료
    }

    return client->should_shutdown ||
        (WaitForSingleObject(client->shutdown_event, 0) == WAIT_OBJECT_0);
}

// 네트워크 스레드 메인 함수에서 주기적으로 호출해야 하는 종료 체크
// network_thread_proc 함수에서 다음과 같이 사용:
//
// while (!client_should_exit_thread(client)) {
//     // 네트워크 작업 수행
//     
//     // 블로킹 작업 시 타임아웃 설정
//     // 예: select(), recv() 등에 타임아웃 적용
// }

// =============================================================================
// 추가 개선: 에러 처리 및 로깅 개선
// =============================================================================

// TerminateThread 사용 없이 스레드 종료를 보장하는 방법:
// 1. 모든 블로킹 네트워크 작업에 타임아웃 설정
// 2. 주기적인 종료 신호 확인
// 3. 소켓 강제 종료로 블로킹 해제
// 4. 최후 수단으로만 프로세스 종료 시 시스템 정리에 의존

#ifdef _DEBUG
// 디버그 모드에서 추가 검증
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