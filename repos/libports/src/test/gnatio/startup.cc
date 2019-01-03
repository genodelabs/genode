/*
 * \brief  Wrapper for Ada main program using Terminal session
 * \author Alexander Senier
 * \date   2019-01-03
 */

/* Genode includes */
#include <base/component.h>
#include <terminal_session/connection.h>

extern "C" void _ada_main(void);

/* Required in runtime */
Terminal::Connection *__genode_terminal;

void Component::construct(Genode::Env &env)
{
   Terminal::Connection _terminal (env, "Ada");
   __genode_terminal = &_terminal;

	_ada_main();
	env.parent().exit(0);
}
