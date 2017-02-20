/*
 * \brief  Parent_exit interface
 * \author Christian Prochaska
 * \date   2014-01-16
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _NOUX__PARENT_EXIT__H_
#define _NOUX__PARENT_EXIT__H_

namespace Noux {

	struct Family_member;

	struct Parent_exit
	{
		/*
		 * Handle the exiting of a child
		 */

		virtual void exit_child() = 0;
	};

};

#endif /* _NOUX__PARENT_EXIT__H_ */

