/*
 * \brief  main() wrapper for customizing main()'s stack size  
 * \author Christian Prochaska
 * \date   2008-08-20
 */

/*
 * Copyright (C) 2008-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifdef QT_MAIN_STACK_SIZE

#include <base/thread.h>

using namespace Genode;

extern int qt_main(int argc, char *argv[]);

#define qt_main main

int main(int argc, char *argv[])
{
	Genode::Thread::myself()->stack_size(QT_MAIN_STACK_SIZE);
	
	return qt_main(argc, argv);
}

#endif /* QT_MAIN_STACK_SIZE */
