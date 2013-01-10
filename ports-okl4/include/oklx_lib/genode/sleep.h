/*
 * \brief  Genode C API sleep functions needed by OKLinux
 * \author Stefan Kalkowski
 * \date   2009-05-19
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__OKLINUX_SUPPORT__GENODE__SLEEP_H_
#define _INCLUDE__OKLINUX_SUPPORT__GENODE__SLEEP_H_

/**
 * Sleep for a given period of time
 *
 * \param ms milliseconds to sleep
 */
void genode_sleep(unsigned ms);

/**
 * Sleep forever, never wake up again
 */
void genode_sleep_forever(void);

#endif //_INCLUDE__OKLINUX_SUPPORT__GENODE__SLEEP_H_
