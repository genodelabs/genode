/*
 * \brief  Simple test for lxip VFS plugin
 * \author Emery Hemingway
 * \author Josef Söntgen
 * \author Christian Helmuth
 * \date   2016-10-17
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>



static void ls_socket_fs(char const *path, bool top = true)
{
	if (top)
		printf("recursive listing of %s:\n", path);

	DIR *dp = opendir(path);
	if (dp == NULL) {
		perror("opendir");
		abort();
	}

	struct dirent *dent = NULL;
	while ((dent = readdir(dp))) {
		int d = dent->d_type == DT_DIR;
		printf("  %s  %s/%s\n", d ? "d" : "f", path, dent->d_name);

		if (d) {
			char subdir[128];
			snprintf(subdir, sizeof(subdir), "%s/%s", path, dent->d_name);
			ls_socket_fs(subdir, false);
		}
	}
	closedir(dp);
}


static int remove_sock_dir(char const *sock_root, char const *sock_fd)
{
	char sock_dir[64];
	snprintf(sock_dir, sizeof(sock_dir), "%s/%s", sock_root, sock_fd);
	return unlink(sock_dir);
}


static int recv_client(char const *sock_root, char const *sock_fd)
{
	char sock_data[96];
	snprintf(sock_data, sizeof(sock_data), "%s/%s/data", sock_root, sock_fd);

	int fd = open(sock_data, O_RDONLY);
	if (fd == -1) {
		perror("open");
		return -1;
	}

	char dst;
	ssize_t n = read(fd, &dst, 1);
	printf("receiving data from client %s\n",
	       n > 0 ? "successful" : "failed");

	close(fd);
	return 0;
}


static int send_client(char const *sock_root, char const *sock_fd,
                       char const *src, size_t len)
{
	char sock_data[96];
	snprintf(sock_data, sizeof(sock_data), "%s/%s/data", sock_root, sock_fd);

	int fd = open(sock_data, O_WRONLY);
	if (fd == -1) {
		perror("open");
		return -1;
	}

	ssize_t n = write(fd, src, len);
	printf("sending data to client %s\n",
	       n > 0 && (size_t)n == len ? "successful" : "failed");

	close(fd);
	return 0;
}


static void sock_info(char const *sock_root, char const *sock_fd)
{
	size_t n;

	char local[96];
	snprintf(local, sizeof(local), "%s/%s/local", sock_root, sock_fd);

	int fdl = open(local, O_RDONLY);
	if (fdl == -1) {
		perror("open");
		return;
	}

	n = read(fdl, local, sizeof(local));
	local[n-1] = '\0';
	printf("local: %s\n", local);
	close(fdl);

	char remote[96];
	snprintf(remote, sizeof(remote), "%s/%s/remote", sock_root, sock_fd);

	int fdr = open(remote, O_RDONLY);
	if (fdr == -1) {
		perror("open");
		return;
	}

	n = read(fdr, remote, sizeof(remote));
	remote[n-1] = '\0';
	printf("remote: %s\n", remote);
	close(fdr);
}


static int test_bind_accept(char const *sock_root, char const *sock_fd)
{
	ssize_t n;

	/* bind */
	char sock_bind[96];
	snprintf(sock_bind, sizeof(sock_bind), "%s/%s/bind", sock_root, sock_fd);

	int fdb = open(sock_bind, O_RDWR);
	if (fdb == -1) {
		perror("open");
		return -1;
	}

	char addr[] = "0.0.0.0:80";
	n = write(fdb, addr, sizeof(addr));
	printf("binding to: %s %s\n", addr, n > 0 ? "success" : "failed");
	close(fdb);

	char sock_listen[96];
	snprintf(sock_listen, sizeof(sock_listen), "%s/%s/listen", sock_root, sock_fd);

	int fdl = open(sock_listen, O_RDWR);
	if (fdl == -1) {
		perror("open");
		return -1;
	}

	char backlog[] = "5";
	n = write(fdl, backlog, sizeof(backlog));
	printf("listen backlog: %s %s\n", backlog, n > 0 ? "success" : "failed");
	close(fdl);


	/* accept */
	int res = 0;
	while (1) {
		char sock_accept[96];
		snprintf(sock_accept, sizeof(sock_accept), "%s/%s/accept", sock_root, sock_fd);

		ls_socket_fs(sock_root);

		int fda = open(sock_accept, O_RDWR);
		if (fda == -1) {
			perror("open");
			res = -1;
			break;
		}

		char client_fd[8] = { 0 };
		res = read(fda, client_fd, sizeof(client_fd));
		close(fda);

		if      (res  < 0) break;
		else if (res == 0) continue;

		client_fd[res-1] = '\0';

		printf("accept socket: %s\n", client_fd);

		sock_info(sock_root, client_fd);

		char hello[] = "hello w0rld!\n";
		recv_client(sock_root, client_fd);
		send_client(sock_root, client_fd, hello, sizeof(hello));
		remove_sock_dir(sock_root, client_fd);
	}

	return res;
}


static int test_connect_recv(char const *sock_root, char const *sock_fd)
{
	char sock_connect[96];
	snprintf(sock_connect, sizeof(sock_connect), "%s/%s/connect", sock_root, sock_fd);

	int fd = open(sock_connect, O_RDWR);
	if (fd == -1) {
		perror("open");
		return -1;
	}

	char host[] = "10.0.2.1:80";
	ssize_t n = write(fd, host, sizeof(host));
	(void)n;
	close(fd);
}


static void test_proto(char const *sock_root, char const *proto)
{
	char proto_root[64];
	snprintf(proto_root, sizeof(proto_root), "%s/%s", sock_root, proto);

	ls_socket_fs(proto_root);

	char new_socket_path[64];
	snprintf(new_socket_path, sizeof(new_socket_path), "%s/new_socket", proto_root);

	int fd = open(new_socket_path, O_RDONLY);
	if (fd == -1) {
		perror("open");
		abort();
	}

	char sock_path[16];
	size_t n = read(fd, sock_path, sizeof(sock_path));
	sock_path[n-1] = '\0';
	close(fd);

	ls_socket_fs(proto_root);

	char sock_dir[64];
	snprintf(sock_dir, sizeof(sock_dir), "%s/%s", sock_root, sock_path);

	ls_socket_fs(sock_dir);

	test_bind_accept(sock_root, sock_path);
	// test_connect_recv(proto_root, sock_fd);

	ls_socket_fs(sock_dir);

	remove_sock_dir(sock_root, sock_path);
}


int main()
{
	char const *socket_fs = "/socket";

	ls_socket_fs(socket_fs);
	test_proto(socket_fs, "tcp");
	test_proto(socket_fs, "udp");
	ls_socket_fs(socket_fs);
}
