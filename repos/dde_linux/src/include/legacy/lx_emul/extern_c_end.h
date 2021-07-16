/*
 * \brief  Include after including Linux headers in C++
 * \author Christian Helmuth
 * \date   2014-08-21
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifdef __cplusplus

#undef delete
#undef new
#undef class
#undef private
#undef namespace
#undef virtual

#pragma GCC diagnostic pop

} /* extern "C" */

#endif /* __cplusplus */
