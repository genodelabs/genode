/*
 * \brief  Genode C API audio functions needed by OKLinux
 * \author Stefan Kalkowski
 * \date   2009-11-09
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__OKLINUX_SUPPORT__GENODE__AUDIO_H_
#define _INCLUDE__OKLINUX_SUPPORT__GENODE__AUDIO_H_

int genode_audio_ready(void);

void genode_audio_collect_acks(void);

void genode_audio_trigger_start(void (*func)(unsigned long), unsigned long data);

void genode_audio_trigger_stop(void);

void genode_audio_prepare(void);

unsigned long genode_audio_position(void);

void genode_audio_write(void* src, unsigned long sz);

void genode_audio_fill_silence(unsigned long sz);

unsigned int genode_audio_packet_size(void);

unsigned int genode_audio_packet_count(void);

#endif //_INCLUDE__OKLINUX_SUPPORT__GENODE__AUDIO_H_
