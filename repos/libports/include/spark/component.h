#ifndef _INCLUDE__SPARK__COMPONENT_H_
#define _INCLUDE__SPARK__COMPONENT_H_

#include <base/component.h>

namespace Spark { namespace Component {

   enum Result { EXIT, CONT };
	/**
	 * Run SPARK construct subprogram
	 */
	Result construct(void);
} }

Genode::Env *__genode_env;

extern "C" void adainit(void);
extern "C" void adafinal(void);

extern "C" void __gnat_runtime_initialize(void) { };
extern "C" void __gnat_runtime_finalize(void) { };
extern "C" int __ada_runtime_exit_status;

void Component::construct(Genode::Env &env) {
    __genode_env = &env;
    env.exec_static_constructors();
    __gnat_runtime_initialize();
    adainit();

   switch (Spark::Component::construct()) {
      case Spark::Component::EXIT:
         adafinal();
         __gnat_runtime_finalize();
         __genode_env->parent().exit(__ada_runtime_exit_status);
         break;
      case Spark::Component::CONT:
         /* component constructed, execute event loop */
         break;
   }
}

#endif /* _INCLUDE__SPARK__COMPONENT_H_ */
