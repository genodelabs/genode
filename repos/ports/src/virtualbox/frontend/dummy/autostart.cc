#include <base/printf.h>

#include "VirtualBoxBase.h"
#include "AutostartDb.h"

static bool debug = false;

#define TRACE(X) \
	{ \
		if (debug) \
			PDBG(" called (%s) - eip=%p", __FILE__, \
			     __builtin_return_address(0)); \
		return X; \
	}

#define DUMMY(X) \
	{ \
		PERR("%s called (%s), not implemented, eip=%p", __func__, __FILE__, \
		     __builtin_return_address(0)); \
		while (1) \
			asm volatile ("ud2a"); \
		\
		return X; \
	}

int AutostartDb::addAutostartVM(const char *pszVMId)                            DUMMY(-1)
int AutostartDb::addAutostopVM(char const*)                                     DUMMY(-1)
int AutostartDb::removeAutostopVM(char const*)                                  DUMMY(-1)
int AutostartDb::removeAutostartVM(char const*)                                 DUMMY(-1)

AutostartDb::AutostartDb()                                                      TRACE()
AutostartDb::~AutostartDb()                                                     DUMMY()
int AutostartDb::setAutostartDbPath(char const*path)                            TRACE(VINF_SUCCESS)
