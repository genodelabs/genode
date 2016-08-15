#include "dummy/macros.h"

#include "VirtualBoxErrorInfoImpl.h"

HRESULT VirtualBoxErrorInfo::init(HRESULT, const GUID &, const char *,
                                  const Utf8Str &, IVirtualBoxErrorInfo *) DUMMY(E_FAIL)
