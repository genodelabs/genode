/*
 * \brief  Test Ada.Real_Time
 * \author Alexander Senier
 * \date   2019-03-05
 */

#include <spark/component.h>
#include <timer_session/connection.h>

extern "C" int sleep (int seconds)
{
   Timer::Connection timer (*__genode_env);
	timer.msleep(seconds * 1000);
   return 0;
}

extern "C" void _ada_real_time(void);

namespace Spark {
	Component::Result Component::construct() {
		_ada_real_time();
		return Component::EXIT;
	};
}
