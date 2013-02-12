/*
 * \brief  Arndale specific definition of the context area location
 * \author Sebastian Sumpf
 * \date   2013-02-12
 *
 * We need to place the context area within core outside the physical memory.
 * Sigma0 maps physical to core-local memory always 1:1 when using
 * SIGMA0_REQ_FPAGE_ANY. Those mappings would interfere with the context area.
 */

#include <base/native_types.h>

using namespace Genode;

addr_t Native_config::context_area_virtual_base() { return 0x20000000UL; }
