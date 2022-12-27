/**
 * Genode network session driver,
 * derived from src/VBox/Devices/Network/DrvTAP.cpp.
 */

/*
 * Copyright (C) 2014-2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/*
 * Copyright (C) 2006-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_DRV_TUN
#include <base/log.h>
#include <VBox/log.h>
#include <VBox/vmm/pdmdrv.h>
#include <VBox/vmm/pdmnetifs.h>
#include <VBox/vmm/pdmnetinline.h>

#include <iprt/mem.h>
#include <iprt/uuid.h>

#include "VBoxDD.h"

#include <nic_session/connection.h>
#include <nic/packet_allocator.h>
#include <base/snprintf.h>

#include <internal/thread_create.h>

/* VBox Genode specific */
#include "vmm.h"
#include <libc/component.h>

/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/

struct Nic_client;

/**
 * Nic driver instance data.
 *
 * @implements PDMINETWORKUP
 */
typedef struct DRVNIC
{
	/** The network interface to Nic session. */
	PDMINETWORKUP           INetworkUp;
	/** The config port interface we're representing. */
	PDMINETWORKCONFIG       INetworkConfig;
	/** The network interface to VBox driver. */
	PPDMINETWORKDOWN        pIAboveNet;
	/** The config port interface we're attached to. */
	PPDMINETWORKCONFIG      pIAboveConfig;
	/** Pointer to the driver instance. */
	PPDMDRVINS              pDrvIns;
	/** Receiver thread that handles all signals. */
	PPDMTHREAD              pThread;
	/** Nic::Session client wrapper. */
	Nic_client             *nic_client;
} DRVNIC, *PDRVNIC;


/**
 * Return lock to synchronize the destruction of the
 * PDRVNIC, i.e., the Nic_client.
 */
static Genode::Blockade &destruct_blockade()
{
	static Genode::Blockade blockade { };
	return blockade;
}


class Nic_client
{
	private:

		enum {
			PACKET_SIZE = Nic::Packet_allocator::DEFAULT_PACKET_SIZE,
			BUF_SIZE    = Nic::Session::QUEUE_SIZE * PACKET_SIZE,
		};

		Nic::Packet_allocator  *_tx_block_alloc;
		Nic::Connection         _nic;

		enum { NIC_EP_STACK = 32u << 10, };
		Genode::Entrypoint      _ep;
		pthread_t               _pthread;

		Genode::Signal_handler<Nic_client> _link_state_dispatcher;
		Genode::Signal_handler<Nic_client> _rx_packet_avail_dispatcher;
		Genode::Signal_handler<Nic_client> _rx_ready_to_ack_dispatcher;
		Genode::Signal_handler<Nic_client> _destruct_dispatcher;

		bool _link_up = false;

		/* VM <-> device driver (down) <-> nic_client (up) <-> nic session */
		PPDMINETWORKDOWN   _down_rx;
		PPDMINETWORKCONFIG _down_rx_config;

		void _handle_rx_packet_avail()
		{
			while (_nic.rx()->packet_avail() && _nic.rx()->ready_to_ack()) {
				Nic::Packet_descriptor rx_packet = _nic.rx()->get_packet();

				char *rx_content = _nic.rx()->packet_content(rx_packet);

				Libc::with_libc([&] () {
					int rc = _down_rx->pfnWaitReceiveAvail(_down_rx, RT_INDEFINITE_WAIT);
					if (RT_FAILURE(rc)) return;

					rc = _down_rx->pfnReceive(_down_rx, rx_content, rx_packet.size());
					AssertRC(rc);
				});

				_nic.rx()->acknowledge_packet(rx_packet);
			}
		}

		void _handle_rx_ready_to_ack() { _handle_rx_packet_avail(); }

		void _handle_link_state()
		{
			_link_up = _nic.link_state();

			Libc::with_libc([&] () {
				_down_rx_config->pfnSetLinkState(_down_rx_config,
				                                 _link_up ? PDMNETWORKLINKSTATE_UP
				                                          : PDMNETWORKLINKSTATE_DOWN);
			});
		}

