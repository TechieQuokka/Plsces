// common_headers.h
#ifndef COMMON_HEADERS_H
#define COMMON_HEADERS_H

// Windows ��ũ�� ���� (�ݵ�� ù ��°)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef _WINSOCKAPI_
#define _WINSOCKAPI_
#endif

// Windows �ý��� ��� (���� �߿�!)
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

// ǥ�� C ���
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>

// Visual Studio ����
#ifdef _MSC_VER
#pragma comment(lib, "ws2_32.lib")
#endif

#endif // COMMON_HEADERS_H