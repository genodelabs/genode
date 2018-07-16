/*
 * \brief  Libc passwd database test
 * \author Emery Hemingway
 * \date   2018-07-16
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <sys/types.h>
#include <pwd.h>
#include <stdlib.h>
#include <stdio.h>


void print_db(char const *label, struct passwd *db)
{
	if (db == NULL)
		printf("[%s] NULL\n", label);
	else
		printf("[%s] user:%s uid:%d home:%s \n",
		       label, db->pw_name, db->pw_uid, db->pw_dir);
	fflush(stderr);
}


int main(int argc, char **argv)
{
	static char buf[4096];

	struct passwd db;
	struct passwd *ptr;

	print_db("getpwent", getpwent());
	print_db("getpwent", getpwent());
	print_db("getpwent", getpwent());

	setpwent();
	getpwent_r(&db, buf, sizeof(buf), &ptr);
	print_db("getpwent_r", ptr);
	getpwent_r(&db, buf, sizeof(buf), &ptr);
	print_db("getpwent_r", ptr);
	getpwent_r(&db, buf, sizeof(buf), &ptr);
	print_db("getpwent_r", ptr);

	print_db("getpwnam root",  getpwnam("root"));
	print_db("getpwnam alice", getpwnam("alice"));
	print_db("getpwnam bob",   getpwnam("bob"));

	setpassent(0);
	getpwnam_r("root", &db, buf, sizeof(buf), &ptr);
	print_db("getpwnam_r root", ptr);
	getpwnam_r("alice", &db, buf, sizeof(buf), &ptr);
	print_db("getpwnam_r alice", ptr);
	getpwnam_r("bob", &db, buf, sizeof(buf), &ptr);
	print_db("getpwnam_r bob", ptr);

	print_db("getpwuid 0", getpwuid(0));
	print_db("getpwuid 1", getpwuid(1));
	print_db("getpwuid 2", getpwuid(2));

	setpassent(1);
	getpwuid_r(0, &db, buf, sizeof(buf), &ptr);
	print_db("getpwuid_r 0", ptr);
	getpwuid_r(1, &db, buf, sizeof(buf), &ptr);
	print_db("getpwuid_r 1", ptr);
	getpwuid_r(2, &db, buf, sizeof(buf), &ptr);
	print_db("getpwuid_r 2", ptr);

	endpwent();
	return 0;
}
