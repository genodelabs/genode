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
#include <os/reporter.h>
#include <libc/component.h>
#include <base/heap.h>
#include <base/log.h>

/* cURL includes */
#include <curl/curl.h>

/* Libc includes */
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>

namespace Fetchurl {
	class Fetch;
	struct Main;

	typedef Genode::String<256> Url;
	typedef Genode::Path<256>   Path;
}

static size_t write_callback(char   *ptr,
                             size_t  size,
                             size_t  nmemb,
                             void   *userdata);

static int progress_callback(void *userdata,
                             double dltotal, double dlnow,
                             double ultotal, double ulnow);


class Fetchurl::Fetch : Genode::List<Fetch>::Element
{
	friend class Genode::List<Fetch>;

	public:

		using Genode::List<Fetch>::Element::next;

		Main &main;

		Url  const url;
		Path const path;
		Url  const proxy;
		long       retry;

		double dltotal = 0;
		double dlnow = 0;

		int fd = -1;

		Fetch(Main &main, Url const &url, Path const &path,
		      Url const &proxy, long retry)
		:
			main(main), url(url), path(path),
			proxy(proxy), retry(retry+1)
		{ }
};


struct Fetchurl::Main
{
	Main(Main const &);
	Main &operator = (Main const &);

	Libc::Env &_env;

	Genode::Heap _heap { _env.pd(), _env.rm() };

	Timer::Connection _timer { _env, "reporter" };

	Genode::Constructible<Genode::Expanding_reporter> _reporter { };

	Genode::List<Fetch> _fetches { };

	Timer::One_shot_timeout<Main> _report_timeout {
		_timer, *this, &Main::_report };

	Genode::Duration _report_delay { Genode::Milliseconds { 0 } };

	void _schedule_report()
	{
		using namespace Genode;

		if ((_report_delay.trunc_to_plain_ms().value > 0) &&
		    (!_report_timeout.scheduled()))
		{
			_report_timeout.schedule(_report_delay.trunc_to_plain_us());
		}
	}

	void _report()
	{
		if (!_reporter.constructed())
			return;

		_reporter->generate([&] (Genode::Xml_generator &xml) {
			for (Fetch *f = _fetches.first(); f; f = f->next()) {
				xml.node("fetch", [&] {
					xml.attribute("url",   f->url);
					xml.attribute("total", f->dltotal);
					xml.attribute("now",   f->dlnow);
				});
			}
		});
	}

	void _report(Genode::Duration) { _report(); }

	void parse_config(Genode::Xml_node const &config_node)
	{
		using namespace Genode;

		try {
			enum { DEFAULT_DELAY_MS = 100UL };

			Xml_node const report_node = config_node.sub_node("report");
			if (report_node.attribute_value("progress", false)) {
				Milliseconds delay_ms { 0 };
				delay_ms.value = report_node.attribute_value(
					"delay_ms", (unsigned)DEFAULT_DELAY_MS);
				if (delay_ms.value < 1)
					delay_ms.value = DEFAULT_DELAY_MS;

				_report_delay = Duration(delay_ms);
				_schedule_report();
				_reporter.construct(_env, "progress", "progress");
			}
		}
		catch (...) { }

		auto const parse_fn = [&] (Genode::Xml_node node) {
			Url url;
			Path path;
			Url proxy;
			long retry;

			try {
				node.attribute("url").value(&url);
				node.attribute("path").value(path.base(), path.capacity());
			}
			catch (...) { Genode::error("error reading 'fetch' XML node"); return; }

			proxy = node.attribute_value("proxy", Url());
			retry = node.attribute_value("retry", 0L);

			auto *f = new (_heap) Fetch(*this, url, path, proxy, retry);
			_fetches.insert(f);
		};

		config_node.for_each_sub_node("fetch", parse_fn);
	}

	Main(Libc::Env &e) : _env(e)
	{
		_env.config([&] (Genode::Xml_node const &config) {
			parse_config(config);
		});
	}

	CURLcode _process_fetch(CURL *_curl, Fetch &_fetch)
	{
		Genode::log("fetch ", _fetch.url);

		char const *out_path = _fetch.path.base();

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
				return CURLE_FAILED_INIT;
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
			return CURLE_FAILED_INIT;
		}
		_fetch.fd = fd;

		curl_easy_setopt(_curl, CURLOPT_URL, _fetch.url.string());
		curl_easy_setopt(_curl, CURLOPT_FOLLOWLOCATION, true);

		curl_easy_setopt(_curl, CURLOPT_NOSIGNAL, true);
		curl_easy_setopt(_curl, CURLOPT_FAILONERROR, 1L);

		curl_easy_setopt(_curl, CURLOPT_WRITEFUNCTION, write_callback);
		curl_easy_setopt(_curl, CURLOPT_WRITEDATA, &_fetch);

		curl_easy_setopt(_curl, CURLOPT_NOPROGRESS, 0L);
		curl_easy_setopt(_curl, CURLOPT_PROGRESSFUNCTION, progress_callback);
		curl_easy_setopt(_curl, CURLOPT_PROGRESSDATA, &_fetch);

		curl_easy_setopt(_curl, CURLOPT_SSL_VERIFYPEER, 0L);
		curl_easy_setopt(_curl, CURLOPT_SSL_VERIFYHOST, 0L);

		/* check for optional proxy configuration */
		if (_fetch.proxy != "") {
			curl_easy_setopt(_curl, CURLOPT_PROXY, _fetch.proxy.string());
		}

		CURLcode res = curl_easy_perform(_curl);
		close(_fetch.fd);
		_fetch.fd = -1;

		if (res != CURLE_OK)
			Genode::error(curl_easy_strerror(res), ", failed to fetch ", _fetch.url);
		return res;
	}

	int run()
	{
		CURLcode exit_res = CURLE_OK;

		CURL *curl = curl_easy_init();
		if (!curl) {
			Genode::error("failed to initialize libcurl");
			return -1;
		}

		while (true) {
			_report();

			bool retry_some = false;
			for (Fetch *f = _fetches.first(); f; f = f->next()) {
				if (f->retry < 1) continue;
				CURLcode res = _process_fetch(curl, *f);
				if (res == CURLE_OK) {
					f->retry = 0;
				} else {
					if (--f->retry > 0)
						retry_some = true;
					else
						exit_res = res;
				}
			}
			if (!retry_some) break;
		}

		_report();

		curl_easy_cleanup(curl);

		return exit_res ^ CURLE_OK;
	}
};


static size_t write_callback(char   *ptr,
                             size_t  size,
                             size_t  nmemb,
                             void   *userdata)
{
	Fetchurl::Fetch &fetch = *((Fetchurl::Fetch *)userdata);
	return write(fetch.fd, ptr, size*nmemb);
}


static int progress_callback(void *userdata,
                             double dltotal, double dlnow,
                             double ultotal, double ulnow)
{
	(void)ultotal;
	(void)ulnow;

	Fetchurl::Fetch &fetch = *((Fetchurl::Fetch *)userdata);
	fetch.dltotal = dltotal;
	fetch.dlnow = dlnow;
	fetch.main._schedule_report();
	return CURLE_OK;
}


void Libc::Component::construct(Libc::Env &env)
{
	int res = -1;

	Libc::with_libc([&]() {
		curl_global_init(CURL_GLOBAL_DEFAULT);

		static Fetchurl::Main inst(env);
		res = inst.run();

		curl_global_cleanup();
	});

	env.parent().exit(res);
}

/* dummies to prevent warnings printed by unimplemented libc functions */
extern "C" int   issetugid() { return 1; }
extern "C" pid_t getpid()    { return 1; }

