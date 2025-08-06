#include "server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// =============================================================================
// 프로그램 정보
// =============================================================================

#define PROGRAM_NAME        "Chat Server"
#define PROGRAM_VERSION     "1.0.0"
#define PROGRAM_AUTHOR      "Pisces Team"
#define PROGRAM_DESCRIPTION "Multi-client TCP chat server with authentication"

// =============================================================================
// 명령줄 옵션 구조체
// =============================================================================

typedef struct {
    int show_help;              // 도움말 표시
    int show_version;           // 버전 표시
    int port;                   // 서버 포트 (-1이면 기본값 사용)
    char bind_interface[16];    // 바인드 인터페이스
    int max_clients;            // 최대 클라이언트 수
    int verbose;                // 상세 로그 레벨
    int daemon_mode;            // 데몬 모드 (추후 구현)
    char config_file[256];      // 설정 파일 경로
} command_args_t;

// =============================================================================
// 함수 선언
// =============================================================================

static void print_usage(const char* program_name);
static void print_version(void);
static void print_banner(void);
static int parse_arguments(int argc, char* argv[], command_args_t* args);
static server_config_t create_server_config_from_args(const command_args_t* args);
static void cleanup_and_exit(chat_server_t* server, int exit_code);

// =============================================================================
// 메인 함수
// =============================================================================

