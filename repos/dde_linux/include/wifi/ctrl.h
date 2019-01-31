/*
 * \brief  Wpa_supplicant CTRL interface
 * \author Josef Soentgen
 * \date   2018-07-31
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _WIFI__CTRL_H_
#define _WIFI__CTRL_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define WPA_CTRL_FD 51

struct Msg_buffer
{
	unsigned char recv[4096*8];
	unsigned char send[4096];
	unsigned recv_id;
	unsigned send_id;
	unsigned char event[1024];
	unsigned event_id;
} __attribute__((packed));


void wpa_ctrl_set_fd(void);

void *wifi_get_buffer(void);
void  wifi_notify_cmd_result(void);
void  wifi_block_for_processing(void);
void  wifi_notify_event(void);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _WIFI__CTRL_H_ */
