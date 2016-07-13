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
#include <base/log.h>
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

			bool partof(RTIOPORT port, RTUINT cPorts) const
			{
				RTIOPORT last_port  = port + cPorts - 1;
				RTIOPORT _last_port = _PortStart + _cPorts - 1;
				return ((port <= _PortStart) && (_PortStart <= last_port)) ||
				       ((port <= _last_port) && (_last_port <= last_port));
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

				PDMCritSectLeave(_pDevIns->CTX_SUFF(pCritSectRo));

				if (rc != VERR_IOM_IOPORT_UNUSED && rc != VINF_SUCCESS)
					Genode::log("IOPORT read port=", Genode::Hex(port), " failed"
					            " - callback=", _pfnInCallback, " eip=",
					            __builtin_return_address(0));
 
				return rc;
			}
		};

		Lock       _lock;
		Range_list _ranges;

		Range *_lookup(RTIOPORT PortStart, RTUINT cPorts)
		{
			for (Range *r = _ranges.first(); r; r = r->next())
				if (r->contains(PortStart, cPorts))
					return r;

			return 0;
		}

		void dump()
		{
			for (Range *r = _ranges.first(); r; r = r->next())
				Genode::log(Genode::Hex(r->_PortStart), "+",
				            Genode::Hex(r->_cPorts), " - '",
				            r->_pDevIns && r->_pDevIns->pReg ? r->_pDevIns->pReg->szName : 0, "'");
		}

	public:

		int add_range(PPDMDEVINS pDevIns,
		              RTIOPORT PortStart, RTUINT cPorts, RTHCPTR pvUser,
		              R3PTRTYPE(PFNIOMIOPORTOUT) pfnOutCallback,
		              R3PTRTYPE(PFNIOMIOPORTIN) pfnInCallback,
		              R3PTRTYPE(PFNIOMIOPORTOUTSTRING) pfnOutStringCallback,
		              R3PTRTYPE(PFNIOMIOPORTINSTRING) pfnInStringCallback)
		{
			Range *r = _lookup(PortStart, cPorts);
			if (r) {
				Genode::error("io port inseration failure ",
				              Genode::Hex(PortStart), "+",
				              Genode::Hex(cPorts), " - '",
				              pDevIns && pDevIns->pReg ? pDevIns->pReg->szName : 0, "'");
				dump();
				Assert(!r);
				return VERR_GENERAL_FAILURE;
			}

			if (verbose)
				Genode::log("insert io port range ", Genode::Hex(PortStart), "+",
				            Genode::Hex(cPorts), " - '",
				            pDevIns && pDevIns->pReg ? pDevIns->pReg->szName : 0, "'");

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

				if (verbose)
					Genode::log("delete io port range ",
					            Genode::Hex(r->_PortStart), "+",
					            Genode::Hex(r->_cPorts),    " out of ",
					            Genode::Hex(PortStart),     "+",
					            Genode::Hex(cPorts),        " - '",
					            r->_pDevIns &&
					            r->_pDevIns->pReg ? r->_pDevIns->pReg->szName : 0, "'");

				Range *s = r;
				r = r->next();
				_ranges.remove(s);

				destroy(Genode::env()->heap(), s);
			}

			return deleted ? VINF_SUCCESS : VERR_GENERAL_FAILURE;
		}

		VBOXSTRICTRC write(RTIOPORT port, uint32_t u32Value, size_t cbValue)
		{
			Range *r = _lookup(port, cbValue);
			if (r)
				return r->write(port, u32Value, cbValue);

			if (verbose) {
				char c = u32Value & 0xff;
				Genode::warning("attempted to write to non-existing port ",
				                Genode::Hex(port), "+", cbValue, " "
				                "value=", Genode::Hex(c));
			}

			return VINF_SUCCESS;
		}

		VBOXSTRICTRC read(RTIOPORT port, uint32_t *pu32Value, size_t cbValue)
		{
			Range *r = _lookup(port, cbValue);
			if (r) {
				VBOXSTRICTRC err = r->read(port, pu32Value, cbValue);
				if (err != VERR_IOM_IOPORT_UNUSED)
					return err;
			} else
				if (verbose)
					Genode::warning("attempted to read from non-existing port ",
					                Genode::Hex(port), "+", cbValue, " ", r);

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
					Genode::error("Invalid I/O port (", Genode::Hex(port), ") "
					              "access of size (", Genode::Hex(cbValue), ")");
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
		Genode::log("register I/O port range ",
		            Genode::Hex(PortStart), "-",
		            Genode::Hex(PortStart + cPorts - 1), " '", pszDesc, "'");

	return guest_ioports()->add_range(pDevIns, PortStart, cPorts, pvUser,
	                                  pfnOutCallback, pfnInCallback,
	                                  pfnOutStringCallback, pfnInStringCallback);
}


int IOMR3IOPortDeregister(PVM pVM, PPDMDEVINS pDevIns, RTIOPORT PortStart,
                          RTUINT cPorts)
{
	if (verbose)
		Genode::log("deregister I/O port range ",
		            Genode::Hex(PortStart), "-",
		            Genode::Hex(PortStart + cPorts - 1));

	return guest_ioports()->remove_range(pDevIns, PortStart, cPorts);
}


VMMDECL(VBOXSTRICTRC) IOMIOPortWrite(PVM, PVMCPU, RTIOPORT Port,
                                     uint32_t u32Value, size_t cbValue)
{
	return guest_ioports()->write(Port, u32Value, cbValue);
}


VMMDECL(VBOXSTRICTRC) IOMIOPortRead(PVM, PVMCPU, RTIOPORT Port,
                                    uint32_t *pu32Value, size_t cbValue)
{
	return guest_ioports()->read(Port, pu32Value, cbValue);
}
