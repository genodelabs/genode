/*
 * \brief  Pager support for Codezero
 * \author Norman Feske
 * \date   2010-02-16
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/ipc_pager.h>
#include <base/printf.h>

/* Codezero includes */
#include <codezero/syscalls.h>


using namespace Genode;
using namespace Codezero;

enum { verbose_page_faults = false };


/************************
 ** Page-fault utility **
 ************************/

class Fault
{
	public:

		enum Type { READ, WRITE, EXEC, UNKNOWN };

	private:

		/**
		 * Translate Codezero page-fault information to generic fault type
		 *
		 * \param sr status
		 * \param pte page-table entry
		 */
		static Type _fault_type(umword_t sr, umword_t pte)
		{
			if (is_prefetch_abort(sr))
				return EXEC;

			if ((pte & PTE_PROT_MASK) == (__MAP_USR_RO & PTE_PROT_MASK))
				return WRITE;

			return READ;
		}

		Type     _type;
		umword_t _addr;
		umword_t _ip;

	public:

		/**
		 * Constructor
		 *
		 * \param kdata  Codezero-specific page-fault information
		 */
		Fault(struct fault_kdata const &kdata)
		:
			_type(_fault_type(kdata.fsr, kdata.pte)),
			_addr(_type == EXEC ? kdata.faulty_pc : kdata.far),
			_ip(kdata.faulty_pc)
		{ }

		Type     type() const { return _type; }
		umword_t addr() const { return _addr; }
		umword_t ip()   const { return _ip; }
};


/**
 * Print page-fault information in a human-readable form
 */
inline void print_page_fault(Fault &fault, int from)
{
	printf("page (%s%s%s) fault from %d at pf_addr=%lx, pf_ip=%lx\n",
	       fault.type() == Fault::READ  ? "r"  : "-",
	       fault.type() == Fault::WRITE ? "w"  : "-",
	       fault.type() == Fault::EXEC  ? "x"  : "-",
	       from, fault.addr(), fault.ip());
}


/***************
 ** IPC pager **
 ***************/

void Ipc_pager::wait_for_fault()
{
	for (;;) {
		int ret = l4_receive(L4_ANYTHREAD);

		if (ret < 0) {
			PERR("pager: l4_received returned ret=%d", ret);
			continue;
		}

		umword_t tag         = l4_get_tag();
		int      faulter_tid = l4_get_sender();

		if (tag != L4_IPC_TAG_PFAULT) {
			PWRN("got an unexpected IPC from %d", faulter_tid);
			continue;
		}

		/* copy fault information from message registers */
		struct fault_kdata fault_kdata;
		for (unsigned i = 0; i < sizeof(fault_kdata_t)/sizeof(umword_t); i++)
			((umword_t *)&fault_kdata)[i] = read_mr(MR_UNUSED_START + i);

		Fault fault(fault_kdata);

		if (verbose_page_faults)
			print_page_fault(fault, faulter_tid);

		/* determine corresponding page in our own address space */
		_pf_addr  = fault.addr();
		_pf_write = fault.type() == Fault::WRITE;
		_pf_ip    = fault.ip();
		_last     = faulter_tid;

		return;
	}
}


void Ipc_pager::reply_and_wait_for_fault()
{
	/* install mapping */
	umword_t flags = _reply_mapping.writeable() ? MAP_USR_RW
	                                            : MAP_USR_RO;
	
	/*
	 * XXX: remove heuristics for mapping device registers.
	 */
	if (_reply_mapping.from_phys() == 0x10120000  /* LCD */
	 || _reply_mapping.from_phys() == 0x10006000  /* keyboard */
	 || _reply_mapping.from_phys() == 0x10007000) /* mouse */
		flags = MAP_USR_IO;

	int ret = l4_map((void *)_reply_mapping.from_phys(),
	                 (void *)_reply_mapping.to_virt(),
	                 _reply_mapping.num_pages(), flags, _last);

	/* wake up faulter if mapping succeeded */
	if (ret < 0)
		PERR("l4_map returned %d, putting thread %d to sleep", ret, _last);
	else
		acknowledge_wakeup();

	/* wait for next page fault */
	wait_for_fault();
}


void Ipc_pager::acknowledge_wakeup()
{
	enum { SUCCESS = 0 };
	l4_set_sender(_last);
	l4_ipc_return(SUCCESS);
}


Ipc_pager::Ipc_pager() : Native_capability(thread_myself(), 0) { }

