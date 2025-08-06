# 🐟 Pisces Chat System

**Windows 기반 멀티스레드 TCP 채팅 시스템**

Pisces Chat System은 C언어로 구현된 고성능 채팅 애플리케이션입니다. 클라이언트-서버 아키텍처를 기반으로 하며, 다중 사용자 실시간 채팅을 지원합니다.

## ✨ 주요 기능

### 🖥️ 서버
- **다중 클라이언트 지원** - 최대 64명의 동시 접속자
- **실시간 메시지 브로드캐스팅** - 모든 사용자에게 즉시 메시지 전달
- **사용자 인증 시스템** - 고유한 사용자명 기반 인증
- **연결 상태 관리** - 하트비트를 통한 자동 연결 감지
- **비동기 I/O 처리** - select() 기반 논블로킹 소켓
- **관리자 기능** - 서버 상태 모니터링 및 통계

### 💻 클라이언트
- **멀티스레드 아키텍처** - UI와 네트워크 처리 분리
- **콘솔 기반 UI** - 직관적인 텍스트 인터페이스
- **자동 재연결** - 네트워크 중단 시 자동 복구
- **명령어 시스템** - 다양한 채팅 명령어 지원
- **실시간 알림** - 사용자 입/퇴장 알림

## 🏗️ 시스템 구조

```
┌─────────────────┐    TCP/IP    ┌─────────────────┐
│   Chat Client   │ ◄─────────►  │   Chat Server   │
│                 │              │                 │
│ ┌─────────────┐ │              │ ┌─────────────┐ │
│ │ UI Thread   │ │              │ │Main Loop    │ │
│ │             │ │              │ │(select)     │ │
│ └─────────────┘ │              │ └─────────────┘ │
│ ┌─────────────┐ │              │ ┌─────────────┐ │
│ │Network      │ │              │ │Client       │ │
│ │Thread       │ │              │ │Manager      │ │
│ └─────────────┘ │              │ └─────────────┘ │
└─────────────────┘              └─────────────────┘
```

## 🚀 빠른 시작

### 시스템 요구사항
- **운영체제**: Windows 10/11 (64-bit)
- **컴파일러**: Visual Studio 2019 이상 또는 MinGW-w64
- **라이브러리**: Winsock2 (ws2_32.lib)

### 빌드 방법

#### Visual Studio 사용
1. `Pisces.Chat.sln` 솔루션 파일 열기
2. 빌드 구성을 `Release` 또는 `Debug`로 설정
3. **솔루션 빌드** (`Ctrl+Shift+B`)

#### 명령줄 빌드
```bash
# 프로젝트 디렉토리에서
msbuild Pisces.Chat.sln /p:Configuration=Release /p:Platform=x64
```

### 실행 방법

#### 서버 실행
```bash
# 기본 설정으로 서버 실행 (포트 8080)
./server.exe

# 사용자 지정 설정으로 실행
./server.exe -p 9000 -m 32 -v
```

#### 클라이언트 실행
```bash
# 기본 설정으로 클라이언트 실행
./client.exe

# 자동 연결 및 인증
./client.exe -s localhost -p 8080 -u "사용자명" -c -a
```

## 📖 사용법

### 서버 명령어
```bash
server.exe [옵션]

옵션:
  -p, --port <포트>          서버 포트 (기본값: 8080)
  -b, --bind <인터페이스>     바인드할 IP 주소 (기본값: 모든 인터페이스)
  -m, --max-clients <수>     최대 클라이언트 수 (기본값: 64)
  -v, --verbose              상세 로그 출력
  -q, --quiet                오류만 출력
  -h, --help                 도움말 표시
      --version              버전 정보 표시
```

### 클라이언트 명령어
```bash
client.exe [옵션]

옵션:
  -s, --server <호스트>       서버 주소 (기본값: localhost)
  -p, --port <포트>          서버 포트 (기본값: 8080)
  -u, --username <이름>      사용자명
  -c, --connect              시작 시 자동 연결
  -a, --auth                 자동 인증
  -v, --verbose              상세 로그 출력
  -q, --quiet                오류만 출력
      --no-colors            색상 출력 비활성화
  -h, --help                 도움말 표시
      --version              버전 정보 표시
```

### 채팅 명령어 (클라이언트 내에서 사용)
```
/connect <호스트> <포트>     서버에 연결
/auth <사용자명>            사용자 인증
/disconnect                 서버 연결 해제
/users                      온라인 사용자 목록 보기
/status                     클라이언트 상태 정보 출력
/clear                      채팅 히스토리 지우기
/help                       명령어 도움말
/quit                       클라이언트 종료
```

## 🔧 고급 설정

### 서버 설정
서버는 다음과 같은 주요 설정을 지원합니다:

