#ifndef nscore_h___
#define nscore_h___

#include "prtypes.h"

#define NS_IMETHOD_(type) virtual type
#define NS_IMETHOD NS_IMETHOD_(nsresult)

typedef PRUint32 nsresult;

#include "nsError.h"

#endif /* nscore_h___ */
