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

#include <base/printf.h>
#include <base/semaphore.h>
#include <base/sleep.h>

#include <thread_qt.h>

using namespace Genode;

extern int qt_main(int argc, char *argv[]);

class Main_thread : public Thread_qt
{
	protected:
		
		int _argc;
		char **_argv;
		Semaphore &_finished;
		int _result;
	
	public:

		Main_thread(int argc, char *argv[], Semaphore &finished) :
			Thread_qt("Qt main thread"),
			_argc(argc),
			_argv(argv),
			_finished(finished),
			_result(0) { }
		
		virtual void entry()
		{
			/* call the real main() function */
			_result = ::qt_main(_argc, _argv);
			
			_finished.up();
			
			sleep_forever();
		}
	
		int result() { return _result; }
};

#define qt_main main

int main(int argc, char *argv[])
{
//	PDBG("QT_MAIN_STACK_SIZE == %d", QT_MAIN_STACK_SIZE);
	
	Semaphore finished;
	
	Main_thread main_thread(argc, argv, finished);
	main_thread.set_stack_size(QT_MAIN_STACK_SIZE);
	main_thread.start();
	
	/* wait for the thread to finish */
	finished.down();
	
//	PDBG("main_thread finished");

	return main_thread.result();
}

#endif /* QT_MAIN_STACK_SIZE */
