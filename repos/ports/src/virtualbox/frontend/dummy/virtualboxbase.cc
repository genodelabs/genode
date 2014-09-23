#include <base/printf.h>

#include "VirtualBoxImpl.h"
#include "VBox/com/MultiResult.h"

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
		PERR("%s called (%s:%u), not implemented, eip=%p", __func__, __FILE__, __LINE__, \
		     __builtin_return_address(0)); \
		while (1) \
			asm volatile ("ud2a"); \
		\
		return X; \
	}

HRESULT VirtualBoxBase::setError(HRESULT aResultCode, const char *pcsz, ...)
{
	Genode::printf(ESC_ERR);

	va_list list;
	va_start(list, pcsz);

	Genode::printf("%s : %s", this->getComponentName(),
	               Utf8Str(pcsz, list).c_str());

	va_end(list);

	Genode::printf(ESC_END "\n");

	TRACE(aResultCode);
}

HRESULT VirtualBox::CreateAppliance(IAppliance**)                               DUMMY(E_FAIL)

void    VirtualBoxBase::clearError()                                            TRACE()
HRESULT VirtualBoxBase::setError(HRESULT aResultCode)                           DUMMY(E_FAIL)
HRESULT VirtualBoxBase::setError(const com::ErrorInfo &ei)                      DUMMY(E_FAIL)
HRESULT VirtualBoxBase::handleUnexpectedExceptions(VirtualBoxBase *const,
                                                   RT_SRC_POS_DECL)             TRACE(E_FAIL)
HRESULT VirtualBoxBase::setErrorInternal(HRESULT, GUID const&, char const*,
                                         com::Utf8Str, bool, bool)              DUMMY(E_FAIL)
HRESULT VirtualBoxBase::initializeComForThread(void)                            TRACE(S_OK)

HRESULT VirtualBoxErrorInfo::init(HRESULT, const GUID &, const char *,
                                  const Utf8Str &, IVirtualBoxErrorInfo *)      DUMMY(E_FAIL)

HRESULT VBoxEventDesc::init(IEventSource* aSource, VBoxEventType_T aType, ...)  TRACE(S_OK)
HRESULT VBoxEventDesc::reinit(VBoxEventType_T aType, ...)                       TRACE(S_OK)
