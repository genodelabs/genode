/**
 * \brief  Noux initialization
 * \author Sebastian Sumpf
 * \date   2017-02-24
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <libc/component.h>
#include <construct.h>

void Libc::Component::construct(Libc::Env &env)
{
	noux_construct(env);
}
