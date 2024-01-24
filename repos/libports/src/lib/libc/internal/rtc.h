/*
 * \brief  Interface for obtaining real-time clock values
 * \author Norman Feske
 * \date   2020-08-18
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _LIBC__INTERNAL__RTC_H_
#define _LIBC__INTERNAL__RTC_H_

#include <sys/time.h>

namespace Libc { struct Rtc; }


struct Libc::Rtc : Vfs::Watch_response_handler
{
	using Allocator = Genode::Allocator;

	Vfs_plugin &_vfs;

	Allocator &_alloc;

	Vfs::Vfs_watch_handle *_watch_handle { nullptr };

	Rtc_path const _rtc_path;

	Watch &_watch;

	time_t _rtc_value { 0 };

	bool _rtc_value_out_of_date { true };

	Milliseconds _msecs_when_rtc_updated { 0 };

	bool const _rtc_path_valid = (_rtc_path != "");

	void _update_rtc_value_from_file()
	{
		_vfs.with_root_dir([&] (Directory &root_dir) {
			try {
				File_content const content(_alloc, root_dir, _rtc_path.string(),
				                           File_content::Limit{4096U});
				content.bytes([&] (char const *ptr, size_t size) {

					char buf[32] { };
					::memcpy(buf, ptr, min(sizeof(buf) - 1, size));

					struct tm tm { };
					if (strptime(buf, "%Y-%m-%d %H:%M:%S", &tm)
					 || strptime(buf, "%Y-%m-%d %H:%M", &tm)) {
						_rtc_value = timegm(&tm);
						if (_rtc_value == (time_t)-1)
							_rtc_value = 0;
					}
				});
			} catch (...) {
				warning(_rtc_path, " not readable, returning ", _rtc_value);
			}
		});
	}

	Rtc(Vfs_plugin &vfs, Allocator &alloc, Rtc_path const &rtc_path, Watch &watch)
	:
		_vfs(vfs), _alloc(alloc), _rtc_path(rtc_path), _watch(watch)
	{
		if (!_rtc_path_valid) {
			warning("rtc not configured, returning ", _rtc_value);
			return;
		}

		_watch_handle = _watch.alloc_watch_handle(_rtc_path.string());

		if (_watch_handle)
			_watch_handle->handler(this);
	}


	/******************************************
	 ** Vfs::Watch_reponse_handler interface **
	 ******************************************/

	void watch_response() override
	{
		_rtc_value_out_of_date = true;
	}

	timespec read(Duration current_time)
	{
		struct timespec result { };

		if (!_rtc_path_valid)
			return result;

		if (_rtc_value_out_of_date) {
			_update_rtc_value_from_file();
			_msecs_when_rtc_updated = current_time.trunc_to_plain_ms();
			_rtc_value_out_of_date = false;
		}

		/*
		 * Return time as sum of cached RTC value and relative 'current_time'
		 */
		Milliseconds const current_msecs = current_time.trunc_to_plain_ms();

		Milliseconds const msecs_since_rtc_update {
			current_msecs.value - _msecs_when_rtc_updated.value };

		uint64_t const seconds_since_rtc_update =
			msecs_since_rtc_update.value / 1000;

		result.tv_sec  = _rtc_value + seconds_since_rtc_update;
		result.tv_nsec = (msecs_since_rtc_update.value % 1000) * (1000*1000);

		return result;
	}
};

#endif /* _LIBC__INTERNAL__RTC_H_ */
