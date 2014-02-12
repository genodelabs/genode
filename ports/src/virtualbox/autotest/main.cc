#include <base/printf.h>
#include <base/thread.h>

#include <unistd.h>
#include <fcntl.h>

static char buf[256];

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
		write(fd_dst, buf, len);
		sum += len;
	}
	close(fd_src);
	close(fd_dst);

	printf("wrote %zu bytes to overlay.vdi - res=%d\n", sum, len);
	printf("vbox_auto_test_helper is done.\n");
	return 0;
}
