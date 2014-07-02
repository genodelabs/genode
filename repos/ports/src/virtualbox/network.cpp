/**
 * Genode network session driver,
 * derived from src/VBox/Devices/Network/DrvTAP.cpp.
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
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
#include <VBox/log.h>
#include <VBox/vmm/pdmdrv.h>
#include <VBox/vmm/pdmnetifs.h>
#include <VBox/vmm/pdmnetinline.h>

#include <iprt/mem.h>
#include <iprt/uuid.h>

#include "VBoxDD.h"

#include <nic_session/connection.h>
#include <nic/packet_allocator.h>

/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * TAP driver instance data.
 *
 * @implements PDMINETWORKUP
 */
typedef struct DRVTAP
{
    /** The network interface. */
    PDMINETWORKUP           INetworkUp;
    /** The network interface. */
    PPDMINETWORKDOWN        pIAboveNet;
    /** Pointer to the driver instance. */
    PPDMDRVINS              pDrvIns;
    /** Reader thread. */
    PPDMTHREAD              pThread;

    /** @todo The transmit thread. */
    /** Transmit lock used by drvTAPNetworkUp_BeginXmit. */
    RTCRITSECT              XmitLock;

	Nic::Session           *nic_session;

	PDMINETWORKCONFIG      INetworkConfig;

} DRVTAP, *PDRVTAP;


/** Converts a pointer to TAP::INetworkUp to a PRDVTAP. */
#define PDMINETWORKUP_2_DRVTAP(pInterface) ( (PDRVTAP)((uintptr_t)pInterface - RT_OFFSETOF(DRVTAP, INetworkUp)) )
#define PDMINETWORKCONFIG_2_DRVTAP(pInterface) ( (PDRVTAP)((uintptr_t)pInterface - RT_OFFSETOF(DRVTAP, INetworkConfig)) )


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/

static int net_send_packet(void * packet, uint32_t packet_len, Nic::Session * nic) {

	/* allocate transmit packet */
	Packet_descriptor tx_packet;
	try {
		tx_packet = nic->tx()->alloc_packet(packet_len);
	} catch (Nic::Session::Tx::Source::Packet_alloc_failed) {
		PERR("tx packet alloc failed");
        return VERR_NO_MEMORY;
	}

	/* fill packet with content */
	char * stream_buffer = nic->tx()->packet_content(tx_packet);
	memcpy(stream_buffer, packet, packet_len);

	nic->tx()->submit_packet(tx_packet);

	/* wait for acknowledgement */
	Packet_descriptor ack_tx_packet = nic->tx()->get_acked_packet();

	if (ack_tx_packet.size()   != tx_packet.size() ||
	    ack_tx_packet.offset() != tx_packet.offset())
		PERR("unexpected acked packet");

	nic->tx()->release_packet(tx_packet);

	return VINF_SUCCESS;
}

/**
 * @interface_method_impl{PDMINETWORKUP,pfnBeginXmit}
 */
static DECLCALLBACK(int) drvTAPNetworkUp_BeginXmit(PPDMINETWORKUP pInterface, bool fOnWorkerThread)
{
    PDRVTAP pThis = PDMINETWORKUP_2_DRVTAP(pInterface);
    int rc = RTCritSectTryEnter(&pThis->XmitLock);
    if (RT_FAILURE(rc))
    {
        /** @todo XMIT thread */
        rc = VERR_TRY_AGAIN;
    }
    return rc;
}


/**
 * @interface_method_impl{PDMINETWORKUP,pfnAllocBuf}
 */
