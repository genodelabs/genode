/*
 * \brief  libgcc contrib code for lx-hybrid components
 * \author Christian Helmuth
 * \date   2019-05-21
 *
 * GCC version 7 and above generate calls to __divmoddi for 64bit integer
 * division on 32-bit. Unfortunately, libgcc liberaries of older compilers
 * lack this symbol and are still in use by Debian/Ubuntu LTS at least.
 *
 * Implementation is similar to GCC 8.3 libgcc/libgcc2.c and libgcc/libgcc2.h -
 * original license header follows.
 */
/* Copyright (C) 1989-2016 Free Software Foundation, Inc.

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 3, or (at your option) any later
version.

GCC is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

Under Section 7 of GPL version 3, you are granted additional
permissions described in the GCC Runtime Library Exception, version
3.1, as published by the Free Software Foundation.

You should have received a copy of the GNU General Public License and
a copy of the GCC Runtime Library Exception along with this program;
see the files COPYING3 and COPYING.RUNTIME respectively.  If not, see
<http://www.gnu.org/licenses/>.  */

#ifdef __i386__
typedef int          SItype  __attribute__ ((mode (SI)));
typedef int          DItype  __attribute__ ((mode (DI)));
typedef unsigned int UDItype __attribute__ ((mode (DI)));
typedef SItype  Wtype;
typedef DItype  DWtype;
typedef UDItype UDWtype;
struct DWstruct { Wtype low, high; };
typedef union { struct DWstruct s; DWtype ll; } DWunion;

extern UDWtype __udivmoddi4 (UDWtype, UDWtype, UDWtype *);

DWtype __divmoddi4 (DWtype u, DWtype v, DWtype *rp)
{
  Wtype c1 = 0, c2 = 0;
  DWunion uu = {.ll = u};
  DWunion vv = {.ll = v};
  DWtype w;
  DWtype r;

  if (uu.s.high < 0)
    c1 = ~c1, c2 = ~c2,
    uu.ll = -uu.ll;
  if (vv.s.high < 0)
    c1 = ~c1,
    vv.ll = -vv.ll;

  w = __udivmoddi4 (uu.ll, vv.ll, (UDWtype*)&r);
  if (c1)
    w = -w;
  if (c2)
    r = -r;

  *rp = r;
  return w;
}
#endif

