/*
 * \brief  Wrapper for the Ada main program
 * \author Norman Feske
 * \date   2009-09-23
 */

#include <spark/component.h>

/* local includes */
#include <machinery.h>

/**
 * Declaration of the Ada main procedure
 */
extern "C" void _ada_main(void);

namespace Spark {
Component::Result Component::construct() {
	   _ada_main();
	   test_spark_object_construction();
      return Component::EXIT;
   };
}
