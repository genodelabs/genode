/*
 * \brief  Generic definitions for the location of the thread-context area
 * \author Sebastian Sumpf
 * \date   2013-02-12
 */

#include <base/native_types.h>

using namespace Genode;

addr_t Native_config::context_area_virtual_base() { return 0x40000000UL; }
