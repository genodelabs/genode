#include "VirtualBoxBase.h"

#include "dummy/macros.h"

#include <VBox/com/ErrorInfo.h>

static bool debug = false;


void ErrorInfo::init(bool aKeepObj) TRACE()
void ErrorInfo::cleanup()           TRACE()
HRESULT ErrorInfoKeeper::restore()  TRACE(S_OK)

void ErrorInfo::copyFrom(const ErrorInfo &x) DUMMY()
