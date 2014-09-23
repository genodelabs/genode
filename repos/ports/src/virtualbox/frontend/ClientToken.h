
#ifndef ____H_CLIENTTOKEN
#define ____H_CLIENTTOKEN

#include <VBox/com/ptr.h>
#include <VBox/com/AutoLock.h>

#include "MachineImpl.h"

class Machine::ClientToken
{
	public:
		ClientToken(const ComObjPtr<Machine> &pMachine, SessionMachine *pSessionMachine);

		bool isReady();
		bool release();
		void getId(Utf8Str &strId);
};
#endif /* !____H_CLIENTTOKEN */
