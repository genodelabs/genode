/*
 * \brief  main() wrapper for customizing main()'s stack size  
 * \author Christian Prochaska
 * \date   2008-08-20
 */

/*
 * Copyright (C) 2008-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifdef QT_MAIN_STACK_SIZE

#include <libc/component.h>

Genode::size_t Libc::Component::stack_size() { return QT_MAIN_STACK_SIZE; }

#endif /* QT_MAIN_STACK_SIZE */
