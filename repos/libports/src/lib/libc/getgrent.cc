/*
 * \brief  Group database operations
 * \author Boin
 * \date   2026-01-27
 */

/*
 * Copyright (C) 2026 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/node.h>

/* libc includes */
#include <sys/types.h>
#include <grp.h>
#include <stdlib.h>

/* libc-local includes */
#include <internal/errno.h>
#include <internal/init.h>

/* code adapted from existing getpwent implementations */

namespace Libc {

	struct Group_fields;

	using Group_string = String<128>;
}

/*
 * This struct must not contain any pointer because it is copied as
 * plain old data.
 */
struct Libc::Group_fields
{
	struct Buffer
	{
		char buf[Group_string::capacity()] { };

		Buffer(Group_string const &string)
		{
			copy_cstring(buf, string.string(), sizeof(buf));
		};
	};

	Buffer name;
	Buffer passwd;
	gid_t gid;
};

using namespace Libc;

static Group_fields   *_fields_ptr = nullptr;
static group          *_group_ptr  = nullptr;
static int            _group_index = 0;


static group group_from_fields(Group_fields &fields)
{
	return group {
		.gr_name   = fields.name.buf,
		.gr_passwd = fields.passwd.buf,
		.gr_gid    = fields.gid,
	};
}


void Libc::init_group(Node const &config)
{
	static Group_fields fields {
		.name   = config.attribute_value("name", Group_string("root")),
		.passwd = config.attribute_value("passwd", Group_string("")),
		.gid    = config.attribute_value("gid", 0U)
	};

	static group group = group_from_fields(fields);

	_fields_ptr = &fields;
	_group_ptr = &group;
}


extern "C" group *getgrent()
{
	struct Missing_call_of_init_group : Exception { };
	if (!_group_ptr)
		throw Missing_call_of_init_group();

	return (_group_index++ == 0) ? _group_ptr : nullptr;
}


template <typename COND_FN>
static int copy_out_grent(group *in, char *buffer, size_t bufsize, group **out,
                          COND_FN const &cond_fn)
{
	*out = nullptr;

	struct Missing_call_of_init_group : Exception { };
	if (!_fields_ptr || !_group_ptr)
		throw Missing_call_of_init_group();

	if (bufsize < sizeof(Group_fields))
		return Errno(ERANGE);

	if (!cond_fn(*_group_ptr))
		return Errno(ENOENT);

	Group_fields &dst = *(Group_fields *)buffer;

	dst  = *_fields_ptr;
	*in  = group_from_fields(dst);
	*out = in;

	return 0;
}


extern "C" int getgrent_r(group *in, char *buffer, size_t bufsize, group **out)
{
	auto match = [&] (group const &) { return _group_index++ == 0; };

	return copy_out_grent(in, buffer, bufsize, out, match);
}


extern "C" struct group *getgrnam(const char *group)
{
	return (_group_ptr && Genode::strcmp(group, _group_ptr->gr_name) == 0)
	      ? _group_ptr : nullptr;
}


extern "C" int getgrnam_r(char const *login, group *in,
                          char *buffer, size_t bufsize, group **out)
{
	auto match = [&] (group const &group) {
		return !Genode::strcmp(group.gr_name, login); };

	return copy_out_grent(in, buffer, bufsize, out, match);
}


extern "C" group *getgrgid(gid_t gid)
{
	return (_group_ptr && gid == _group_ptr->gr_gid)
	      ? _group_ptr  : NULL;
}


extern "C" int getgrgid_r(gid_t gid, group *in,
                          char *buffer, size_t bufsize, group **out)
{
	auto match = [&] (group const &group) { return group.gr_gid == gid; };

	return copy_out_grent(in, buffer, bufsize, out, match);
}


extern "C" int setgroupent(int)
{
	_group_index = 0;
	return 0;
}


extern "C" void setgrent()
{
	_group_index = 0;
}

extern "C" void endgrent() { }