		void _handle_destruct()
		{
			_nic.link_state_sigh(Genode::Signal_context_capability());
			_nic.rx_channel()->sigh_packet_avail(Genode::Signal_context_capability());
			_nic.rx_channel()->sigh_ready_to_ack(Genode::Signal_context_capability());

			destruct_blockade().wakeup();
		}

		void _tx_ack()
		{
			/* check for acknowledgements */
			while (_nic.tx()->ack_avail()) {
				Nic::Packet_descriptor acked_packet = _nic.tx()->get_acked_packet();
				auto packet_allocated_len = Nic::Packet_descriptor(acked_packet.offset(),
				                                                   Nic::Packet_allocator::OFFSET_PACKET_SIZE);
				_nic.tx()->release_packet(packet_allocated_len);
			}
		}

		static Nic::Packet_allocator* _packet_allocator()
		{
			return new (vmm_heap()) Nic::Packet_allocator(&vmm_heap());
		}

		void _handle_pthread_registration()
		{
			Genode::Thread *myself = Genode::Thread::myself();
			if (!myself || Libc::pthread_create_from_thread(&_pthread, *myself, &myself)) {
				Genode::error("network will not work - thread for pthread "
				              "registration invalid");
				return;
			}
		}

		Genode::Signal_handler<Nic_client> _pthread_reg_sigh {
			_ep, *this, &Nic_client::_handle_pthread_registration };

	public:

		Nic_client(Genode::Env &env, PDRVNIC drvtap, char const *label)
		:
			_tx_block_alloc(_packet_allocator()),
			_nic(env, _tx_block_alloc, BUF_SIZE, BUF_SIZE, label),
			_ep(env, NIC_EP_STACK, "nic_ep", Genode::Affinity::Location()),
			_link_state_dispatcher(_ep, *this, &Nic_client::_handle_link_state),
			_rx_packet_avail_dispatcher(_ep, *this, &Nic_client::_handle_rx_packet_avail),
			_rx_ready_to_ack_dispatcher(_ep, *this, &Nic_client::_handle_rx_ready_to_ack),
			_destruct_dispatcher(_ep, *this, &Nic_client::_handle_destruct),
			_down_rx(drvtap->pIAboveNet),
			_down_rx_config(drvtap->pIAboveConfig)
		{
			Genode::Signal_transmitter(_pthread_reg_sigh).submit();
		}

		~Nic_client()
		{
			/* XXX Libc::pthread_free(&_pthread); */
			destroy(vmm_heap(), _tx_block_alloc);
		}

		void enable_signals()
		{
			_nic.link_state_sigh(_link_state_dispatcher);
			_nic.rx_channel()->sigh_packet_avail(_rx_packet_avail_dispatcher);
			_nic.rx_channel()->sigh_ready_to_ack(_rx_ready_to_ack_dispatcher);

			/* inform signal handler ep */
			_link_state_dispatcher.local_submit();
		}

		Genode::Signal_context_capability dispatcher()   { return _destruct_dispatcher; }
		Nic::Mac_address                   mac_address() { return _nic.mac_address(); }

		bool alloc_packet(Nic::Packet_descriptor &pkg, uint32_t packet_len)
		{
			auto const result = _nic.tx()->alloc_packet_attempt(packet_len);

			return result.convert<bool>([&](auto &p) {
				pkg = p;
				return true;
			}, [&] (auto &) {
				return false;
			});
		}

		int send_packet(Nic::Packet_descriptor const &tx_packet, void *packet, uint32_t packet_len)
		{
			if (!_link_up) { return VERR_NET_DOWN; }

			/* check for acknowledgements */
			_tx_ack();

			if (tx_packet.size() < packet_len) {
				RTLogPrintf("%s: packet too large\n", __func__);
				_nic.tx()->release_packet(tx_packet);
				return VINF_SUCCESS;
			}

			/* send it */
			auto const tx_content = _nic.tx()->packet_content(tx_packet);
			Genode::memcpy(tx_content, packet, packet_len);
			auto tx_packet_actual_len = Nic::Packet_descriptor(tx_packet.offset(), packet_len);
			_nic.tx()->submit_packet(tx_packet_actual_len);
			return VINF_SUCCESS;
		}
};