int main(int argc, char* argv[]) {
    // 명령줄 인수 파싱
    command_args_t args = { 0 };
    if (parse_arguments(argc, argv, &args) != 0) {
        return EXIT_FAILURE;
    }

    // 도움말 또는 버전 표시 후 종료
    if (args.show_help) {
        print_usage(argv[0]);
        return EXIT_SUCCESS;
    }

    if (args.show_version) {
        print_version();
        return EXIT_SUCCESS;
    }

    // 프로그램 배너 출력
    print_banner();

    // 네트워크 초기화
    LOG_INFO("Initializing network subsystem...");
    if (network_initialize() != NETWORK_SUCCESS) {
        LOG_ERROR("Failed to initialize network");
        return EXIT_FAILURE;
    }

    // 서버 설정 생성
    server_config_t config = create_server_config_from_args(&args);

    // 서버 인스턴스 생성
    LOG_INFO("Creating server instance...");
    chat_server_t* server = server_create(&config);
    if (!server) {
        LOG_ERROR("Failed to create server instance");
        network_cleanup();
        return EXIT_FAILURE;
    }

    // 신호 처리기 설정
    server_setup_signal_handlers(server);

    // 서버 시작
    LOG_INFO("Starting server...");
    if (server_start(server) != 0) {
        LOG_ERROR("Failed to start server");
        cleanup_and_exit(server, EXIT_FAILURE);
    }

    // 서버 정보 출력
    LOG_INFO("=================================================");
    LOG_INFO("Server started successfully!");
    LOG_INFO("Port: %d", config.port);
    LOG_INFO("Max clients: %d", config.max_clients);
    LOG_INFO("Heartbeat: %s", config.enable_heartbeat ? "Enabled" : "Disabled");
    LOG_INFO("=================================================");
    LOG_INFO("Press Ctrl+C to stop the server");
    LOG_INFO("");

    // 메인 루프 실행
    int result = server_run(server);

    if (result == 0) {
        LOG_INFO("Server terminated normally");
    }
    else {
        LOG_ERROR("Server terminated with errors");
    }

    // 정리 및 종료
    cleanup_and_exit(server, result == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
    return EXIT_SUCCESS;  // 실제로는 cleanup_and_exit에서 exit() 호출
}

// =============================================================================
// 도움말 및 정보 출력 함수들
// =============================================================================

static void print_usage(const char* program_name) {
    printf("Usage: %s [OPTIONS]\n\n", program_name);
    printf("A multi-client TCP chat server with user authentication.\n\n");

    printf("OPTIONS:\n");
    printf("  -p, --port <port>       Server port (default: %d)\n", DEFAULT_SERVER_PORT);
    printf("  -b, --bind <interface>  Bind to specific interface (default: all)\n");
    printf("  -m, --max-clients <num> Maximum clients (default: %d, max: %d)\n",
        MAX_SERVER_CLIENTS, MAX_SERVER_CLIENTS);
    printf("  -v, --verbose           Enable verbose logging (DEBUG level)\n");
    printf("  -q, --quiet             Quiet mode (ERROR level only)\n");
    printf("  -c, --config <file>     Load configuration from file\n");
    printf("  -h, --help              Show this help message\n");
    printf("      --version           Show version information\n");
    printf("\n");

    printf("EXAMPLES:\n");
    printf("  %s                      Start server on default port %d\n", program_name, DEFAULT_SERVER_PORT);
    printf("  %s -p 9000 -v          Start on port 9000 with verbose logging\n", program_name);
    printf("  %s -b 127.0.0.1 -m 32  Bind to localhost, max 32 clients\n", program_name);
    printf("\n");

    printf("SIGNALS:\n");
    printf("  SIGINT (Ctrl+C)         Graceful shutdown\n");
    printf("\n");

    printf("For more information, visit: https://github.com/pisces-team/chat-server\n");
}

static void print_version(void) {
    printf("%s v%s\n", PROGRAM_NAME, PROGRAM_VERSION);
    printf("Author: %s\n", PROGRAM_AUTHOR);
    printf("Description: %s\n", PROGRAM_DESCRIPTION);
    printf("\n");
    printf("Build info:\n");
    printf("  Compiled: %s %s\n", __DATE__, __TIME__);
    printf("  Platform: Windows (Winsock2)\n");
    printf("  Max clients: %d\n", MAX_SERVER_CLIENTS);
    printf("  Protocol version: %d\n", PROTOCOL_VERSION);
}

static void print_banner(void) {
    printf("\n");
    printf("  +==========================================+\n");
    printf("  |            %s v%s            |\n", PROGRAM_NAME, PROGRAM_VERSION);
    printf("  |        Multi-Client TCP Chat Server     |\n");
    printf("  +==========================================+\n");
    printf("\n");
}

// =============================================================================
// 명령줄 인수 파싱
// =============================================================================

static int parse_arguments(int argc, char* argv[], command_args_t* args) {
    // 기본값 설정
    args->show_help = 0;
    args->show_version = 0;
    args->port = -1;  // -1이면 기본값 사용
    args->bind_interface[0] = '\0';
    args->max_clients = -1;  // -1이면 기본값 사용
    args->verbose = 0;
    args->daemon_mode = 0;
    args->config_file[0] = '\0';

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

        // 포트
        else if (strcmp(arg, "-p") == 0 || strcmp(arg, "--port") == 0) {
            if (i + 1 >= argc) {
                LOG_ERROR("Option %s requires a port number", arg);
                return -1;
            }

            args->port = atoi(argv[++i]);
            if (args->port <= 0 || args->port > 65535) {
                LOG_ERROR("Invalid port number: %d (must be 1-65535)", args->port);
                return -1;
            }
        }

        // 바인드 인터페이스
        else if (strcmp(arg, "-b") == 0 || strcmp(arg, "--bind") == 0) {
            if (i + 1 >= argc) {
                LOG_ERROR("Option %s requires an interface address", arg);
                return -1;
            }

            utils_string_copy(args->bind_interface, sizeof(args->bind_interface), argv[++i]);
        }

        // 최대 클라이언트 수
        else if (strcmp(arg, "-m") == 0 || strcmp(arg, "--max-clients") == 0) {
            if (i + 1 >= argc) {
                LOG_ERROR("Option %s requires a number", arg);
                return -1;
            }

            args->max_clients = atoi(argv[++i]);
            if (args->max_clients <= 0 || args->max_clients > MAX_SERVER_CLIENTS) {
                LOG_ERROR("Invalid max clients: %d (must be 1-%d)", args->max_clients, MAX_SERVER_CLIENTS);
                return -1;
            }
        }

        // 상세 로그
        else if (strcmp(arg, "-v") == 0 || strcmp(arg, "--verbose") == 0) {
            args->verbose = 1;
        }

        // 조용한 모드
        else if (strcmp(arg, "-q") == 0 || strcmp(arg, "--quiet") == 0) {
            args->verbose = -1;  // 조용한 모드 표시
        }

        // 설정 파일
        else if (strcmp(arg, "-c") == 0 || strcmp(arg, "--config") == 0) {
            if (i + 1 >= argc) {
                LOG_ERROR("Option %s requires a file path", arg);
                return -1;
            }

            utils_string_copy(args->config_file, sizeof(args->config_file), argv[++i]);
        }

        // 알 수 없는 옵션
        else {
            LOG_ERROR("Unknown option: %s", arg);
            LOG_ERROR("Use -h or --help for usage information");
            return -1;
        }
    }

    return 0;
}

