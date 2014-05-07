/*
 * \brief  Dummy implementations of symbols needed by VirtualBox
 * \author Norman Feske
 * \date   2013-08-22
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Genode includes */
#include <base/printf.h>

/* VirtualBox includes */
#include <VBox/vmm/vmapi.h>

#define CXX_DUMMY(retval, signature) \
int signature { \
	PDBG( #signature " called, not implemented"); \
	for (;;); \
	return retval; \
}

#define CHECKED_CXX_DUMMY(retval, signature) \
int signature { \
	PINF( #signature " called, not implemented"); \
	return retval; \
}

CXX_DUMMY(-1, VMMR3InitCompleted(VM*, VMINITCOMPLETED))
CXX_DUMMY(-1, VMMR3InitR0(VM*))
CXX_DUMMY(-1, VMMR3InitRC(VM*))
CXX_DUMMY(-1, VMMR3Init(VM*))
CXX_DUMMY(-1, VMMR3Relocate(VM*, long long))
CXX_DUMMY(-1, VMMR3Term(VM*))
CXX_DUMMY(-1, PGMR3InitCompleted(VM*, VMINITCOMPLETED))
CXX_DUMMY(-1, PGMNotifyNxeChanged(VMCPU*, bool))
CXX_DUMMY(-1, VMMR3SendSipi(VM*, unsigned int, unsigned int))
CXX_DUMMY(-1, VMMR3SendInitIpi(VM*, unsigned int))
CXX_DUMMY(-1, VMMR3EmtRendezvousFF(VM*, VMCPU*))
CXX_DUMMY(-1, VMMR3YieldStop(VM*))
CXX_DUMMY(-1, VMMR3EmtRendezvousSetDisabled(VMCPU*, bool))
