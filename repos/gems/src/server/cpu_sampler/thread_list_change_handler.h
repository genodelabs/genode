/*
 * \brief  Thread list change handler interface
 * \author Christian Prochaska
 * \date   2016-01-19
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _THREAD_LIST_CHANGE_HANDLER_H_
#define _THREAD_LIST_CHANGE_HANDLER_H_

struct Thread_list_change_handler
{
	virtual void thread_list_changed() = 0;
};

#endif /* _THREAD_LIST_CHANGE_HANDLER_H_ */
