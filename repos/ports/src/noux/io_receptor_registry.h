/*
 * \brief  Io receptor registry
 * \author Josef Soentgen
 * \date   2012-10-05
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _NOUX__IO_RECEPTOR_REGISTRY_H_
#define _NOUX__IO_RECEPTOR_REGISTRY_H_

/* Genode includes */
#include <base/lock.h>
#include <util/list.h>


namespace Noux {
	struct Io_receptor;
	struct Io_receptor_registry;
}


struct Noux::Io_receptor : List<Io_receptor>::Element
{
	private:

		Lock *_lock;

	public:

		Io_receptor(Lock *lock) : _lock(lock) { }

		void check_and_wakeup()
		{
			if (_lock)
				_lock->unlock();
		}
};


class Noux::Io_receptor_registry
{
	private:

		List<Io_receptor> _receptors;
		Lock              _receptors_lock;

	public:

		Io_receptor_registry() { }

		~Io_receptor_registry()
		{
			Io_receptor *receptor;
			while ((receptor = _receptors.first()) != 0)
				_receptors.remove(receptor);
		}

		void register_receptor(Io_receptor *receptor)
		{
			Lock::Guard guard(_receptors_lock);

			_receptors.insert(receptor);
		}

		void unregister_receptor(Io_receptor *receptor)
		{
			Lock::Guard guard(_receptors_lock);

			_receptors.remove(receptor);
		}

		Io_receptor *first() { return _receptors.first(); }
};

#endif /* _NOUX__IO_RECEPTOR_REGISTRY_H_ */
