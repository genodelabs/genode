/*
 * \brief   Generic page flags
 * \author  Stefan Kalkowski
 * \date    2014-02-24
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__CPU__PAGE_FLAGS_H_
#define _INCLUDE__CPU__PAGE_FLAGS_H_

#include <base/cache.h>
#include <base/output.h>

namespace Genode {

	enum Writeable   { RO, RW            };
	enum Executeable { NO_EXEC, EXEC     };
	enum Privileged  { USER, KERN        };
	enum Global      { NO_GLOBAL, GLOBAL };
	enum Type        { RAM, DEVICE       };

	struct Page_flags;
}


struct Genode::Page_flags
{
	Writeable     writeable;
	Executeable   executable;
	Privileged    privileged;
	Global        global;
	Type          type;
	Genode::Cache cacheable;

	void print(Genode::Output & out) const
	{
		using Genode::print;
		using namespace Genode;

		print(out, (writeable == RW)   ? "writeable, " : "readonly, ",
		           (executable ==EXEC) ? "exec, "      : "noexec, ");
		if (privileged == KERN)   print(out, "privileged, ");
		if (global     == GLOBAL) print(out, "global, ");
		if (type       == DEVICE) print(out, "iomem, ");
		switch (cacheable) {
			case UNCACHED:        print(out, "uncached");       break;
			case CACHED:          print(out, "cached");         break;
			case WRITE_COMBINED:  print(out, "write-combined"); break;
		};
	}
};


namespace Genode {

	static constexpr Page_flags PAGE_FLAGS_KERN_IO
		{ RW, NO_EXEC, KERN, GLOBAL, DEVICE, Genode::UNCACHED };
	static constexpr Page_flags PAGE_FLAGS_KERN_DATA
		{ RW, EXEC,    KERN, GLOBAL, RAM,    Genode::CACHED   };
	static constexpr Page_flags PAGE_FLAGS_KERN_TEXT
		{ RW, EXEC,    KERN, GLOBAL, RAM,    Genode::CACHED   };
	static constexpr Page_flags PAGE_FLAGS_KERN_EXCEP
		{ RW, EXEC,    KERN, GLOBAL, RAM,    Genode::CACHED   };
	static constexpr Page_flags PAGE_FLAGS_UTCB
		{ RW, NO_EXEC, USER, NO_GLOBAL, RAM, Genode::CACHED   };
}

#endif /* _INCLUDE__CPU__PAGE_FLAGS_H_ */
