/*
 * \brief  Include after including Linux headers in C++
 * \author Christian Helmuth
 * \date   2014-08-21
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifdef __cplusplus

#undef new
#undef class
#undef private

#pragma GCC diagnostic pop

} /* extern "C" */

#endif /* __cplusplus */
