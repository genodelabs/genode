/*
 * \brief  File-system listener
 * \author Norman Feske
 * \date   2012-04-11
 */

#ifndef _FILE_SYSTEM__LISTENER_H_
#define _FILE_SYSTEM__LISTENER_H_

/* Genode includes */
#include <file_system_session/file_system_session.h>
#include <util/list.h>
#include <base/lock.h>
#include <base/signal.h>

namespace File_system {

	class Listener : public Genode::List<Listener>::Element
	{
		private:

			Genode::Lock                      _lock;
			Genode::Signal_context_capability _sigh;
			bool                 _marked_as_updated;

		public:

			Listener() : _marked_as_updated(false) { }

			Listener(Signal_context_capability sigh)
			: _sigh(sigh), _marked_as_updated(false) { }

			void notify()
			{
				Genode::Lock::Guard guard(_lock);

				if (_marked_as_updated && _sigh.valid())
					Signal_transmitter(_sigh).submit();

				_marked_as_updated = false;
			}

			void mark_as_updated()
			{
				Genode::Lock::Guard guard(_lock);

				_marked_as_updated = true;
			}

			bool valid() const { return _sigh.valid(); }
	};

}

#endif /* _FILE_SYSTEM__LISTENER_H_ */
