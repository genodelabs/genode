/* tap device loopback test using FreeBSD API */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <netdb.h>

#include <net/if.h>
#include <net/if_tap.h>

#include <sys/ioctl.h>

int main(int argc, char** argv)
{
	enum { BUFFLEN = 1500 };

	int fd = open("/dev/tap0", O_RDWR);
	if (fd == -1) {
		printf("Error: open(/dev/tap0) failed\n");
		return 1;
	}

	struct ifreq ifr;
	memset(&ifr, 0, sizeof(ifr));
	if (ioctl(fd, TAPGIFNAME, (void *)&ifr) < 0) {
		printf("Error: TAPGIFNAME failed\n");
		return 2;
	}
	printf("Successfully opened device %s\n", ifr.ifr_name);

	/* get mac address */
	char mac[6];
	memset(mac, 0, sizeof(mac));
	if (ioctl(fd, SIOCGIFADDR, (void *)mac) < 0) {
		printf("Error: SIOCGIFADDR failed\n");
		return 3;
	}

	/**
	 * Set mac address if we are in uplink mode.
	 * In Uplink mode, the default mac address is 0x02 02 02 02 02 02.
	 * In Nic mode, the nic_router will assign 0x02 02 02 02 02 00 to the first
	 * client.
	 */
	if (mac[5] >= 0x02) {
		mac[5]++;
		if (ioctl(fd, SIOCSIFADDR, (void *)mac) < 0) {
			printf("Error: SIOCSIFADDR failed\n");
			return 4;
		}
	}

	/* try other ioctls */
	struct tapinfo info;
	memset(&info, 0, sizeof(info));
	if (ioctl(fd, TAPGIFINFO, (void *)&info) < 0)
		printf("Warning: TAPGIFINFO failed\n");

	char buffer[BUFFLEN];
	unsigned frame_cnt = 0;
	while (frame_cnt < 2) {
		/* read a frame */
		ssize_t received = read(fd, buffer, BUFFLEN);
		if (received < 0)
			return 1;

		/* write a frame */
		ssize_t written = write(fd, buffer, received);
		if (written < received) {
			printf("Unable to write frame %d\n", frame_cnt);
			return 1;
		}
		frame_cnt++;
	}

	close(fd);

	return 0;
}