/** Converts a pointer to Nic::INetworkUp to a PRDVNic. */
#define PDMINETWORKUP_2_DRVNIC(pInterface) ( (PDRVNIC)((uintptr_t)pInterface - RT_OFFSETOF(DRVNIC, INetworkUp)) )
#define PDMINETWORKCONFIG_2_DRVNIC(pInterface) ( (PDRVNIC)((uintptr_t)pInterface - RT_OFFSETOF(DRVNIC, INetworkConfig)) )


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/

/**
 * @interface_method_impl{PDMINETWORKUP,pfnBeginXmit}
 */
static DECLCALLBACK(int) drvNicNetworkUp_BeginXmit(PPDMINETWORKUP pInterface, bool fOnWorkerThread)
{
	return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMINETWORKUP,pfnAllocBuf}
 */
static DECLCALLBACK(int) drvNicNetworkUp_AllocBuf(PPDMINETWORKUP pInterface, size_t cbMin,
                                                  PCPDMNETWORKGSO pGso, PPPDMSCATTERGATHER ppSgBuf)
{
	PDRVNIC pThis = PDMINETWORKUP_2_DRVNIC(pInterface);
	Nic_client *nic_client = pThis->nic_client;

	/*
	 * Allocate a scatter / gather buffer descriptor that is immediately
	 * followed by the buffer space of its single segment.  The GSO context
	 * comes after that again.
	 */
	PPDMSCATTERGATHER pSgBuf = (PPDMSCATTERGATHER)RTMemAlloc(RT_ALIGN_Z(sizeof(*pSgBuf), 16)
	                                                         + RT_ALIGN_Z(cbMin, 16)
	                                                         + (pGso ? RT_ALIGN_Z(sizeof(*pGso), 16) : 0));
	if (!pSgBuf)
		return VERR_NO_MEMORY;

	/*
	 * Initialize the S/G buffer and return.
	 */
	pSgBuf->fFlags         = PDMSCATTERGATHER_FLAGS_MAGIC | PDMSCATTERGATHER_FLAGS_OWNER_1;
	pSgBuf->cbUsed         = 0;
	pSgBuf->cbAvailable    = RT_ALIGN_Z(cbMin, 16);
	pSgBuf->pvAllocator    = NULL;
	if (!pGso)
		pSgBuf->pvUser     = NULL;
	else
	{
		pSgBuf->pvUser     = (uint8_t *)(pSgBuf + 1) + pSgBuf->cbAvailable;
		*(PPDMNETWORKGSO)pSgBuf->pvUser = *pGso;
	}
	pSgBuf->cSegs          = 1;
	pSgBuf->aSegs[0].cbSeg = pSgBuf->cbAvailable;
	pSgBuf->aSegs[0].pvSeg = pSgBuf + 1;

	pSgBuf->pvAllocator = RTMemAllocZ(sizeof(Nic::Packet_descriptor));
	if (!pSgBuf->pvAllocator) {
		RTMemFree(pSgBuf);
		return VERR_TRY_AGAIN;
	}

	if (!nic_client->alloc_packet(*(Nic::Packet_descriptor *)pSgBuf->pvAllocator,
	                              Nic::Packet_allocator::OFFSET_PACKET_SIZE)) {
		RTMemFree(pSgBuf->pvAllocator);
		RTMemFree(pSgBuf);
		/* VERR_ERR_MEMORY leads to assertion in E1000 ... try again is evaluated */
		return VERR_TRY_AGAIN;
	}

	*ppSgBuf = pSgBuf;
	return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMINETWORKUP,pfnFreeBuf}
 */
