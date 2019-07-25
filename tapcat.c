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
#endif
#include "tap-public.h"



int
main(int argc, char * argv[]) {
	const char * argv0, * name;
	int tap_fd = -1;
	int ret = 0;

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
		ret = execute_process(argc, argv, tap_fd);
		if (ret == -1) {
			perror("exec");
			return ret;
		}
	}

#ifndef _WIN32
	unsigned char buf[0x1000];
	struct pollfd pfds[2];

	memset(pfds, 0, sizeof pfds);

	pfds[0].fd = 0;
	pfds[0].events = POLLIN | POLLHUP;

	pfds[1].fd = tap_fd;
	pfds[1].events = POLLIN | POLLHUP;

	for (;;) {
		ret = poll(pfds, LEN(pfds), -1);
		if (ret < 0) {
			perror("poll");
			break;
		}

		if (pfds[0].revents & POLLIN) {
			ret = read(pfds[0].fd, buf, sizeof buf);
			all_write(tap_fd, buf, ret);
		}

		if (pfds[1].revents & POLLIN) {
			ret = read(pfds[1].fd, buf, sizeof buf);
			all_write(1, buf, ret);
		}

		if (pfds[0].revents & POLLHUP || pfds[1].revents & POLLHUP) {
			break;
		}
	}
#endif

	close_tap(tap_fd);

	return ret;
}
