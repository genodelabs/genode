/*
 * \brief  Macro to make a class non-copyable
 * \author Martin Stein
 * \date   2023-06-09
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TRESOR__NONCOPYABLE_H_
#define _TRESOR__NONCOPYABLE_H_

#define NONCOPYABLE(class_name) \
	class_name(class_name const &) = delete; \
	class_name &operator = (class_name const &) = delete; \

#endif /* _TRESOR__NONCOPYABLE_H_ */
