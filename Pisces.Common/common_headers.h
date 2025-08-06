#ifndef COMMON_HEADERS_H
#define COMMON_HEADERS_H

// 한 번만 정의
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef _WINSOCKAPI_
#define _WINSOCKAPI_
#endif

// Windows 네트워크 헤더들을 올바른 순서로
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#endif // COMMON_HEADERS_H