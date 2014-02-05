/*
 * \brief  Call the main function of the dynamic program
 * \author Martin Stein
 * \date   2013-12-14
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

typedef void (*Func)(void);

int genode_atexit(Func);

extern "C" { void const ** get_program_var_addr(char const * name); }

extern char ** genode_argv;
extern int     genode_argc;
extern char ** genode_envp;

/**
 * Cast a given function pointer into a 'void (*)()' function pointer
 *
 * \param ptr  source function pointer
 */
template <typename Func_ptr>
static Func * func(Func_ptr ptr)
{
	return reinterpret_cast<Func *>(const_cast<void **>(ptr));
}

/**
 * Call the main function of the dynamic program
 *
 * \param main_ptr  raw pointer of program main-function
 *
 * \return  return value of the program main-function
 */
extern "C" int call_program_main(Func main_func)
{
	/* call constructors of global objects of the program */
	Func * const _ctors_end   = func(get_program_var_addr("_ctors_end"));
	Func * const _ctors_start = func(get_program_var_addr("_ctors_start"));
	for (Func * ctor = _ctors_end; ctor != _ctors_start; (*--ctor)());

	/* register global-object destructors of program at LDSO atexit-array */
	Func * const _dtors_end   = func(get_program_var_addr("_dtors_end"));
	Func * const _dtors_start = func(get_program_var_addr("_dtors_start"));
	for (Func * dtor = _dtors_start; dtor != _dtors_end; genode_atexit(*dtor++));

	/* call main function of the program */
	typedef int (*Main)(int, char **, char **);
	Main const main = reinterpret_cast<Main>(main_func);
	return main(genode_argc, genode_argv, genode_envp);
}
