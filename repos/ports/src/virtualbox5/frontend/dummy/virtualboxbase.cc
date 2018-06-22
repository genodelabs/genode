#include <base/log.h>

#include "VirtualBoxImpl.h"
#include "VBox/com/MultiResult.h"
#include "iprt/cpp/exception.h"

#include "dummy/macros.h"

static bool debug = false;

HRESULT VirtualBoxBase::setError(HRESULT aResultCode, const char *pcsz, ...)
{
	va_list list;
	va_start(list, pcsz);

	Genode::error(this->getComponentName(), " : ", Utf8Str(pcsz, list).c_str());

	va_end(list);

	TRACE(aResultCode);
}

HRESULT VirtualBox::createAppliance(ComPtr<IAppliance> &aAppliance)             DUMMY(E_FAIL)

HRESULT VirtualBoxBase::setErrorBoth(HRESULT, int)                              DUMMY(E_FAIL)
HRESULT VirtualBoxBase::setErrorBoth(HRESULT, int, const char *, ...)           DUMMY(E_FAIL)
HRESULT VirtualBoxBase::setErrorVrc(int)                                        DUMMY(E_FAIL)
HRESULT VirtualBoxBase::setErrorVrc(int, char const*, ...)                      DUMMY(E_FAIL)
HRESULT VirtualBoxBase::setErrorNoLog(HRESULT, const char *, ...)               DUMMY(E_FAIL)

void    VirtualBoxBase::clearError()                                            TRACE()
HRESULT VirtualBoxBase::setError(HRESULT aResultCode)                           DUMMY(E_FAIL)
HRESULT VirtualBoxBase::setError(const com::ErrorInfo &ei)                      DUMMY(E_FAIL)
HRESULT VirtualBoxBase::setErrorInternal(HRESULT, GUID const&, char const*,
                                         com::Utf8Str, bool, bool)              DUMMY(E_FAIL)
HRESULT VirtualBoxBase::initializeComForThread(void)                            TRACE(S_OK)
void    VirtualBoxBase::uninitializeComForThread(void)                          TRACE()

HRESULT VirtualBoxBase::handleUnexpectedExceptions(VirtualBoxBase *const, RT_SRC_POS_DECL)
{
    try { throw; }
    catch (const RTCError &err) { Genode::error(err.what()); }
    catch (const std::exception &err) { Genode::error(err.what()); }
    catch (...) { Genode::error("An unexpected exception occurred"); }

    return E_FAIL;
}
