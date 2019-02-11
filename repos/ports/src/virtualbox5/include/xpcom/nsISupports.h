#ifndef __gen_nsISupports_h__
#define __gen_nsISupports_h__

#ifndef nsError_h__
#include "nsError.h"
#endif

#ifndef nsISupportsImpl_h__
#include "nsISupportsImpl.h"
#endif

class nsISupports
{
	public:

		/* make the class polymorphic, so it can be used with 'dynamic_cast' */
		virtual ~nsISupports() { }

		void AddRef() { }
		void Release() { }
};

#endif /* __gen_nsISupports_h__ */