```c
typedef struct {
    uint16_t port;                  // 서버 포트
    char bind_interface[16];        // 바인드 인터페이스
    int max_clients;                // 최대 클라이언트 수
    int select_timeout_ms;          // select 타임아웃
    int heartbeat_interval_sec;     // 하트비트 간격
    int client_timeout_sec;         // 클라이언트 타임아웃
    log_level_t log_level;          // 로그 레벨
    int enable_heartbeat;           // 하트비트 활성화
} server_config_t;
```

### 클라이언트 설정
```c
typedef struct {
    char server_host[256];          // 서버 호스트
    uint16_t server_port;           // 서버 포트
    char username[32];              // 사용자명
    int auto_reconnect;             // 자동 재연결
    int reconnect_interval;         // 재연결 간격
    int connect_timeout;            // 연결 타임아웃
    int heartbeat_timeout;          // 하트비트 타임아웃
} client_config_t;
```

## 📡 프로토콜 명세

### 메시지 구조
모든 메시지는 12바이트 헤더와 가변 길이 페이로드로 구성됩니다:

```c
typedef struct {
    uint32_t magic;           // 매직 넘버: 0x43484154 ("CHAT")
    uint16_t version;         // 프로토콜 버전: 1
    uint16_t type;            // 메시지 타입
    uint32_t payload_size;    // 페이로드 크기
} message_header_t;
```

### 메시지 타입
- **1000번대**: 시스템 메시지 (연결, 하트비트)
- **2000번대**: 인증 메시지
- **3000번대**: 채팅 메시지
- **4000번대**: 사용자 관리
- **9000번대**: 오류 메시지

### 통신 흐름
```
클라이언트                          서버
    │                                │
    ├── MSG_CONNECT_REQUEST ──────►  │
    │                                ├── 사용자명 검증
    │ ◄──── MSG_CONNECT_RESPONSE  ───┤
    │                                │
    ├── MSG_CHAT_SEND ─────────────► │
    │ ◄──── MSG_CHAT_BROADCAST  ─────┤ (모든 클라이언트)
    │                                │
    ├── MSG_USER_LIST_REQUEST ─────► │
    │ ◄──── MSG_USER_LIST_RESPONSE ──┤
    │                                │
    ├─◄ MSG_HEARTBEAT   ─────────────┤
    ├── MSG_HEARTBEAT_ACK ─────────► │
    │                                │
```

## 🔍 문제 해결

### 자주 발생하는 문제

**Q: 서버가 시작되지 않아요**
```
A: 포트가 이미 사용 중인지 확인하세요:
   netstat -an | findstr :8080
   다른 포트를 사용하려면: server.exe -p 9000
```

**Q: 클라이언트가 연결되지 않아요**
```
A: 1. 서버가 실행 중인지 확인
   2. 방화벽 설정 확인
   3. 올바른 서버 주소와 포트 확인
   4. 네트워크 연결 상태 확인
```

**Q: 메시지가 전달되지 않아요**
```
A: 1. 사용자 인증이 완료되었는지 확인 (/auth <사용자명>)
   2. 서버 로그에서 오류 메시지 확인
   3. 네트워크 연결 상태 확인 (/status 명령어)
```

### 디버깅

상세한 로그를 보려면 verbose 모드로 실행하세요:
```bash
# 서버 디버그 모드
server.exe -v

# 클라이언트 디버그 모드  
client.exe -v
```

## 🏃‍♂️ 성능

### 벤치마크
- **최대 동시 연결**: 64개 클라이언트
- **메시지 처리량**: 초당 약 1,000개 메시지
- **메모리 사용량**: 서버 약 2MB, 클라이언트 약 1MB
- **CPU 사용률**: 유휴 상태에서 1% 미만

### 최적화 팁
1. **서버**: 클라이언트 수가 많을 때는 select 타임아웃을 줄이세요
2. **클라이언트**: 자동 재연결 간격을 적절히 조정하세요
3. **네트워크**: 로컬 네트워크에서 최상의 성능을 보입니다

### 개발 가이드라인
- 코드 스타일: 기존 코드의 스타일을 따라주세요
- 주석: 새로운 기능에는 적절한 주석을 추가해주세요
- 테스트: 변경사항은 충분히 테스트해주세요

## 📝 라이선스

이 프로젝트는 MIT 라이선스 하에 배포됩니다. 자세한 내용은 `LICENSE` 파일을 참조하세요.

## 🎉 감사의 글

이 프로젝트는 다음 기술들을 사용합니다:
- **Winsock2**: Windows 소켓 프로그래밍
- **Windows API**: 콘솔 UI 및 스레드 관리
- **Visual Studio**: 개발 환경

---

**⭐ 이 프로젝트가 도움이 되었다면 Star를 눌러주세요!**

*마지막 업데이트: 2025년 8월*
