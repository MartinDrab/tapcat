#include <linux/if.h>
#include <linux/if_tun.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include "tap-public.h"






#define TUN_DEV "/dev/net/tun"

#define LEN(x) (sizeof x / sizeof * x)

int
all_write(int fd, const unsigned char * buf, int len) {
	register int ret;

	while (len) {
		ret = write(fd, buf, len);
		if (ret == -1) {
			if (errno == EINTR) continue;
			return ret;
		}
		buf += ret;
		len -= ret;
	}
	return 0;
}


int
open_tap(const char * dev, int *device_handle) {
	struct ifreq ifr;
	int fd = -1;
	int err = 0;

	fd = open(TUN_DEV, O_RDWR | O_CLOEXEC);
	if (fd < 0) {
		err = errno;
		perror("Cannot open " TUN_DEV);
		return err;
	}

	memset(&ifr, 0, sizeof ifr);

	ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
	if (*dev) {
		strncpy(ifr.ifr_name, dev, IFNAMSIZ);
	}

	if ((err = ioctl(fd, TUNSETIFF, &ifr)) < 0) {
		err = errno;
		perror("ioctl");
		close(fd);
		return err;
	}

	*device_handle = fd;

	return err;
}


void
close_tap(int device_handle)
{
	close(device_handle);

	return;
}


int
execute_process(int argc, const char *argv[], int TAPHandle)
{
	int ret = 0;
	dup2(TAPHandle, 0);
	dup2(TAPHandle, 1);
	ret = execvp(*argv, argv);

	return ret;
}
