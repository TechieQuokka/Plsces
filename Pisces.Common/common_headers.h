// common_headers.h
#ifndef COMMON_HEADERS_H
#define COMMON_HEADERS_H

// Windows 매크로 정의 (반드시 첫 번째)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef _WINSOCKAPI_
#define _WINSOCKAPI_
#endif

// Windows 시스템 헤더 (순서 중요!)
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

// 표준 C 헤더
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>

// Visual Studio 전용
#ifdef _MSC_VER
#pragma comment(lib, "ws2_32.lib")
#endif

#endif // COMMON_HEADERS_H