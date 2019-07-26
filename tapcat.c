#ifndef _WIN32
#include <linux/if.h>
#include <linux/if_tun.h>
#endif
#include <errno.h>
#include <fcntl.h>
#ifndef _WIN32
#include <poll.h>
#endif
#include <stdio.h>
#include <string.h>
#ifndef _WIN32
#include <sys/ioctl.h>
#include <unistd.h>
#else
#include <WinSock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#endif
#include "tap-public.h"



#ifdef _WIN32

static int _sock_init(const char *Host, const char *Service, SOCKET *Socket)
{
	int ret = 0;
	WSADATA wsaData;
	struct addrinfo *result = NULL;
	struct addrinfo *ptr = NULL;
	struct addrinfo hints;
	SOCKET tmpSocket = INVALID_SOCKET;

	ret = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (ret == 0) {
		memset(&hints, 0, sizeof(hints));
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;
		ret = getaddrinfo(Host, Service, &hints, &result);
		if (ret == 0) {
			for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {
				tmpSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
				if (tmpSocket != INVALID_SOCKET) {
					ret = connect(tmpSocket, ptr->ai_addr, ptr->ai_addrlen);
					if (ret == 0) {
						*Socket = tmpSocket;
						break;
					} else ret = GetLastError();
				
					if (ret != 0)
						closesocket(tmpSocket);
				} else ret = GetLastError();

				if (ret != 0)
					break;
			}



			freeaddrinfo(result);
		} else ret = GetLastError();

		if (ret != 0)
			WSACleanup();
	}

	return ret;
}



static void _sock_finit(SOCKET Socket)
{
	closesocket(Socket);
	WSACleanup();

	return;
}

#endif


int
main(int argc, char * argv[]) {
	const char * argv0, * name;
	int tap_fd = 0;
	int ret = 0;
	SOCKET anotherFD = -1;

	argv0 = *argv; argv++; argc--;

	if (argc < 1) {
		fprintf(stderr, "%s tap-device [cmd [...]]\n", argv0);
		return 1;
	}

	name = *argv; argv++; argc--;

	ret = open_tap(name, &tap_fd);
	if (ret != 0) {
		perror("cannot open tap");
		return ret;
	}

	if (*argv) {
//		ret = execute_process(argc, argv, tap_fd);
//		if (ret == -1) {
//			perror("exec");
//			return ret;
//		}

#ifdef _WIN32
		ret = _sock_init("192.168.10.254", "1234", &anotherFD);
		if (ret != 0) {
			fprintf(stderr, "_sock_init: %u\n", ret);
			return ret;
		}
#endif
	}

	char buf[0x1000];
#ifdef _WIN32
	WSAPOLLFD pfds[2];
#else
	struct pollfd pfds[2];
#endif

	memset(pfds, 0, sizeof pfds);

	pfds[0].fd = anotherFD;
	pfds[0].events = POLLIN;

	pfds[1].fd = tap_fd;
	pfds[1].events = POLLIN;

	for (;;) {
#ifdef _WIN32
		ret = WSAPoll(pfds, 2, -1);
#else
		ret = poll(pfds, LEN(pfds), -1);
#endif
		if (ret < 0) {
			ret = GetLastError();
			fprintf(stderr, "poll: %u\n", ret);
			break;
		}

		if (pfds[0].revents & POLLIN) {
			ret = recv(pfds[0].fd, buf, sizeof buf, 0);
			if (ret > 0)
				send(pfds[1].fd, buf, ret, 0);
		}

		if (pfds[1].revents & POLLIN) {
			ret = recv(pfds[1].fd, buf, sizeof buf, 0);
			if (ret > 0)
				send(pfds[0].fd, buf, ret, 0);
		}

		if (pfds[0].revents & POLLHUP || pfds[1].revents & POLLHUP) {
			break;
		}
	}

#ifdef _WIN32
	_sock_finit(anotherFD);
#endif

	close_tap(tap_fd);

	return ret;
}
