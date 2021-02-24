/*
 * \brief  Utility for querying the child-exit state from init's state report
 * \author Norman Feske
 * \author Martin Stein
 * \date   2021-03-05
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CHILD_EXIT_STATE_H_
#define _CHILD_EXIT_STATE_H_

/* Genode includes */
#include <util/xml_node.h>

/* local includes */
#include <types.h>

namespace File_vault {

	class Child_exit_state;
}

class File_vault::Child_exit_state
{
	public:

		typedef String<128> Name;
		typedef String<16>  Version;

	private:

		bool    _exists     = false;
		bool    _exited     = false;
		bool    _responsive = true;
		int     _code       = 0;
		Version _version { };

	public:

		Child_exit_state(Xml_node init_state, Name const &name)
		{
			init_state.for_each_sub_node("child", [&] (Xml_node child) {
				if (child.attribute_value("name", Name()) == name) {
					_exists = true;
					_version = child.attribute_value("version", Version());

					if (child.has_attribute("exited")) {
						_exited = true;
						_code = child.attribute_value("exited", 0L);
					}

					_responsive = (child.attribute_value("skipped_heartbeats", 0U) <= 2);
				}
			});
		}

		bool    exists() const     { return _exists     ; }
		bool    exited() const     { return _exited     ; }
		bool    responsive() const { return _responsive ; }
		int     code() const       { return _code       ; }
		Version version() const    { return _version    ; }
};

#endif /* _CHILD_EXIT_STATE_H_ */
