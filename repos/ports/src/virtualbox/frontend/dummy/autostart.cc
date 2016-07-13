#include "VirtualBoxBase.h"

#include "dummy/macros.h"

#include "AutostartDb.h"

static bool debug = false;


int AutostartDb::addAutostartVM(const char *pszVMId)                            DUMMY(-1)
int AutostartDb::addAutostopVM(char const*)                                     DUMMY(-1)
int AutostartDb::removeAutostopVM(char const*)                                  DUMMY(-1)
int AutostartDb::removeAutostartVM(char const*)                                 DUMMY(-1)

AutostartDb::AutostartDb()                                                      TRACE()
AutostartDb::~AutostartDb()                                                     DUMMY()
int AutostartDb::setAutostartDbPath(char const*path)                            TRACE(VINF_SUCCESS)
