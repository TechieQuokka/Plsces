#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

// =============================================================================
// 프로토콜 기본 상수
// =============================================================================

#define PROTOCOL_MAGIC          0x43484154  // "CHAT" in hex
#define PROTOCOL_VERSION        1           // 현재 프로토콜 버전
#define MAX_MESSAGE_SIZE        4096        // 최대 메시지 크기 (4KB)
#define MAX_USERNAME_LENGTH     32          // 최대 사용자명 길이
#define MAX_CLIENTS             100         // 최대 동시 접속자 수
#define SERVER_DEFAULT_PORT     8080        // 기본 서버 포트

// =============================================================================
// 메시지 타입 정의 (카테고리별 분류)
// =============================================================================

typedef enum {
    // 시스템 메시지 (1000번대)
    MSG_SYSTEM_BASE = 1000,
    MSG_CONNECT_REQUEST = 1001,     // 클라이언트 -> 서버: 연결 요청
    MSG_CONNECT_RESPONSE = 1002,    // 서버 -> 클라이언트: 연결 응답
    MSG_DISCONNECT = 1003,          // 양방향: 연결 종료
    MSG_HEARTBEAT = 1004,           // 양방향: 연결 유지 확인
    MSG_HEARTBEAT_ACK = 1005,       // 양방향: 하트비트 응답

    // 인증 메시지 (2000번대)
    MSG_AUTH_BASE = 2000,
    MSG_AUTH_REQUEST = 2001,        // 클라이언트 -> 서버: 인증 요청
    MSG_AUTH_RESPONSE = 2002,       // 서버 -> 클라이언트: 인증 응답
    MSG_AUTH_FAILED = 2003,         // 서버 -> 클라이언트: 인증 실패

    // 채팅 메시지 (3000번대)
    MSG_CHAT_BASE = 3000,
    MSG_CHAT_SEND = 3001,           // 클라이언트 -> 서버: 채팅 메시지
    MSG_CHAT_BROADCAST = 3002,      // 서버 -> 클라이언트: 브로드캐스트
    MSG_CHAT_PRIVATE = 3003,        // 양방향: 개인 메시지 (추후 확장)

    // 사용자 관리 (4000번대)
    MSG_USER_BASE = 4000,
    MSG_USER_LIST_REQUEST = 4001,   // 클라이언트 -> 서버: 사용자 목록 요청
    MSG_USER_LIST_RESPONSE = 4002,  // 서버 -> 클라이언트: 사용자 목록 응답
    MSG_USER_JOINED = 4003,         // 서버 -> 클라이언트: 사용자 입장
    MSG_USER_LEFT = 4004,           // 서버 -> 클라이언트: 사용자 퇴장

    // 오류 메시지 (9000번대)
    MSG_ERROR_BASE = 9000,
    MSG_ERROR_GENERIC = 9001,       // 일반 오류
    MSG_ERROR_PROTOCOL = 9002,      // 프로토콜 오류
    MSG_ERROR_AUTH = 9003,          // 인증 오류
    MSG_ERROR_PERMISSION = 9004,    // 권한 오류
    MSG_ERROR_SERVER_FULL = 9005,   // 서버 포화 상태
    MSG_ERROR_INVALID_USERNAME = 9006  // 잘못된 사용자명
} message_type_t;

// =============================================================================
// 응답 코드 정의
// =============================================================================

typedef enum {
    RESPONSE_SUCCESS = 0,           // 성공
    RESPONSE_ERROR = -1,            // 일반 오류
    RESPONSE_INVALID_INPUT = -2,    // 잘못된 입력
    RESPONSE_NETWORK_ERROR = -3,    // 네트워크 오류
    RESPONSE_AUTH_FAILED = -4,      // 인증 실패
    RESPONSE_SERVER_FULL = -5,      // 서버 포화
    RESPONSE_USERNAME_TAKEN = -6,   // 사용자명 중복
    RESPONSE_NOT_CONNECTED = -7     // 연결되지 않음
} response_code_t;

// =============================================================================
// 메시지 우선순위 (추후 확장용)
// =============================================================================

typedef enum {
    PRIORITY_LOW = 0,       // 일반 채팅 메시지
    PRIORITY_NORMAL = 1,    // 시스템 메시지
    PRIORITY_HIGH = 2,      // 인증, 연결 관련
    PRIORITY_CRITICAL = 3   // 오류, 긴급 메시지
} message_priority_t;

// =============================================================================
// 유틸리티 매크로
// =============================================================================

// 메시지 타입 카테고리 확인
#define IS_SYSTEM_MSG(type)     ((type) >= MSG_SYSTEM_BASE && (type) < MSG_AUTH_BASE)
#define IS_AUTH_MSG(type)       ((type) >= MSG_AUTH_BASE && (type) < MSG_CHAT_BASE)
#define IS_CHAT_MSG(type)       ((type) >= MSG_CHAT_BASE && (type) < MSG_USER_BASE)
#define IS_USER_MSG(type)       ((type) >= MSG_USER_BASE && (type) < MSG_ERROR_BASE)
#define IS_ERROR_MSG(type)      ((type) >= MSG_ERROR_BASE)

// 메시지 타입을 문자열로 변환 (디버깅용)
const char* message_type_to_string(message_type_t type);

#endif // PROTOCOL_H