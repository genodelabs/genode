/*
 * \brief  Test defintions common for Genode and Linux
 * \author Johannes Schlatow
 * \date   2022-03-07
 *
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__TEST__CACHE__COMMON_H_
#define _SRC__TEST__CACHE__COMMON_H_

template <typename TEST>
unsigned long timed_test(void * src, void * dst, size_t sz, unsigned iterations, TEST && func)
{
	Time s { };

	for (; iterations; iterations--)
		func(src, dst, sz);

	{
		Time e { } ;
		Duration d = Time::duration(s, e);
		return d.value;
	}
}


void touch_words(void * src, void *, size_t size)
{
	unsigned * data = reinterpret_cast<unsigned*>(src);
	for (size = size/sizeof(unsigned); size; size--)
		data[size]++;
}


template<size_t START_SZ_KB, size_t END_SZ_KB, typename TEST>
void sweep_test(void * src, void * dst, unsigned iterations, TEST && func)
{
	size_t size = START_SZ_KB;
	while (size <= END_SZ_KB)
	{
		func(src, dst, size*1024, iterations);

		size = size << 1;
	}
}


#endif /* _SRC__TEST__CACHE__COMMON_H_ */
