/*
 * \brief  Wrapper for Ada main program using Terminal session
 * \author Alexander Senier
 * \date   2019-01-03
 */

/* Genode includes */
#include <spark/component.h>

extern "C" void _ada_main(void);

namespace Spark {
	Component::Result Component::construct() {
		_ada_main();
		return Component::EXIT;
	};
}
