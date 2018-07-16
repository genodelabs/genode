/*
 * \brief  Password database operations
 * \author Emery Hemingway
 * \date   2018-07-15
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/log.h>
#include <util/string.h>

/* Libc includes */
#include <sys/types.h>
#include <pwd.h>
#include <stdlib.h>

/* local includes */
#include "libc_errno.h"
#include "task.h"

typedef Genode::String<128> Passwd_string;

struct Passwd_fields {
	Passwd_string name   { "root" };
	Passwd_string passwd { };
	unsigned long uid = 0;
	unsigned long gid = 0;
	unsigned long change = 0;
	Passwd_string clas   { };
	Passwd_string gecos  { };
	Passwd_string home   { "/" };
	Passwd_string shell  { };
	unsigned long expire = 0;
	unsigned long fields = 0;

	Passwd_fields() { }
};


static void _fill_passwd(struct passwd &db, Passwd_fields &fields)
{
	using namespace Genode;

	/* reset buffers */
	Genode::memset(&db, 0x00, sizeof(struct passwd));
	fields = Passwd_fields();

	try {
		Xml_node const passwd = Libc::libc_config().sub_node("passwd");

		fields.name   = passwd.attribute_value("name",   fields.name);
		fields.uid    = passwd.attribute_value("uid",   0UL);
		fields.gid    = passwd.attribute_value("gid",   0UL);
		fields.change = passwd.attribute_value("change",   0UL);
		fields.passwd = passwd.attribute_value("passwd", fields.passwd);
		fields.clas   = passwd.attribute_value("class",  fields.clas);
		fields.gecos  = passwd.attribute_value("gecos",  fields.gecos);
		fields.home   = passwd.attribute_value("home",   fields.home);
		fields.shell  = passwd.attribute_value("shell",  fields.shell);
		fields.expire = passwd.attribute_value("expire",   0UL);
		fields.fields = passwd.attribute_value("fields",   0UL);
	}

	catch (...) { }

	db.pw_name    = (char *)fields.name.string();
	db.pw_uid     = fields.uid;
	db.pw_gid     = fields.gid;
	db.pw_change  = fields.change;
	db.pw_passwd  = (char *)fields.passwd.string();
	db.pw_class   = (char *)fields.clas.string();
	db.pw_gecos   = (char *)fields.gecos.string();
	db.pw_dir     = (char *)fields.home.string();
	db.pw_shell   = (char *)fields.shell.string();
	db.pw_expire = fields.expire;
	db.pw_fields = fields.fields;
}


static int _fill_r(struct passwd *in,
                   char *buffer, size_t bufsize,
                   struct passwd **out)
{
	if (!in || !buffer) return Libc::Errno(EINVAL);
	if (bufsize < sizeof(Passwd_fields)) return Libc::Errno(ERANGE);

	Passwd_fields *fields = (Passwd_fields *)buffer;
	_fill_passwd(*in, *fields);
	*out = in;
	return 0;
}


static struct passwd *_fill_static()
{
	struct Passwd_storage {
		struct passwd pwd;
		Passwd_fields fields;
	};

	static Passwd_storage *db = nullptr;
	if (!db)
		db = (Passwd_storage *)malloc(sizeof(struct Passwd_storage));

	_fill_passwd(db->pwd, db->fields);
	return &db->pwd;
}


static int _passwd_index = 0;


extern "C" {

struct passwd *getpwent(void)
{
	return (_passwd_index++ == 0)
		? _fill_static()
		: NULL;
}


int getpwent_r(struct passwd *in,
               char *buffer, size_t bufsize,
               struct passwd **out)
{
	*out = NULL;
	return _passwd_index++ == 0
		? _fill_r(in, buffer, bufsize, out)
		: 0;
}


struct passwd *getpwnam(const char *login)
{
	struct passwd *result = _fill_static();
	return (Genode::strcmp(login, result->pw_name) == 0)
		? result : NULL;
}


int getpwnam_r(const char *login, struct passwd *in,
               char *buffer, size_t bufsize,
               struct passwd **out)
{
	int rc = _fill_r(in, buffer, bufsize, out);
	if (rc == 0 && *out && Genode::strcmp(login, in->pw_name) != 0)
		*out = NULL;
	return rc;
}


struct passwd *getpwuid(uid_t uid)
{
	struct passwd *result = _fill_static();
	return (uid == result->pw_uid)
		? result : NULL;
}


int getpwuid_r(uid_t uid, struct passwd *in,
               char *buffer, size_t bufsize,
               struct passwd **out)
{
	int rc = _fill_r(in, buffer, bufsize, out);
	if (rc == 0 && *out && uid != in->pw_uid)
		*out = NULL;
	return rc;
}


int setpassent(int) { _passwd_index = 0; return 0; }

void setpwent(void) { _passwd_index = 0; }

void endpwent(void) { }

} /* extern "C" */