// =============================================================================
// 서버 설정 생성
// =============================================================================

static server_config_t create_server_config_from_args(const command_args_t* args) {
    server_config_t config;

    // 설정 파일이 지정된 경우 먼저 로드
    if (args->config_file[0] != '\0') {
        if (server_load_config_from_file(args->config_file, &config) != 0) {
            LOG_WARNING("Failed to load config file, using defaults");
            config = server_get_default_config();
        }
    }
    else {
        config = server_get_default_config();
    }

    // 명령줄 인수로 설정 덮어쓰기
    if (args->port != -1) {
        config.port = (uint16_t)args->port;
    }

    if (args->bind_interface[0] != '\0') {
        utils_string_copy(config.bind_interface, sizeof(config.bind_interface), args->bind_interface);
    }

    if (args->max_clients != -1) {
        config.max_clients = args->max_clients;
    }

    // 로그 레벨 설정
    if (args->verbose == 1) {
        config.log_level = LOG_LEVEL_DEBUG;
    }
    else if (args->verbose == -1) {
        config.log_level = LOG_LEVEL_ERROR;
    }

    return config;
}

// =============================================================================
// 정리 및 종료
// =============================================================================

static void cleanup_and_exit(chat_server_t* server, int exit_code) {
    LOG_INFO("Cleaning up and shutting down...");

    // 서버 중지
    if (server) {
        if (server->state == SERVER_STATE_RUNNING) {
            server_stop(server);
        }

        // 최종 통계 출력
        LOG_INFO("Final server statistics:");
        server_print_statistics(server);

        // 서버 인스턴스 해제
        server_destroy(server);
    }

    // 네트워크 정리
    network_cleanup();

    // 종료 메시지
    if (exit_code == EXIT_SUCCESS) {
        LOG_INFO("Server shutdown completed successfully");
    }
    else {
        LOG_ERROR("Server shutdown completed with errors");
    }

    printf("\nThank you for using %s!\n", PROGRAM_NAME);

    // 프로그램 종료
    exit(exit_code);
}

// =============================================================================
// Windows 전용 진입점 (선택사항)
// =============================================================================

#ifdef _WIN32
// Windows 서비스 모드 지원을 위한 준비 (추후 구현)
// 현재는 콘솔 애플리케이션으로만 동작

// 콘솔 이벤트 핸들러 (Ctrl+C 등)
static BOOL WINAPI console_ctrl_handler(DWORD dwCtrlType) {
    switch (dwCtrlType) {
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
    case CTRL_CLOSE_EVENT:
        LOG_INFO("Console close event received");
        // 신호 핸들러에서 처리됨
        return TRUE;

    case CTRL_LOGOFF_EVENT:
    case CTRL_SHUTDOWN_EVENT:
        LOG_WARNING("System shutdown event received");
        return FALSE;  // 시스템이 처리하도록 함

    default:
        return FALSE;
    }
}

#endif // _WIN32