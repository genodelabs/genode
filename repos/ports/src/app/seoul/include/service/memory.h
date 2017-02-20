/*
 * \brief  Memory header expected by Seoul
 * \author Markus Partheymueller
 * \date   2013-03-02
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

struct Aligned {
        size_t alignment;
        Aligned(size_t alignment) : alignment(alignment) {}
};

void *operator new[](size_t size);

void *operator new[](size_t size, Aligned const alignment);

void *operator new (size_t size);

void operator delete[](void *ptr);
