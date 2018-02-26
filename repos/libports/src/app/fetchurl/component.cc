/*
 * \brief  Native fetchurl utility for Nix
 * \author Emery Hemingway
 * \date   2016-03-08
 */

/*
 * Copyright (C) 2016-2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <timer_session/connection.h>
#include <os/path.h>
#include <base/attached_rom_dataspace.h>
#include <libc/component.h>
#include <base/log.h>

/* cURL includes */
#include <curl/curl.h>

/* Libc includes */
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>


static size_t write_callback(char   *ptr,
                             size_t  size,
                             size_t  nmemb,
                             void   *userdata)
{
	int *fd = (int*)userdata;
	return write(*fd, ptr, size*nmemb);
}


static int fetchurl(Genode::Xml_node config_node)
{
	Genode::String<256> url;
	Genode::Path<256>   path;
	CURLcode res = CURLE_OK;

	Libc::with_libc([&]() { curl_global_init(CURL_GLOBAL_DEFAULT); });

	bool verbose = config_node.attribute_value("verbose", false);

	config_node.for_each_sub_node("fetch", [&] (Genode::Xml_node node) {

		if (res != CURLE_OK) return;

		try {
			node.attribute("url").value(&url);
			node.attribute("path").value(path.base(), path.capacity());
		} catch (...) { Genode::error("error reading 'fetch' node"); return; }

		char const *out_path = path.base();

		/* create compound directories leading to the path */
		for (size_t sub_path_len = 0; ; sub_path_len++) {

			bool const end_of_path = (out_path[sub_path_len] == 0);
			bool const end_of_elem = (out_path[sub_path_len] == '/');

			if (end_of_path)
				break;

			if (!end_of_elem)
				continue;

			Genode::String<256> sub_path(Genode::Cstring(out_path, sub_path_len));

			/* skip '/' */
			sub_path_len++;

			/* if sub path is a directory, we are fine */
			struct stat sb;
			sb.st_mode = 0;
			stat(sub_path.string(), &sb);
			if (S_ISDIR(sb.st_mode))
				continue;

			/* create directory for sub path */
			if (mkdir(sub_path.string(), 0777) < 0) {
				Genode::error("failed to create directory ", sub_path);
				break;
			}
		}

		int fd = open(out_path, O_CREAT | O_RDWR);
		if (fd == -1) {
			switch (errno) {
			case EACCES:
				Genode::error("permission denied at ", out_path); break;
			case EEXIST:
				Genode::error(out_path, " already exists"); break;
			case EISDIR:
				Genode::error(out_path, " is a directory"); break;
			case ENOSPC:
				Genode::error("cannot create ", out_path, ", out of space"); break;
			default:
				Genode::error("creation of ", out_path, " failed (errno=", errno, ")");
			}
			res = CURLE_FAILED_INIT;
			return;
		}

		CURL *curl = curl_easy_init();
		if (!curl) {
			Genode::error("failed to initialize libcurl");
			res = CURLE_FAILED_INIT;
			return;
		}

		curl_easy_setopt(curl, CURLOPT_URL, url.string());
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, true);

		curl_easy_setopt(curl, CURLOPT_VERBOSE, verbose);
		curl_easy_setopt(curl, CURLOPT_NOSIGNAL, true);
		curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);

		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &fd);

		curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);

		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

		/* check for optional proxy configuration */
		try {
			Genode::String<256> proxy;
			node.attribute("proxy").value(&proxy);
			curl_easy_setopt(curl, CURLOPT_PROXY, proxy.string());
		} catch (...) { }

		Libc::with_libc([&]() {
			res = curl_easy_perform(curl);
			close(fd);

			if (res != CURLE_OK)
				Genode::error(curl_easy_strerror(res));

			curl_easy_cleanup(curl);
		});
	});

	curl_global_cleanup();

	Genode::warning("SSL certificates not verified");

	return res ^ CURLE_OK;
}

void Libc::Component::construct(Libc::Env &env)
{
	env.config([&env] (Genode::Xml_node config) {
		env.parent().exit( fetchurl(config) );
	});
}

/* dummies to prevent warnings printed by unimplemented libc functions */
extern "C" int   issetugid() { return 1; }
extern "C" pid_t getpid()    { return 1; }