static DECLCALLBACK(int) drvNicNetworkUp_FreeBuf(PPDMINETWORKUP pInterface, PPDMSCATTERGATHER pSgBuf)
{
	PDRVNIC pThis = PDMINETWORKUP_2_DRVNIC(pInterface);
	if (pSgBuf)
	{
		Assert((pSgBuf->fFlags & PDMSCATTERGATHER_FLAGS_MAGIC_MASK) == PDMSCATTERGATHER_FLAGS_MAGIC);
		pSgBuf->fFlags = 0;
		if (pSgBuf->pvAllocator)
			RTMemFree(pSgBuf->pvAllocator);
		RTMemFree(pSgBuf);
	}
	return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMINETWORKUP,pfnSendBuf}
 */
static DECLCALLBACK(int) drvNicNetworkUp_SendBuf(PPDMINETWORKUP pInterface, PPDMSCATTERGATHER pSgBuf, bool fOnWorkerThread)
{
	PDRVNIC pThis = PDMINETWORKUP_2_DRVNIC(pInterface);
	Nic_client *nic_client = pThis->nic_client;

	AssertPtr(pSgBuf);
	Assert((pSgBuf->fFlags & PDMSCATTERGATHER_FLAGS_MAGIC_MASK) == PDMSCATTERGATHER_FLAGS_MAGIC);

	if (!pSgBuf->pvAllocator) {
		RTLogPrintf("%s: error in packet allocation\n", __func__);
		return VERR_GENERAL_FAILURE;
	}

	auto &packet = *(Nic::Packet_descriptor *)pSgBuf->pvAllocator;

	/* Set an FTM checkpoint as this operation changes the state permanently. */
	PDMDrvHlpFTSetCheckpoint(pThis->pDrvIns, FTMCHECKPOINTTYPE_NETWORK);

	int rc;
	if (!pSgBuf->pvUser)
	{
		Log2(("drvNicSend: pSgBuf->aSegs[0].pvSeg=%p pSgBuf->cbUsed=%#x\n"
		      "%.*Rhxd\n", pSgBuf->aSegs[0].pvSeg, pSgBuf->cbUsed, pSgBuf->cbUsed,
		      pSgBuf->aSegs[0].pvSeg));

		rc = nic_client->send_packet(packet, pSgBuf->aSegs[0].pvSeg, pSgBuf->cbUsed);
	}
	else
	{
		uint8_t         abHdrScratch[256];
		uint8_t const  *pbFrame = (uint8_t const *)pSgBuf->aSegs[0].pvSeg;
		PCPDMNETWORKGSO pGso    = (PCPDMNETWORKGSO)pSgBuf->pvUser;
		uint32_t const  cSegs   = PDMNetGsoCalcSegmentCount(pGso, pSgBuf->cbUsed);
		Assert(cSegs > 1);
		rc = VINF_SUCCESS;
		for (size_t iSeg = 0; iSeg < cSegs; iSeg++)
		{
			uint32_t cbSegFrame;
			void *pvSegFrame = PDMNetGsoCarveSegmentQD(pGso, (uint8_t *)pbFrame, pSgBuf->cbUsed,
			                                           abHdrScratch, iSeg, cSegs, &cbSegFrame);
			rc = nic_client->send_packet(packet, pvSegFrame, cbSegFrame);
			if (RT_FAILURE(rc))
				break;
		}
	}

	pSgBuf->fFlags = 0;
	if (pSgBuf->pvAllocator)
		RTMemFree(pSgBuf->pvAllocator);
	RTMemFree(pSgBuf);

	AssertRC(rc);
	if (RT_FAILURE(rc))
		rc = rc == VERR_NO_MEMORY ? VERR_NET_NO_BUFFER_SPACE : VERR_NET_DOWN;
	return rc;
}


/**
 * @interface_method_impl{PDMINETWORKUP,pfnEndXmit}
 */
static DECLCALLBACK(void) drvNicNetworkUp_EndXmit(PPDMINETWORKUP pInterface)
{
	PDRVNIC pThis = PDMINETWORKUP_2_DRVNIC(pInterface);
}