static DECLCALLBACK(int) drvTAPNetworkUp_AllocBuf(PPDMINETWORKUP pInterface, size_t cbMin,
                                                  PCPDMNETWORKGSO pGso, PPPDMSCATTERGATHER ppSgBuf)
{
    PDRVTAP pThis = PDMINETWORKUP_2_DRVTAP(pInterface);
    Assert(RTCritSectIsOwner(&pThis->XmitLock));

    /*
     * Allocate a scatter / gather buffer descriptor that is immediately
     * followed by the buffer space of its single segment.  The GSO context
     * comes after that again.
     */
    PPDMSCATTERGATHER pSgBuf = (PPDMSCATTERGATHER)RTMemAlloc(  RT_ALIGN_Z(sizeof(*pSgBuf), 16)
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

#if 0 /* poison */
    memset(pSgBuf->aSegs[0].pvSeg, 'F', pSgBuf->aSegs[0].cbSeg);
#endif
    *ppSgBuf = pSgBuf;
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMINETWORKUP,pfnFreeBuf}
 */
static DECLCALLBACK(int) drvTAPNetworkUp_FreeBuf(PPDMINETWORKUP pInterface, PPDMSCATTERGATHER pSgBuf)
{
    PDRVTAP pThis = PDMINETWORKUP_2_DRVTAP(pInterface);
    Assert(RTCritSectIsOwner(&pThis->XmitLock));
    if (pSgBuf)
    {
        Assert((pSgBuf->fFlags & PDMSCATTERGATHER_FLAGS_MAGIC_MASK) == PDMSCATTERGATHER_FLAGS_MAGIC);
        pSgBuf->fFlags = 0;
        RTMemFree(pSgBuf);
    }
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMINETWORKUP,pfnSendBuf}
 */
static DECLCALLBACK(int) drvTAPNetworkUp_SendBuf(PPDMINETWORKUP pInterface, PPDMSCATTERGATHER pSgBuf, bool fOnWorkerThread)
{
    PDRVTAP pThis = PDMINETWORKUP_2_DRVTAP(pInterface);

    AssertPtr(pSgBuf);
    Assert((pSgBuf->fFlags & PDMSCATTERGATHER_FLAGS_MAGIC_MASK) == PDMSCATTERGATHER_FLAGS_MAGIC);
    Assert(RTCritSectIsOwner(&pThis->XmitLock));

    /* Set an FTM checkpoint as this operation changes the state permanently. */
    PDMDrvHlpFTSetCheckpoint(pThis->pDrvIns, FTMCHECKPOINTTYPE_NETWORK);

    int rc;
    if (!pSgBuf->pvUser)
    {
        Log2(("drvTAPSend: pSgBuf->aSegs[0].pvSeg=%p pSgBuf->cbUsed=%#x\n"
              "%.*Rhxd\n",
              pSgBuf->aSegs[0].pvSeg, pSgBuf->cbUsed, pSgBuf->cbUsed, pSgBuf->aSegs[0].pvSeg));

		rc = net_send_packet(pSgBuf->aSegs[0].pvSeg, pSgBuf->cbUsed, pThis->nic_session);
    }
    else
    {
        uint8_t         abHdrScratch[256];
        uint8_t const  *pbFrame = (uint8_t const *)pSgBuf->aSegs[0].pvSeg;
        PCPDMNETWORKGSO pGso    = (PCPDMNETWORKGSO)pSgBuf->pvUser;
        uint32_t const  cSegs   = PDMNetGsoCalcSegmentCount(pGso, pSgBuf->cbUsed);  Assert(cSegs > 1);
        rc = VINF_SUCCESS;
        for (size_t iSeg = 0; iSeg < cSegs; iSeg++)
        {
            uint32_t cbSegFrame;
            void *pvSegFrame = PDMNetGsoCarveSegmentQD(pGso, (uint8_t *)pbFrame, pSgBuf->cbUsed, abHdrScratch,
                                                       iSeg, cSegs, &cbSegFrame);
			rc = net_send_packet(pvSegFrame, cbSegFrame, pThis->nic_session);
            if (RT_FAILURE(rc))
                break;
        }
    }

    pSgBuf->fFlags = 0;
    RTMemFree(pSgBuf);

    AssertRC(rc);
    if (RT_FAILURE(rc))
        rc = rc == VERR_NO_MEMORY ? VERR_NET_NO_BUFFER_SPACE : VERR_NET_DOWN;
    return rc;
}


/**
 * @interface_method_impl{PDMINETWORKUP,pfnEndXmit}
 */
static DECLCALLBACK(void) drvTAPNetworkUp_EndXmit(PPDMINETWORKUP pInterface)
{
    PDRVTAP pThis = PDMINETWORKUP_2_DRVTAP(pInterface);
    RTCritSectLeave(&pThis->XmitLock);
}


/**
 * @interface_method_impl{PDMINETWORKUP,pfnSetPromiscuousMode}
 */
static DECLCALLBACK(void) drvTAPNetworkUp_SetPromiscuousMode(PPDMINETWORKUP pInterface, bool fPromiscuous)
{
    LogFlow(("drvTAPNetworkUp_SetPromiscuousMode: fPromiscuous=%d\n", fPromiscuous));
    /* nothing to do */
}


/**
 * Notification on link status changes.
 *
 * @param   pInterface      Pointer to the interface structure containing the called function pointer.
 * @param   enmLinkState    The new link state.
 * @thread  EMT
 */
static DECLCALLBACK(void) drvTAPNetworkUp_NotifyLinkChanged(PPDMINETWORKUP pInterface, PDMNETWORKLINKSTATE enmLinkState)
{
    LogFlow(("drvTAPNetworkUp_NotifyLinkChanged: enmLinkState=%d\n", enmLinkState));
    /** @todo take action on link down and up. Stop the polling and such like. */
}



static DECLCALLBACK(int) drvGetMac(PPDMINETWORKCONFIG pInterface, PRTMAC pMac)
{
	PDRVTAP pThis = PDMINETWORKCONFIG_2_DRVTAP(pInterface);

	static_assert (sizeof(*pMac) == sizeof(pThis->nic_session->mac_address()), 
	               "should be equal");
	memcpy(pMac, pThis->nic_session->mac_address().addr, sizeof(*pMac));
	return VINF_SUCCESS;
}


/**
 * Asynchronous I/O thread for handling receive.
 *
 * @returns VINF_SUCCESS (ignored).
 * @param   Thread          Thread handle.
 * @param   pvUser          Pointer to a DRVTAP structure.
 */
static DECLCALLBACK(int) drvTAPAsyncIoThread(PPDMDRVINS pDrvIns, PPDMTHREAD pThread)
{
    PDRVTAP pThis = PDMINS_2_DATA(pDrvIns, PDRVTAP);
    LogFlow(("drvTAPAsyncIoThread: pThis=%p\n", pThis));

    if (pThread->enmState == PDMTHREADSTATE_INITIALIZING)
        return VINF_SUCCESS;

	Nic::Session * nic = pThis->nic_session;

    while (pThread->enmState == PDMTHREADSTATE_RUNNING)
	{
		Packet_descriptor rx_packet = nic->rx()->get_packet();

		/* send it to the network bus */
		char * rx_content = nic->rx()->packet_content(rx_packet);

		int rc1 = pThis->pIAboveNet->pfnWaitReceiveAvail(pThis->pIAboveNet,
		                                                 RT_INDEFINITE_WAIT);
		if (RT_FAILURE(rc1))
			continue;

		rc1 = pThis->pIAboveNet->pfnReceive(pThis->pIAboveNet, rx_content,
		                                    rx_packet.size());
		AssertRC(rc1);

		/* acknowledge received packet */
		nic->rx()->acknowledge_packet(rx_packet);
	}

    return VINF_SUCCESS;
}


/**
 * Unblock the send thread so it can respond to a state change.
 *
 * @returns VBox status code.
 * @param   pDevIns     The pcnet device instance.
 * @param   pThread     The send thread.
 */
static DECLCALLBACK(int) drvTapAsyncIoWakeup(PPDMDRVINS pDrvIns, PPDMTHREAD pThread)
{
	PDRVTAP pThis = PDMINS_2_DATA(pDrvIns, PDRVTAP);
	Assert(!"Not implemented");
	return VINF_SUCCESS;
}


/* -=-=-=-=- PDMIBASE -=-=-=-=- */

/**
 * @interface_method_impl{PDMIBASE,pfnQueryInterface}
 */
static DECLCALLBACK(void *) drvTAPQueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
    PPDMDRVINS  pDrvIns = PDMIBASE_2_PDMDRV(pInterface);
    PDRVTAP     pThis   = PDMINS_2_DATA(pDrvIns, PDRVTAP);

    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE, &pDrvIns->IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMINETWORKUP, &pThis->INetworkUp);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMINETWORKCONFIG, &pThis->INetworkConfig);
    return NULL;
}

