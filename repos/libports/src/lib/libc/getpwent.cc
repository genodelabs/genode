/*
 * \brief  Password database operations
 * \author Norman Feske
 * \date   2019-09-20
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <util/string.h>
#include <util/xml_node.h>

/* libc includes */
#include <sys/types.h>
#include <pwd.h>
#include <stdlib.h>

/* libc-local includes */
#include <internal/legacy.h>
#include <internal/errno.h>
#include <internal/init.h>

namespace Libc {

	struct Passwd_fields;

	typedef String<128> Passwd_string;
}


/*
 * This struct must not contain any pointer because it is copied as
 * plain old data.
 */
struct Libc::Passwd_fields
{
	struct Buffer
	{
		char buf[Passwd_string::capacity()] { };

		Buffer(Passwd_string const &string)
		{
			Genode::strncpy(buf, string.string(), sizeof(buf));
		}
	};

	Buffer name;
	Buffer passwd;
	uid_t  uid;
	gid_t  gid;
	time_t change;
	Buffer clas;
	Buffer gecos;
	Buffer home;
	Buffer shell;
	time_t expire;
	int    fields;
};

using namespace Libc;

static Passwd_fields *_fields_ptr   = nullptr;
static passwd        *_passwd_ptr   = nullptr;
static int            _passwd_index = 0;


static passwd passwd_from_fields(Passwd_fields &fields)
{
	return passwd {
		.pw_name   = fields.name.buf,
		.pw_passwd = fields.passwd.buf,
		.pw_uid    = fields.uid,
		.pw_gid    = fields.gid,
		.pw_change = fields.change,
		.pw_class  = fields.clas.buf,
		.pw_gecos  = fields.gecos.buf,
		.pw_dir    = fields.home.buf,
		.pw_shell  = fields.shell.buf,
		.pw_expire = fields.expire,
		.pw_fields = fields.fields
	};
}


void Libc::init_passwd(Xml_node config)
{
	static Passwd_fields fields {
		.name   =         config.attribute_value("name",   Passwd_string("root")),
		.passwd =         config.attribute_value("passwd", Passwd_string("")),
		.uid    =         config.attribute_value("uid",    0U),
		.gid    =         config.attribute_value("gid",    0U),
		.change = (time_t)config.attribute_value("change", 0U),
		.clas   =         config.attribute_value("class",  Passwd_string("")),
		.gecos  =         config.attribute_value("gecos",  Passwd_string("")),
		.home   =         config.attribute_value("home",   Passwd_string("/")),
		.shell  =         config.attribute_value("shell",  Passwd_string("")),
		.expire = (time_t)config.attribute_value("expire", 0U),
		.fields =    (int)config.attribute_value("fields", 0U)
	};

	static passwd passwd = passwd_from_fields(fields);

	_fields_ptr = &fields;
	_passwd_ptr = &passwd;
}


extern "C" passwd *getpwent()
{
	struct Missing_call_of_init_passwd : Exception { };
	if (!_passwd_ptr)
		throw Missing_call_of_init_passwd();

	return (_passwd_index++ == 0) ? _passwd_ptr : nullptr;
}


template <typename COND_FN>
static int copy_out_pwent(passwd *in, char *buffer, size_t bufsize, passwd **out,
                          COND_FN const &cond_fn)
{
	*out = nullptr;

	struct Missing_call_of_init_passwd : Exception { };
	if (!_fields_ptr || !_passwd_ptr)
		throw Missing_call_of_init_passwd();

	if (bufsize < sizeof(Passwd_fields))
		return Errno(ERANGE);

	if (!cond_fn(*_passwd_ptr))
		return Errno(ENOENT);

	Passwd_fields &dst = *(Passwd_fields *)buffer;

	dst  = *_fields_ptr;
	*in  = passwd_from_fields(dst);
	*out = in;

	return 0;
}


extern "C" int getpwent_r(passwd *in, char *buffer, size_t bufsize, passwd **out)
{
	auto match = [&] (passwd const &) { return _passwd_index++ == 0; };

	return copy_out_pwent(in, buffer, bufsize, out, match);
}


extern "C" struct passwd *getpwnam(const char *login)
{
	return (_passwd_ptr && Genode::strcmp(login, _passwd_ptr->pw_name) == 0)
	      ? _passwd_ptr : nullptr;
}


extern "C" int getpwnam_r(char const *login, passwd *in,
                          char *buffer, size_t bufsize, passwd **out)
{
	auto match = [&] (passwd const &passwd) {
		return !Genode::strcmp(passwd.pw_name, login); };

	return copy_out_pwent(in, buffer, bufsize, out, match);
}


extern "C" passwd *getpwuid(uid_t uid)
{
	return (_passwd_ptr && uid == _passwd_ptr->pw_uid)
	      ? _passwd_ptr  : NULL;
}


extern "C" int getpwuid_r(uid_t uid, passwd *in,
                          char *buffer, size_t bufsize, passwd **out)
{
	auto match = [&] (passwd const &passwd) { return passwd.pw_uid == uid; };

	return copy_out_pwent(in, buffer, bufsize, out, match);
}


extern "C" int setpassent(int)
{
	_passwd_index = 0;
	return 0;
}


extern "C" void setpwent()
{
	_passwd_index = 0;
}


extern "C" void endpwent() { }
