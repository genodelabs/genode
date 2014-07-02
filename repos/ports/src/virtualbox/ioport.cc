/*
 * \brief  VirtualBox I/O port monitor
 * \author Norman Feske
 * \date   2013-09-02
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Genode includes */
#include <util/list.h>
#include <base/printf.h>
#include <base/lock.h>
#include <base/env.h>

/* VirtualBox includes */
#include <VBox/vmm/iom.h>
#include <VBox/err.h>
#include <VBox/vmm/pdmdev.h>

static bool verbose = false;

class Guest_ioports
{
	struct Range;

	/*
	 * XXX Use AVL tree instead of a linked list
	 */

	typedef Genode::List<Range> Range_list;
	typedef Genode::Lock        Lock;

	private:

		struct Range : Range_list::Element
		{
			PPDMDEVINS            _pDevIns;
			RTIOPORT              _PortStart;
			RTUINT                _cPorts;
			RTHCPTR               _pvUser;
			PFNIOMIOPORTOUT       _pfnOutCallback;
			PFNIOMIOPORTIN        _pfnInCallback;
			PFNIOMIOPORTOUTSTRING _pfnOutStringCallback;
			PFNIOMIOPORTINSTRING  _pfnInStringCallback;

			/**
			 * Return true if range contains specified subrange
			 */
			bool contains(RTIOPORT PortStart, RTUINT cPorts) const
			{
				return (PortStart >= _PortStart)
				 && (PortStart <= _PortStart + _cPorts - 1);
			}

			bool partof(RTIOPORT PortStart, RTUINT cPorts) const
			{
				return (PortStart <= _PortStart)
				 && (PortStart + cPorts - 1 >= _PortStart + _cPorts - 1);
			}

			Range(PPDMDEVINS            pDevIns,
			      RTIOPORT              PortStart,
			      RTUINT                cPorts,
			      RTHCPTR               pvUser,
			      PFNIOMIOPORTOUT       pfnOutCallback,
			      PFNIOMIOPORTIN        pfnInCallback,
			      PFNIOMIOPORTOUTSTRING pfnOutStringCallback,
			      PFNIOMIOPORTINSTRING  pfnInStringCallback)
			:
				_pDevIns              (pDevIns),
				_PortStart            (PortStart),
				_cPorts               (cPorts),
				_pvUser               (pvUser),
				_pfnOutCallback       (pfnOutCallback),
				_pfnInCallback        (pfnInCallback),
				_pfnOutStringCallback (pfnOutStringCallback),
				_pfnInStringCallback  (pfnInStringCallback)
			{ }

			VBOXSTRICTRC write(RTIOPORT port, uint32_t u32Value, unsigned cb)
			{
				if (!_pfnOutCallback)
					return VINF_IOM_R3_IOPORT_WRITE;

//				PDBG("IOPORT write Port=0x%lx", (long)port);
				VBOXSTRICTRC rc = PDMCritSectEnter(_pDevIns->CTX_SUFF(pCritSectRo),
				                                   VINF_IOM_R3_IOPORT_WRITE);
				if (rc != VINF_SUCCESS)
					return rc;

				rc = _pfnOutCallback(_pDevIns, _pvUser, port, u32Value, cb);

				PDMCritSectLeave(_pDevIns->CTX_SUFF(pCritSectRo));

				return rc;
			}

			VBOXSTRICTRC read(RTIOPORT port, uint32_t *pu32Value, unsigned cb)
			{
				if (!_pfnInCallback)
					return VINF_IOM_R3_IOPORT_READ;

				VBOXSTRICTRC rc = PDMCritSectEnter(_pDevIns->CTX_SUFF(pCritSectRo),
				                                   VINF_IOM_R3_IOPORT_READ);
				if (rc != VINF_SUCCESS)
					return rc;
					
				rc = _pfnInCallback(_pDevIns, _pvUser, port, pu32Value, cb);
				if (rc != VINF_SUCCESS)
					PDBG("IOPORT read port=0x%x failed - callback %p eip %p",
					     port, _pfnInCallback, __builtin_return_address(0));
 
				PDMCritSectLeave(_pDevIns->CTX_SUFF(pCritSectRo));

				return rc;
			}
		};

		Lock       _lock;
		Range_list _ranges;

		Range *_lookup(RTIOPORT PortStart, RTUINT cPorts)
		{
			for (Range *r = _ranges.first(); r; r = r->next())
				if (r->contains(PortStart, cPorts)) {
//					PINF("lookuped %lx %lx", r->_PortStart, r->_cPorts);
					return r;
				}

			return 0;
		}

		/*
		 * The whitelist is used to suppress log messages, which the VM tries
		 * to access I/O ports with no device model associated. TinyCore Linux
		 * seems to probe a lot of I/O ports, e.g. the LPT3 ports.
		 */
		bool _white_listed(RTIOPORT port)
		{
			/* LPT1 */ if (port >= 0x0378 && port <= 0x037f) return true;
			/* LPT3 */ if (port >= 0x0278 && port <= 0x027f) return true;
			/* ECP  */ if (port >= 0x0778 && port <= 0x077a) return true;
			/* IDE1 */ if (port >= 0x0170 && port <= 0x017f) return true;
			return false;
		}

	public:

