#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

enum {
	BUF_SIZE     = 8UL*1024UL*1024UL,
	ITERATION    = 1024UL,
	TOTAL_MEM_KB = BUF_SIZE / 1024 * ITERATION,
};


template <typename Test>
void memcpy_test(void * dst = nullptr, void * src = nullptr,
                 size_t size = BUF_SIZE)
{
	void * const from_buf = src ? src : malloc(size);
	void * const to_buf   = dst ? dst : malloc(size);

	Test test;
	test.start();

	for (unsigned i = 0; i < ITERATION; i++)
		test.copy(to_buf, from_buf, BUF_SIZE);

	test.finished();

	if (!src) free(from_buf);
	if (!dst) free(to_buf);
}


static inline void *bytewise_memcpy(void *dst, const void *src, size_t size)
{
	char *d = (char *)dst, *s = (char *)src;

	/* copy eight byte chunks */
	for (size_t i = size >> 3; i > 0; i--, *d++ = *s++,
	                                       *d++ = *s++,
	                                       *d++ = *s++,
	                                       *d++ = *s++,
	                                       *d++ = *s++,
	                                       *d++ = *s++,
	                                       *d++ = *s++,
	                                       *d++ = *s++);

	/* copy left over */
	for (size_t i = 0; i < (size & 0x7); i++, *d++ = *s++);

	return dst;
}
