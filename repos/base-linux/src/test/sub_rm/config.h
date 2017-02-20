/*
 * \brief  Linux-specific policy for sub_rm test
 * \author Norman Feske
 * \date   2011-11-22
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/*
 * The Linux implementation of the RM service does not support attaching
 * the same sub RM session twice. This configuration enables the respective
 * error-handling test.
 */
enum { attach_twice_forbidden = true };

enum { support_attach_sub_any = false };