/**
 * @interface_method_impl{PDMINETWORKUP,pfnSetPromiscuousMode}
 */
static DECLCALLBACK(void) drvNicNetworkUp_SetPromiscuousMode(PPDMINETWORKUP pInterface, bool fPromiscuous)
{
	LogFlow(("drvNicNetworkUp_SetPromiscuousMode: fPromiscuous=%d\n", fPromiscuous));
	/* nothing to do */
}


/**
 * Notification on link status changes.
 */
static DECLCALLBACK(void) drvNicNetworkUp_NotifyLinkChanged(PPDMINETWORKUP pInterface, PDMNETWORKLINKSTATE enmLinkState)
{
	LogFlow(("drvNicNetworkUp_NotifyLinkChanged: enmLinkState=%d\n", enmLinkState));
	/*
	 * At this point we could stop waiting for signals etc. but for now we just do nothing.
	 */
}


static DECLCALLBACK(int) drvGetMac(PPDMINETWORKCONFIG pInterface, PRTMAC pMac)
{
	PDRVNIC pThis = PDMINETWORKCONFIG_2_DRVNIC(pInterface);
	Nic_client *nic_client = pThis->nic_client;

	static_assert (sizeof(*pMac) == sizeof(nic_client->mac_address()),
	               "should be equal");
	memcpy(pMac, nic_client->mac_address().addr, sizeof(*pMac));
	return VINF_SUCCESS;
}


/* -=-=-=-=- PDMIBASE -=-=-=-=- */

/**
 * @interface_method_impl{PDMIBASE,pfnQueryInterface}
 */
static DECLCALLBACK(void *) drvNicQueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
	PPDMDRVINS  pDrvIns = PDMIBASE_2_PDMDRV(pInterface);
	PDRVNIC     pThis   = PDMINS_2_DATA(pDrvIns, PDRVNIC);

	PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE, &pDrvIns->IBase);
	PDMIBASE_RETURN_INTERFACE(pszIID, PDMINETWORKUP, &pThis->INetworkUp);
	PDMIBASE_RETURN_INTERFACE(pszIID, PDMINETWORKCONFIG, &pThis->INetworkConfig);
	return NULL;
}


/* -=-=-=-=- PDMDRVREG -=-=-=-=- */

static DECLCALLBACK(void) drvNicDestruct(PPDMDRVINS pDrvIns)
{
	PDRVNIC pThis = PDMINS_2_DATA(pDrvIns, PDRVNIC);
	Nic_client *nic_client = pThis->nic_client;

	if (!nic_client)
		Genode::error("nic_client not valid at destruction time");

	if (nic_client)
		Genode::Signal_transmitter(nic_client->dispatcher()).submit();

	/* wait until the recv thread exits */
	destruct_blockade().block();

	if (nic_client)
		destroy(vmm_heap(), nic_client);
}


/**
 * Construct a Nic network transport driver instance.
 *
 * @copydoc FNPDMDRVCONSTRUCT
 */
