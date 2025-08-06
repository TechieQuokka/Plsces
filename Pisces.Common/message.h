#ifndef MESSAGE_H
#define MESSAGE_H

#include "protocol.h"
#include <stdint.h>
#include <time.h>

// =============================================================================
// 경량화된 메시지 구조체
// =============================================================================

// 기본 메시지 헤더 (12바이트로 경량화)
typedef struct {
    uint32_t magic;           // 매직 넘버 (0x43484154)
    uint16_t version;         // 프로토콜 버전
    uint16_t type;            // 메시지 타입 (message_type_t)
    uint32_t payload_size;    // 페이로드 크기
} message_header_t;

// 전체 메시지 구조
typedef struct {
    message_header_t header;
    char* payload;            // 동적 할당될 페이로드
} message_t;

// =============================================================================
// 자주 사용되는 페이로드 구조체들
// =============================================================================

// 연결 요청 페이로드
typedef struct {
    char username[MAX_USERNAME_LENGTH];
    uint32_t client_version;
} connect_request_payload_t;

// 연결 응답 페이로드
typedef struct {
    response_code_t result;
    char message[256];        // 성공/실패 메시지
    uint32_t user_id;         // 성공 시 할당된 사용자 ID
} connect_response_payload_t;

// 인증 요청 페이로드
typedef struct {
    char username[MAX_USERNAME_LENGTH];
    char password[64];        // 추후 해시값으로 대체 예정
} auth_request_payload_t;

// 인증 응답 페이로드
typedef struct {
    response_code_t result;
    char session_token[64];   // 성공 시 세션 토큰
    char message[256];
} auth_response_payload_t;

// 채팅 메시지 페이로드
typedef struct {
    uint32_t sender_id;
    char sender_name[MAX_USERNAME_LENGTH];
    char message[MAX_MESSAGE_SIZE - sizeof(message_header_t) - 64]; // 남은 공간 활용
    time_t timestamp;
} chat_message_payload_t;

// 사용자 목록 응답 페이로드
typedef struct {
    uint32_t user_count;
    char users[][MAX_USERNAME_LENGTH];  // 가변 길이 배열
} user_list_payload_t;

// 오류 메시지 페이로드
typedef struct {
    response_code_t error_code;
    char error_message[512];
    uint32_t error_context;   // 추가 컨텍스트 정보
} error_payload_t;

// =============================================================================
// 메시지 생성 함수들
// =============================================================================

/**
 * 새 메시지 생성 (메모리 할당 포함)
 * @param type 메시지 타입
 * @param payload 페이로드 데이터
 * @param payload_size 페이로드 크기
 * @return 생성된 메시지 포인터 (실패 시 NULL)
 */
message_t* message_create(message_type_t type, const void* payload, uint32_t payload_size);

/**
 * 메시지 메모리 해제
 * @param msg 해제할 메시지
 */
void message_destroy(message_t* msg);

/**
 * 메시지 복사
 * @param src 원본 메시지
 * @return 복사된 메시지 (실패 시 NULL)
 */
message_t* message_clone(const message_t* src);

// =============================================================================
// 직렬화/역직렬화 함수들
// =============================================================================

/**
 * 메시지를 바이트 배열로 직렬화
 * @param msg 직렬화할 메시지
 * @param buffer 출력 버퍼
 * @param buffer_size 버퍼 크기
 * @return 실제 사용된 바이트 수 (실패 시 -1)
 */
int message_serialize(const message_t* msg, char* buffer, size_t buffer_size);

/**
 * 바이트 배열에서 메시지 역직렬화
 * @param buffer 입력 바이트 배열
 * @param buffer_size 버퍼 크기
 * @return 역직렬화된 메시지 (실패 시 NULL)
 */
message_t* message_deserialize(const char* buffer, size_t buffer_size);

/**
 * 메시지 헤더만 역직렬화 (페이로드 크기 확인용)
 * @param buffer 입력 바이트 배열
 * @param header 출력될 헤더
 * @return 성공 시 0, 실패 시 음수
 */
int message_deserialize_header(const char* buffer, message_header_t* header);

// =============================================================================
// 메시지 검증 함수들
// =============================================================================

/**
 * 메시지 유효성 검증
 * @param msg 검증할 메시지
 * @return 유효하면 1, 무효하면 0
 */
int message_validate(const message_t* msg);

/**
 * 메시지 헤더 유효성 검증
 * @param header 검증할 헤더
 * @return 유효하면 1, 무효하면 0
 */
int message_validate_header(const message_header_t* header);

// =============================================================================
// 편의 함수들 (특정 타입 메시지 생성)
// =============================================================================

/**
 * 연결 요청 메시지 생성
 * @param username 사용자명
 * @return 생성된 메시지
 */
message_t* message_create_connect_request(const char* username);

/**
 * 연결 응답 메시지 생성
 * @param result 결과 코드
 * @param message 메시지 내용
 * @param user_id 사용자 ID
 * @return 생성된 메시지
 */
message_t* message_create_connect_response(response_code_t result, const char* message, uint32_t user_id);

/**
 * 채팅 메시지 생성
 * @param sender_id 발신자 ID
 * @param sender_name 발신자 이름
 * @param content 메시지 내용
 * @return 생성된 메시지
 */
message_t* message_create_chat(uint32_t sender_id, const char* sender_name, const char* content);

/**
 * 오류 메시지 생성
 * @param error_code 오류 코드
 * @param error_message 오류 메시지
 * @return 생성된 메시지
 */
message_t* message_create_error(response_code_t error_code, const char* error_message);

// =============================================================================
// 디버깅 지원 함수들
// =============================================================================

/**
 * 메시지 타입을 문자열로 변환 (protocol.h에서 선언됨)
 * @param type 메시지 타입
 * @return 타입 문자열
 */
const char* message_type_to_string(message_type_t type);

/**
 * 메시지 내용을 로그용으로 출력
 * @param msg 출력할 메시지
 */
void message_print_debug(const message_t* msg);

/**
 * 메시지 크기 계산
 * @param msg 메시지
 * @return 전체 메시지 크기 (헤더 + 페이로드)
 */
size_t message_get_total_size(const message_t* msg);

#endif // MESSAGE_H