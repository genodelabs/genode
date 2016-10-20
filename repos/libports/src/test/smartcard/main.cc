/*
 * \brief  pcsc-lite test
 * \author Christian Prochaska
 * \date   2016-09-19
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <stdio.h>
#include <string.h>

#include <winscard.h>

int main()
{
	LONG rv;

	SCARDCONTEXT hContext;

	LPTSTR mszReaders;
	DWORD dwReaders;

	SCARD_READERSTATE state;

	SCARDHANDLE hCard;
	DWORD dwActiveProtocol;

	SCARD_IO_REQUEST pioSendPci;

	/* SELECT FILE 0x3F00 */
	BYTE cmd[] = { 0x00, 0xA4, 0x00, 0x00, 0x02, 0x3F, 0x00 };

	BYTE pbRecvBuffer[256];
	DWORD dwRecvLength;

	rv = SCardEstablishContext(SCARD_SCOPE_SYSTEM, NULL, NULL, &hContext);

	printf("SCardEstablishContext: 0x%lx\n", rv);

	dwReaders = SCARD_AUTOALLOCATE;
	rv = SCardListReaders(hContext, NULL, (LPTSTR)&mszReaders, &dwReaders);

	printf("SCardListreaders: 0x%lx, %lu, %s\n", rv, dwReaders, mszReaders);

	memset(&state, 0, sizeof state);
	state.szReader = mszReaders;
 	rv = SCardGetStatusChange(hContext, 0, &state, 1);

	printf("SCardGetStatusChange(): 0x%lx, %lx\n", rv, state.dwEventState);

	while (state.dwEventState & SCARD_STATE_EMPTY) {

		state.dwCurrentState = state.dwEventState;

	 	rv = SCardGetStatusChange(hContext, INFINITE, &state, 1);

		printf("SCardGetStatusChange(): 0x%lx, %lx\n", rv, state.dwEventState);
	}

	rv = SCardConnect(hContext, mszReaders, SCARD_SHARE_EXCLUSIVE, SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1, &hCard, &dwActiveProtocol);

	printf("SCardConnect: 0x%lx, %lu\n", rv, dwActiveProtocol);

	switch (dwActiveProtocol) {
		case SCARD_PROTOCOL_T0:
			printf("Protocol: T0\n");
			pioSendPci = *SCARD_PCI_T0;
			break;
		case SCARD_PROTOCOL_T1:
			printf("Protocol: T1\n");
			pioSendPci = *SCARD_PCI_T1;
			break;
	}

	dwRecvLength = sizeof(pbRecvBuffer);

	rv = SCardTransmit(hCard, &pioSendPci, cmd, sizeof(cmd), NULL, pbRecvBuffer, &dwRecvLength);

	printf("SCardTransmit: 0x%lx\n", rv);

	printf("Response: ");
	for (unsigned int i=0; i < dwRecvLength; i++)
		printf("%02X ", pbRecvBuffer[i]);
	printf("\n");

	rv = SCardDisconnect(hCard, SCARD_LEAVE_CARD);

	printf("SCardDisconnect: 0x%lx\n", rv);

	rv = SCardFreeMemory(hContext, mszReaders);

	printf("SCardFreeMemory: 0x%lx\n", rv);

	rv = SCardReleaseContext(hContext);
	
	printf("SCardReleaseContext: 0x%lx\n", rv);

	return 0;
}