static DECLCALLBACK(int) drvNicConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfg, uint32_t fFlags)
{
	PDRVNIC pThis = PDMINS_2_DATA(pDrvIns, PDRVNIC);
	PDMDRV_CHECK_VERSIONS_RETURN(pDrvIns);
	int rc;

	/*
	 * Init the static parts.
	 */
	pThis->pDrvIns                          = pDrvIns;
	/* IBase */
	pDrvIns->IBase.pfnQueryInterface        = drvNicQueryInterface;
	/* INetwork */
	pThis->INetworkUp.pfnBeginXmit          = drvNicNetworkUp_BeginXmit;
	pThis->INetworkUp.pfnAllocBuf           = drvNicNetworkUp_AllocBuf;
	pThis->INetworkUp.pfnFreeBuf            = drvNicNetworkUp_FreeBuf;
	pThis->INetworkUp.pfnSendBuf            = drvNicNetworkUp_SendBuf;
	pThis->INetworkUp.pfnEndXmit            = drvNicNetworkUp_EndXmit;
	pThis->INetworkUp.pfnSetPromiscuousMode = drvNicNetworkUp_SetPromiscuousMode;
	pThis->INetworkUp.pfnNotifyLinkChanged  = drvNicNetworkUp_NotifyLinkChanged;
	/* INetworkConfig - used on Genode to request Mac address of nic_session */
	pThis->INetworkConfig.pfnGetMac         = drvGetMac;

	/*
	 * Check that no-one is attached to us.
	 */
	AssertMsgReturn(PDMDrvHlpNoAttach(pDrvIns) == VERR_PDM_NO_ATTACHED_DRIVER,
	                ("Configuration error: Not possible to attach anything to"
	                 " this driver!\n"), VERR_PDM_DRVINS_NO_ATTACH);

	/*
	 * Query the above network port interface.
	 */
	pThis->pIAboveNet = PDMIBASE_QUERY_INTERFACE(pDrvIns->pUpBase, PDMINETWORKDOWN);
	if (!pThis->pIAboveNet)
		return PDMDRV_SET_ERROR(pDrvIns, VERR_PDM_MISSING_INTERFACE_ABOVE,
		                        N_("Configuration error: The above device/driver"
		                           " didn't export the network port interface"));
	pThis->pIAboveConfig = PDMIBASE_QUERY_INTERFACE(pDrvIns->pUpBase, PDMINETWORKCONFIG);
	if (!pThis->pIAboveConfig)
		return PDMDRV_SET_ERROR(pDrvIns, VERR_PDM_MISSING_INTERFACE_ABOVE,
		                        N_("Configuration error: the above device/driver"
		                            " didn't export the network config interface!\n"));

	char label[8];
	uint64_t slot;
	rc = CFGMR3QueryInteger(pCfg, "Slot", &slot);
	if (RT_FAILURE(rc))
		return PDMDRV_SET_ERROR(pDrvIns, rc,
		                        N_("Configuration error: Failed to retrieve the network interface slot"));
	Genode::snprintf(label, sizeof(label), "%d", slot);

	/*
	 * Setup Genode nic_session connection
	 */
	try {
		pThis->nic_client = new (vmm_heap()) Nic_client(genode_env(), pThis, label);
	} catch (...) {
		return VERR_HOSTIF_INIT_FAILED;
	}

	return 0;
}


static DECLCALLBACK(void) drvNicPowerOn(PPDMDRVINS pDrvIns)
{
	PDRVNIC pThis = PDMINS_2_DATA(pDrvIns, PDRVNIC);

	if (pThis && pThis->nic_client)
		pThis->nic_client->enable_signals();
}

/**
 * Nic network transport driver registration record.
 */
const PDMDRVREG g_DrvHostInterface =
{
	/* u32Version */
	PDM_DRVREG_VERSION,
	/* szName */
	"HostInterface",
	/* szRCMod */
	"",
	/* szR0Mod */
	"",
	/* pszDescription */
	"Genode Network Session Driver",
	/* fFlags */
	PDM_DRVREG_FLAGS_HOST_BITS_DEFAULT,
	/* fClass. */
	PDM_DRVREG_CLASS_NETWORK,
	/* cMaxInstances */
	~0U,
	/* cbInstance */
	sizeof(DRVNIC),
	/* pfnConstruct */
	drvNicConstruct,
	/* pfnDestruct */
	drvNicDestruct,
	/* pfnRelocate */
	NULL,
	/* pfnIOCtl */
	NULL,
	drvNicPowerOn,
	/* pfnReset */
	NULL,
	/* pfnSuspend */
	NULL,
	/* pfnResume */
	NULL,
	/* pfnAttach */
	NULL,
	/* pfnDetach */
	NULL,
	/* pfnPowerOff */
	NULL,
	/* pfnSoftReset */
	NULL,
	/* u32EndVersion */
	PDM_DRVREG_VERSION
};
