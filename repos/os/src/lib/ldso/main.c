/*
 * \brief  libc startup code
 * \author Sebastian Sumpf
 * \date   2009-10-26
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <dlfcn.h>
#include <fcntl.h>
#include <rtld.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/param.h>
#include "file.h"

typedef void (*func_ptr_type)();

func_ptr_type
_rtld(Elf_Addr *sp, func_ptr_type *exit_proc, Obj_Entry **objp);

extern char **lx_environ;

int call_program_main(void (*main_func)(void));


static void *setup_stack(const char *name, long fd)
{
	char **p;
	int env_count = 0;
	char *env_end = '\0';

	if (!lx_environ) {
		lx_environ = &env_end;
	}

	for (p = lx_environ; *p; p++, env_count++) ;
	env_count++;

	long sp_argc = 1;
	const char *sp_argv[] = { name, '\0' };
	char *sp_at[] = {
	         (char*)AT_BASE,                //map base of ldso
	         (char*)LINK_ADDRESS,
	         (char*)AT_EXECFD,              //file handle of program to load
	         (char*)fd,
	         (char*)AT_NULL,                //AT terminator
	};

	void *sp = malloc(sizeof(sp_argc) + sizeof(sp_argv) + (env_count * sizeof(long))
	                  + sizeof(sp_at));
	void *sp_tmp = sp;

	//build dummy stack
	memcpy(sp_tmp, &sp_argc, sizeof(sp_argc));  //argc
	sp_tmp += sizeof(sp_argc);
	memcpy(sp_tmp, sp_argv, sizeof(sp_argv));  //argv
	sp_tmp += sizeof(sp_argv);
	memcpy(sp_tmp, lx_environ, env_count * sizeof(long)); //environ
	sp_tmp += env_count * sizeof(long);
	memcpy(sp_tmp, sp_at, sizeof(sp_at));  //auxiliary vector

	return sp;
}


int main(int argc, char **argv)
{
	char *binary = "binary";
	char binary_buf[64];

	Obj_Entry *objp;
	func_ptr_type exit_proc;

	int fd = open(binary, O_RDONLY);

	/* find file name of file descriptor */
	if (!find_binary_name(fd, binary_buf, sizeof(binary_buf)))
		binary = binary_buf;

	/* build dummy stack */
	void *sp =  setup_stack(binary, (long)fd);
	func_ptr_type const program_main = _rtld(sp, &exit_proc, &objp);

	/* call main function of dynamic program */
	return call_program_main(program_main);
}