/* -=-=-=-=- PDMDRVREG -=-=-=-=- */

static DECLCALLBACK(void) drvTAPDestruct(PPDMDRVINS pDrvIns)
{
	Assert(!"Not implemented");
}

/**
 * Construct a TAP network transport driver instance.
 *
 * @copydoc FNPDMDRVCONSTRUCT
 */
static DECLCALLBACK(int) drvTAPConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfg, uint32_t fFlags)
{
    PDRVTAP pThis = PDMINS_2_DATA(pDrvIns, PDRVTAP);
    PDMDRV_CHECK_VERSIONS_RETURN(pDrvIns);

    /*
     * Init the static parts.
     */
    pThis->pDrvIns                      = pDrvIns;

    /* IBase */
    pDrvIns->IBase.pfnQueryInterface    = drvTAPQueryInterface;
    /* INetwork */
    pThis->INetworkUp.pfnBeginXmit              = drvTAPNetworkUp_BeginXmit;
    pThis->INetworkUp.pfnAllocBuf               = drvTAPNetworkUp_AllocBuf;
    pThis->INetworkUp.pfnFreeBuf                = drvTAPNetworkUp_FreeBuf;
    pThis->INetworkUp.pfnSendBuf                = drvTAPNetworkUp_SendBuf;
    pThis->INetworkUp.pfnEndXmit                = drvTAPNetworkUp_EndXmit;
    pThis->INetworkUp.pfnSetPromiscuousMode     = drvTAPNetworkUp_SetPromiscuousMode;
    pThis->INetworkUp.pfnNotifyLinkChanged      = drvTAPNetworkUp_NotifyLinkChanged;

	/* INetworkConfig - used on Genode to request Mac address of nic_session */
	pThis->INetworkConfig.pfnGetMac             = drvGetMac;

	/*
	 * Setup Genode nic_session connection
	 */
	Nic::Packet_allocator *tx_block_alloc =
		new (Genode::env()->heap()) Nic::Packet_allocator(Genode::env()->heap());

	enum {
		PACKET_SIZE = Nic::Packet_allocator::DEFAULT_PACKET_SIZE,
		BUF_SIZE    = Nic::Session::QUEUE_SIZE * PACKET_SIZE,
	};

	try {
		pThis->nic_session = new Nic::Connection(tx_block_alloc, BUF_SIZE,
		                                         BUF_SIZE);
	} catch (...) {
		return VERR_HOSTIF_INIT_FAILED;
	}

    /*
     * Check that no-one is attached to us.
     */
    AssertMsgReturn(PDMDrvHlpNoAttach(pDrvIns) == VERR_PDM_NO_ATTACHED_DRIVER,
                    ("Configuration error: Not possible to attach anything to this driver!\n"),
                    VERR_PDM_DRVINS_NO_ATTACH);

    /*
     * Query the network port interface.
     */
    pThis->pIAboveNet = PDMIBASE_QUERY_INTERFACE(pDrvIns->pUpBase, PDMINETWORKDOWN);
    if (!pThis->pIAboveNet)
        return PDMDRV_SET_ERROR(pDrvIns, VERR_PDM_MISSING_INTERFACE_ABOVE,
                                N_("Configuration error: The above device/driver didn't export the network port interface"));

    /*
     * Create the transmit lock.
     */
    int rc = RTCritSectInit(&pThis->XmitLock);
    AssertRCReturn(rc, rc);

    /*
     * Create the async I/O thread.
     */
    rc = PDMDrvHlpThreadCreate(pDrvIns, &pThis->pThread, pThis, drvTAPAsyncIoThread, drvTapAsyncIoWakeup, 128 * _1K, RTTHREADTYPE_IO, "TAP");
    AssertRCReturn(rc, rc);

	return rc;
}


/**
 * TAP network transport driver registration record.
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
    sizeof(DRVTAP),
    /* pfnConstruct */
    drvTAPConstruct,
    /* pfnDestruct */
    drvTAPDestruct,
    /* pfnRelocate */
    NULL,
    /* pfnIOCtl */
    NULL,
    /* pfnPowerOn */
    NULL,
    /* pfnReset */
    NULL,
    /* pfnSuspend */
    NULL, /** @todo Do power on, suspend and resume handlers! */
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
