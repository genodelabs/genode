#include <base/printf.h>
#include <base/thread.h>

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

static char buf[128 * 1024];

int main(int argc, char **argv)
{
	using Genode::printf;

	int res = unlink("/ram/overlay.vdi");
	printf("unlink result %d\n", res);

	int fd_src = open("/ram/overlay-original.vdi", O_RDONLY);
	int fd_dst = creat("/ram/overlay.vdi", O_CREAT);
	size_t len, sum = 0;

	printf("fd_src %d fd_dst %d\n", fd_src, fd_dst);
	if (fd_src < 0 || fd_dst < 0)
		return 1;

	while ((len = read(fd_src, buf, sizeof(buf))) > 0) {
		while (len) {
			ssize_t written = write(fd_dst, buf, len);
			len -= written;
			sum += written;

			if (written > 0 && len >= 0)
				continue;

			PERR("could not write whole file - %zu %zu %d\n",
			     sum, written, errno);
			return -1;
		}
	}
	close(fd_src);
	close(fd_dst);

	printf("wrote %zu bytes to overlay.vdi - res=%zu\n", sum, len);
	printf("vbox_auto_test_helper is done.\n");
	return 0;
}
