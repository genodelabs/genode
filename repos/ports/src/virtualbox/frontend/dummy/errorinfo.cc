#include <base/printf.h>

#include "VirtualBoxBase.h"
#include <VBox/com/ErrorInfo.h>

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

void ErrorInfo::init(bool aKeepObj) TRACE()
void ErrorInfo::cleanup()           TRACE()
HRESULT ErrorInfoKeeper::restore()  TRACE(S_OK)

void ErrorInfo::copyFrom(const ErrorInfo &x) DUMMY()

