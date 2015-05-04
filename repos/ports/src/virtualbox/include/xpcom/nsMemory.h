#ifndef nsMemory_h__
#define nsMemory_h__

#include <stddef.h>

class nsMemory
{
	public:
		static void * Alloc(size_t size);
		static void * Realloc(void* ptr, size_t size);
		static void   Free(void* ptr);
		static void * Clone(const void* ptr, size_t size);
};

#endif /* nsMemory_h__ */
