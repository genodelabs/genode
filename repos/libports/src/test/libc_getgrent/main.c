/*
 * \brief  Libc group database test
 * \author Boin 
 * \date   2026-01-27
 */

/*
 * Copyright (C) 2026 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <sys/types.h>
#include <grp.h>
#include <stdlib.h>
#include <stdio.h>


void print_db(char const *label, struct group *db)
{
	if (db == NULL)
		printf("[%s] NULL\n", label);
	else
		printf("[%s] group:%s gid:%d \n",
		       label, db->gr_name, db->gr_gid);
	fflush(stderr);
}


int main(int argc, char **argv)
{
	static char buf[4096];

	struct group db;
	struct group *ptr;

	print_db("getgrent", getgrent());
	print_db("getgrent", getgrent());
	print_db("getgrent", getgrent());

	setgrent();
	getgrent_r(&db, buf, sizeof(buf), &ptr);
	print_db("getgrent_r", ptr);
	getgrent_r(&db, buf, sizeof(buf), &ptr);
	print_db("getgrent_r", ptr);
	getgrent_r(&db, buf, sizeof(buf), &ptr);
	print_db("getgrent_r", ptr);

	print_db("getgrnam root",  getgrnam("root"));
	print_db("getgrnam alice", getgrnam("alice"));
	print_db("getgrnam bob",   getgrnam("bob"));

	setgroupent(0);
	getgrnam_r("root", &db, buf, sizeof(buf), &ptr);
	print_db("getgrnam_r root", ptr);
	getgrnam_r("alice", &db, buf, sizeof(buf), &ptr);
	print_db("getgwnam_r alice", ptr);
	getgrnam_r("bob", &db, buf, sizeof(buf), &ptr);
	print_db("getgrnam_r bob", ptr);

	print_db("getgrgid 0", getgrgid(0));
	print_db("getgrgid 1", getgrgid(1));
	print_db("getgrgid 2", getgrgid(2));

	setgroupent(1);
	getgrgid_r(0, &db, buf, sizeof(buf), &ptr);
	print_db("getgrgid_r 0", ptr);
	getgrgid_r(1, &db, buf, sizeof(buf), &ptr);
	print_db("getgrgid_r 1", ptr);
	getgrgid_r(2, &db, buf, sizeof(buf), &ptr);
	print_db("getgrgid_r 2", ptr);

	endgrent();
	return 0;
}
