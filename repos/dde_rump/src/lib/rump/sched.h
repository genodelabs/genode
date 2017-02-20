/**
 * \brief  Rump-scheduling upcalls
 * \author Sebastian Sump
 * \date   2013-12-06
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SCHED_H_
#define _SCHED_H_

extern "C" {
#include <sys/cdefs.h>
#include <sys/types.h>
#include <rump/rump.h>
#include <rump/rumpuser.h>
#include <sys/errno.h>
}

#include <util/hard_context.h>

/* upcalls to rump kernel */
extern struct rumpuser_hyperup _rump_upcalls;

static inline void
rumpkern_unsched(int *nlocks, void *interlock)
{
	_rump_upcalls.hyp_backend_unschedule(0, nlocks, interlock);
}


static inline void
rumpkern_sched(int nlocks, void *interlock)
{
	_rump_upcalls.hyp_backend_schedule(nlocks, interlock);
}

#endif /* _SCHED_H_ */
