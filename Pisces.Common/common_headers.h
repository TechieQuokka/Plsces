#ifndef COMMON_HEADERS_H
#define COMMON_HEADERS_H

// �� ���� ����
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef _WINSOCKAPI_
#define _WINSOCKAPI_
#endif

// Windows ��Ʈ��ũ ������� �ùٸ� ������
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#endif // COMMON_HEADERS_H