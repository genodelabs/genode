/*
 * \brief  Utility for using the Linux chroot mechanism with Genode
 * \author Norman Feske
 * \date   2012-04-18
 */

/* Genode includes */
#include <os/config.h>
#include <base/sleep.h>

/* Linux includes */
#include <sys/stat.h>
#include <sys/mount.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>


enum { MAX_PATH_LEN = 256 };

static bool verbose = false;


/**
 * Return true if specified path is an existing directory
 */
static bool is_directory(char const *path)
{
	struct stat s;
	if (stat(path, &s) != 0)
		return false;

	if (!(s.st_mode & S_IFDIR))
		return false;

	return true;
}


static bool is_path_delimiter(char c) { return c == '/'; }


static bool has_trailing_path_delimiter(char const *path)
{
	char last_char = 0;
	for (; *path; path++)
		last_char = *path;

	return is_path_delimiter(last_char);
}


/**
 * Return number of path elements of given path
 */
static size_t num_path_elements(char const *path)
{
	size_t count = 0;

	/*
	 * If path starts with non-slash, the first characters belongs to a path
	 * element.
	 */
	if (*path && !is_path_delimiter(*path))
		count = 1;

	/* count slashes */
	for (; *path; path++)
		if (is_path_delimiter(*path))
			count++;

	return count;
}


static bool leading_path_elements(char const *path, unsigned num,
                                  char *dst, size_t dst_len)
{
	/* counter of path delimiters */
	unsigned count = 0;
	unsigned i     = 0;

	if (is_path_delimiter(path[0]))
		num++;

	for (; path[i] && (count < num) && (i < dst_len); i++)
	{
		if (is_path_delimiter(path[i]))
			count++;

		if (count == num)
			break;

		dst[i] = path[i];
	}

	if (i + 1 < dst_len) {
		dst[i] = 0;
		return true;
	}

	/* string is cut, append null termination anyway */
	dst[dst_len - 1] = 0;
	return false;
}


static void mirror_path_to_chroot(char const *chroot_path, char const *path)
{
	char target_path[MAX_PATH_LEN];
	Genode::snprintf(target_path, sizeof(target_path), "%s%s",
	                 chroot_path, path);

	/*
	 * Create directory hierarchy pointing to the target path except for the
	 * last element. The last element will be bind-mounted to refer to the
	 * original 'path'.
	 */
	for (unsigned i = 1; i <= num_path_elements(target_path); i++)
	{
		char buf[MAX_PATH_LEN];
		leading_path_elements(target_path, i, buf, sizeof(buf));

		/* skip existing directories */
		if (is_directory(buf))
			continue;

		/* create new directory */
		mkdir(buf, 0777);
	}

	umount(target_path);

	if (verbose) {
		PINF("bind mount from: %s", path);
		PINF("             to: %s", target_path);
	}

	if (mount(path, target_path, 0, MS_BIND, 0))
		PERR("bind mount failed (errno=%d: %s)", errno, strerror(errno));
}


int main(int, char **argv)
{
	using namespace Genode;

	static char     chroot_path[MAX_PATH_LEN];
	static char        cwd_path[MAX_PATH_LEN];
	static char genode_tmp_path[MAX_PATH_LEN];

	/*
	 * Read configuration
	 */
	try {
		Xml_node config = Genode::config()->xml_node();

		config.sub_node("root").attribute("path")
			.value(chroot_path, sizeof(chroot_path));

		verbose = config.attribute("verbose").has_value("yes");

	} catch (...) {
		PERR("invalid config");
		return 1;
	}

	getcwd(cwd_path, sizeof(cwd_path));

	uid_t const uid = getuid();
	snprintf(genode_tmp_path, sizeof(genode_tmp_path), "/tmp/genode-%d", uid);

	/*
	 * Print diagnostic information
	 */
	if (verbose) {
		PINF("work directory:  %s", cwd_path);
		PINF("chroot path:     %s", chroot_path);
		PINF("genode tmp path: %s", genode_tmp_path);
	}

	/*
	 * Validate chroot path
	 */
	if (!is_directory(chroot_path)) {
		PERR("chroot path does not point to valid directory");
		return 2;
	}

	if (has_trailing_path_delimiter(chroot_path)) {
		PERR("chroot path has trailing slash");
		return 3;
	}

	/*
	 * Hardlink directories needed for running Genode within the chroot
	 * environment.
	 */
	mirror_path_to_chroot(chroot_path, cwd_path);
	mirror_path_to_chroot(chroot_path, genode_tmp_path);

	printf("changing root to %s ...\n", chroot_path);

	if (chroot(chroot_path)) {
		PERR("chroot failed (errno=%d: %s)", errno, strerror(errno));
		return 4;
	}

	execve("init", argv, environ);

	PERR("execve failed with errno=%d", errno);
	return 0;
}
