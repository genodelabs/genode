/*
 * \brief  Utility for parsing a depot URL into download location and user name
 * \author Norman Feske
 * \date   2023-03-20
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _MODEL__DEPOT_URL_H_
#define _MODEL__DEPOT_URL_H_

#include <types.h>

namespace Sculpt { struct Depot_url; }


struct Sculpt::Depot_url
{
	using Url  = String<128>;
	using User = Depot::Archive::User;

	Url  download;  /* download location w/o user sub dir */
	User user;      /* name of user sub dir */

	template <size_t N>
	static Depot_url from_string(String<N> const &url)
	{
		Url  download { };
		User user     { };

		auto for_each_slash_pos = [&] (auto const &fn)
		{
			for (size_t i = 0; i < url.length(); i++)
				if (url.string()[i] == '/')
					fn(i);
		};

		unsigned num_slashes    = 0;
		size_t   protocol_len   = 0;
		size_t   last_slash_pos = 0;

		using Protocol = String<16>;  /* "http://" or "https://" */
		Protocol protocol { };

		for_each_slash_pos([&] (size_t i) {
			num_slashes++;
			if (num_slashes == 2) {
				protocol_len = i + 1;
				protocol = { Cstring(url.string(), protocol_len) };
			}
			if (num_slashes > 2)
				last_slash_pos = i;
		});

		if (protocol_len && last_slash_pos > protocol_len) {
			download = { Cstring(url.string(), last_slash_pos) };
			user     = { Cstring(url.string() + last_slash_pos + 1) };
		}

		bool const valid = (protocol == "http://" || protocol == "https://")
		                && (user.length() > 1);

		return valid ? Depot_url { .download = download, .user = user }
		             : Depot_url { };
	}

	bool valid() const { return download.valid() && user.valid(); }
};

#endif /* _MODEL__DEPOT_URL_H_ */