		int  add_range(PPDMDEVINS pDevIns,
		               RTIOPORT PortStart, RTUINT cPorts, RTHCPTR pvUser,
		               R3PTRTYPE(PFNIOMIOPORTOUT) pfnOutCallback,
		               R3PTRTYPE(PFNIOMIOPORTIN) pfnInCallback,
		               R3PTRTYPE(PFNIOMIOPORTOUTSTRING) pfnOutStringCallback,
		               R3PTRTYPE(PFNIOMIOPORTINSTRING) pfnInStringCallback)
		{
/*
			Range *r = _lookup(PortStart, cPorts);
			if (r) {
				PERR("failure 0x%lx+0x%lx", PortStart, cPorts);
				while (1) {}
				return VERR_GENERAL_FAILURE;
			}
*/
			_ranges.insert(new (Genode::env()->heap())
			               Range(pDevIns, PortStart, cPorts, pvUser,
			                     pfnOutCallback, pfnInCallback,
			                     pfnOutStringCallback, pfnInStringCallback));

			return VINF_SUCCESS;
		}

		int remove_range(PPDMDEVINS pDevIns, RTIOPORT PortStart, RTUINT cPorts)
		{
			bool deleted = false;

			for (Range *r = _ranges.first(); r;)
			{
				if (!r->partof(PortStart, cPorts)) {
					r = r->next();
					continue;
				}

				deleted = true;

				PERR("delete %x+%x", r->_PortStart, r->_cPorts);

				Range *s = r;
				r = r->next();
				_ranges.remove(s);
			}

			return deleted ? VINF_SUCCESS : VERR_GENERAL_FAILURE;
		}

		VBOXSTRICTRC write(RTIOPORT port, uint32_t u32Value, size_t cbValue)
		{
			Range *r = _lookup(port, cbValue);
			if (r)
				return r->write(port, u32Value, cbValue);

			if (_white_listed(port))
				return VINF_SUCCESS;

			char c = u32Value & 0xff;
			PWRN("attempted to write to non-existing port 0x%lx+%u  %c (%02x)",
			     port, cbValue, c >= 32 && c <= 176 ? c : '.', c);
			return VINF_SUCCESS;
		}

		VBOXSTRICTRC read(RTIOPORT port, uint32_t *pu32Value, unsigned cbValue)
		{
			Range *r = _lookup(port, cbValue);
			if (r) {
				VBOXSTRICTRC err = r->read(port, pu32Value, cbValue);
				if (err != VERR_IOM_IOPORT_UNUSED)
					return err;
			}

			if (!_white_listed(port))
				PWRN("attempted to read from non-existing port 0x%x+%u %p",
				     port, cbValue, r);

			switch (cbValue)
			{
				case 1:
					*reinterpret_cast<uint8_t *>(pu32Value) = 0xFFU;
					break;
				case 2:
					*reinterpret_cast<uint16_t *>(pu32Value) = 0xFFFFU;
					break;
				case 4:
					*reinterpret_cast<uint32_t *>(pu32Value) = 0xFFFFFFFFU;
					break;
				default:
					PERR("Invalid I/O port (%x) access of size (%x)", port, cbValue);
					return VERR_IOM_INVALID_IOPORT_SIZE;
			}
			return VINF_SUCCESS;
//			return VERR_IOM_IOPORT_UNUSED; /* recompiler does not like this */
		}
};


/**
 * Return pointer to singleton instance
 */
Guest_ioports *guest_ioports()
{
	static Guest_ioports inst;
	return &inst;
}


int
IOMR3IOPortRegisterR3(PVM pVM, PPDMDEVINS pDevIns,
                      RTIOPORT PortStart, RTUINT cPorts, RTHCPTR pvUser,
                      R3PTRTYPE(PFNIOMIOPORTOUT)       pfnOutCallback,
                      R3PTRTYPE(PFNIOMIOPORTIN)        pfnInCallback,
                      R3PTRTYPE(PFNIOMIOPORTOUTSTRING) pfnOutStringCallback,
                      R3PTRTYPE(PFNIOMIOPORTINSTRING)  pfnInStringCallback,
                      const char *pszDesc)
{
	if (verbose)
		PLOG("register I/O port range 0x%x-0x%x '%s'",
		     PortStart, PortStart + cPorts - 1, pszDesc);

	return guest_ioports()->add_range(pDevIns, PortStart, cPorts, pvUser,
	                                  pfnOutCallback, pfnInCallback,
	                                  pfnOutStringCallback, pfnInStringCallback);
}


int IOMR3IOPortDeregister(PVM pVM, PPDMDEVINS pDevIns, RTIOPORT PortStart,
                          RTUINT cPorts)
{
	if (verbose)
		PLOG("deregister I/O port range 0x%x-0x%x",
		     PortStart, PortStart + cPorts - 1);

	return guest_ioports()->remove_range(pDevIns, PortStart, cPorts);
}


VMMDECL(VBOXSTRICTRC) IOMIOPortWrite(PVM pVM, RTIOPORT Port, uint32_t u32Value,
                                     size_t cbValue)
{
//	PDBG("IOMIOPortWrite Port=0x%lx cbValue=%zd", (long)Port, cbValue);
	return guest_ioports()->write(Port, u32Value, cbValue);
}


VMMDECL(VBOXSTRICTRC) IOMIOPortRead(PVM pVM, RTIOPORT Port, uint32_t *pu32Value,
                                    size_t cbValue)
{
	return guest_ioports()->read(Port, pu32Value, cbValue);
}
