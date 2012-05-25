/*
 * \brief   Input service
 * \author  Christian Prochaska
 * \date    2012-03-29
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INPUT_SERVICE_H_
#define _INPUT_SERVICE_H_

/* Genode includes */
#include <input/event_queue.h>

extern Event_queue ev_queue;

extern void create_input_service();

#endif /* _INPUT_SERVICE_H_ */
