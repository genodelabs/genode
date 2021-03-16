/**
 * \brief  USB webcam model back end definition
 * \author Alexander Boettcher
 *
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _WEBCAM_BACKEND_H_
#define _WEBCAM_BACKEND_H_

struct webcam_config {
	unsigned width;
	unsigned height;
	unsigned fps;
};

extern void webcam_backend_config(struct webcam_config *);
extern void capture_bgr_frame(void * pixel);
extern void capture_yuv_frame(void * pixel);
extern void capture_state_changed(bool on, char const * format);

#endif /* _WEBCAM_BACKEND_H_ */
