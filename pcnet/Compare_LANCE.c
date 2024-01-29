/*++

Copyright (c) 1996 ADVANCED MICRO DEVICES, INC. All Rights Reserved.
This software is unpblished and contains the trade secrets and
confidential proprietary information of AMD. Unless otherwise provided
in the Software Agreement associated herewith, it is licensed in confidence
"AS IS" and is not to be reproduced in whole or part by any means except
for backup. Use, duplication, or disclosure by the Government is subject
to the restrictions in paragraph (b) (3) (B) of the Rights in Technical
Data and Computer Software clause in DFAR 52.227-7013 (a) (Oct 1988).
Software owned by Advanced Micro Devices, Inc., 901 Thompson Place,
Sunnyvale, CA 94088.

Module Name:

	lance.c

Abstract:

	This is the main file for the Advanced Micro Devices LANCE
	Ethernet controller NDIS Device Driver.

Environment:
	Kernel Mode - Or whatever is the equivalent on OS/2 and DOS.

Revision History:

$Log:   V:/pvcs/archives/network/pcnet/permanet/src/LANCE.C_v  $
 * 
 *    Rev 1.1   Jul 31 1998 11:59:46   kishore
 * Certification (HCT 7.6) bugs fixed. Dynamic
 * Config Test Problem fixed. Managed Mode
 * of operation (hot plug support) added. 3
 * new groups added to pcnet.mif. Automatic
 * DMI installation (oemsetup.inf modified).
 * 
 * 
 *    Rev 1.63   01 Oct 1997 18:46:48   steiger
 * Changed version to 1.01.006
 * Fixed LanceCheckIrqDma() -- was broken for > 1 
 * adapter installed in a system; causes a conflict
 * to be reported on the 2nd adapter.
 * 
 * 
 *    Rev 1.62   30 Sep 1997 15:17:40   steiger
 * Added LanceMutex function for debug use only.
 * 
 * 
 * 
 *    Rev 1.61   02 Sep 1997 16:20:52   steiger
 * Modified LanceSetupRegistersAndInit () such that the
 * address of LanceRMDFlags is written to the
 * MiniportReserved field of the NDIS packet
 * descriptor. It was being written in LanceHandleInterrupt ()
 * previously.
 * 
 * 
 *    Rev 1.60   12 Aug 1997 17:51:00   steiger
 * Multi-RX implemented. Copy/Compare testing has been done.
 * 
 *    Rev 1.59   31 Jul 1997 14:52:12   steiger
 * Interim check-in.
 * -Multi-send implemented.
 * -Interrupt routine cleaned up somewhat.
 * -Send routine cleaned up.
 *  
 * 
 *      Rev 1.58        10 Jul 1997 17:22:58    steiger
 * Cleaned up LED init.
 * Cleaned up Full Duplex init.
 * Added real-time line speed detection for support of OID_GEN_LINK_SPEED.
 * Added code to ignore TP keyword on adapters >= Laguna.
 * Finished multi-modal link monitor.
 * 
 * 
 *      Rev 1.58        10 Jul 1997 17:08:58    steiger
 *      
 * 
 *      Rev 1.58        10 Jul 1997 17:01:34    steiger
 * Cleaned up LED and Full Duplex init.
 * Added real-time line speed detection for support of OID_GEN_LINK_SPEED.
 * 
 * 
 *      Rev 1.57        07 Jul 1997 17:53:44    steiger
 * Interim check in -- Added multi-modal link status check.
 * Redundant mode is affected by this change.
 * Redundant mode is untested.
 * 
 *      Rev 1.56        30 Jun 1997 18:27:20    steiger
 * Completed support for external PHY media selection.
 * 
 *      Rev 1.53        07 May 1997 19:10:54    steiger
 * Removed yet another stray unwanted conditional.
 * 
 *      Rev 1.52        07 May 1997 19:04:58    steiger
 * Removed conditional statements for redundancy, replaced with "NDIS40_MINIPORT"

--*/

#define DBG 1
#define MYDBG 1


#include <ndis.h>
#include <efilter.h>
#include <lancehrd.h>
#include <lancesft.h>
#include "lance.msg"

#if DBG
INT LanceDbg = 1;
/* APM support */
INT LanceAPMDbg = 0;
INT LanceSendDbg = 1;
INT LanceExtPhyDbg = 1;
INT LanceEventDbg = 1;
INT LanceRxDbg = 1;
INT LanceFilterDbg = 0;
INT LanceBreak = 0;
#define STATIC

#if LANCELOG
NDIS_TIMER LogTimer;
BOOLEAN LogTimerRunning = FALSE;
UCHAR Log[LOG_SIZE];
UCHAR LogPlace = 0;
//UCHAR LogWrapped = 0;
//UCHAR LancePrintLog = 0;
#endif

#endif

ULONG LED_defaults[] = {0x00c0l, 0x0090l, 0x0088l, 0x0081l};
#if DBG    /* Used for Mutex macro in LANCESFT.H */
UCHAR Mutex;
#endif
/* Global data structures for the miniport.     */
NDIS_HANDLE LanceWrapperHandle;

/* Ensure that there are MAX_ADAPTERS zeroes in each of the following three arrays. */
ULONG AdapterIo[MAX_ADAPTERS] = {0, 0, 0, 0};
UCHAR AdapterIrq[MAX_ADAPTERS] = {0, 0, 0, 0};
UCHAR AdapterDma[MAX_ADAPTERS] = {0, 0, 0, 0};

CHAR *Copyright = "Copyright(c) 1996 ADVANCED MICRO DEVICES, INC.";
//BOOLEAN LinkActive = TRUE;

/* Function prototypes  */
STATIC
NDIS_STATUS
LanceInitialize(
	OUT PNDIS_STATUS OpenErrorStatus,
	OUT PUINT SelectedMediumIndex,
	IN PNDIS_MEDIUM MediumArray,
	IN UINT MediumArraySize,
	IN NDIS_HANDLE LanceMiniportHandle,
	IN NDIS_HANDLE ConfigurationHandle
	);

STATIC
NDIS_STATUS
LanceRegisterAdapter(
	IN NDIS_HANDLE LanceMiniportHandle,
	IN NDIS_HANDLE ConfigurationHandle,
	IN PUCHAR NetworkAddress,
	IN PLANCE_ADAPTER Adapter
	);

STATIC
ULONG
LanceHardwareDetails(
	IN PLANCE_ADAPTER Adapter,
	IN NDIS_HANDLE ConfigurationHandle
	);

STATIC
BOOLEAN
LanceCheckIoAddress(
	IN ULONG IoAddress
	);

STATIC
BOOLEAN
LanceCheckIrqDmaValid(
	IN PLANCE_ADAPTER Adapter
	);

STATIC
VOID
LanceCleanResources(
	IN PLANCE_ADAPTER Adapter
	);

STATIC
VOID
LancePciEnableDma(
	IN PLANCE_ADAPTER Adapter
	);

STATIC
VOID
LancePciDisableDma(
	IN PLANCE_ADAPTER Adapter
	);


STATIC
VOID
LanceSetPciDma(
	IN PLANCE_ADAPTER Adapter,
	BOOLEAN EnablePciDma
	);

STATIC
VOID
LanceStartChip(
	IN PLANCE_ADAPTER Adapter
	);

STATIC
NDIS_STATUS
LanceReset(
	OUT PBOOLEAN AddressingReset,
	IN NDIS_HANDLE MiniportAdapterContext
	);


STATIC
VOID
LanceShutdownHandler(
	IN PVOID Context
	);

STATIC
VOID
LanceHalt(
	IN NDIS_HANDLE Context
	);

STATIC
VOID
LanceIoMoveSramData(
	IN PLANCE_ADAPTER Adapter
	);

STATIC
NDIS_STATUS
LanceTransferData(
	OUT PNDIS_PACKET Packet,
	OUT PUINT BytesTransferred,
	IN NDIS_HANDLE MiniportAdapterContext,
	IN NDIS_HANDLE MiniportReceiveContext,
	IN UINT ByteOffset,
	IN UINT BytesToTransfer
	);

STATIC
VOID
InitLEDs (
	IN PLANCE_ADAPTER Adapter
	);

STATIC
VOID
InitFullDuplexMode (
	IN PLANCE_ADAPTER Adapter
	);

#ifdef NDIS40_MINIPORT

STATIC
VOID
LanceFreeNdisPkts (
	IN PLANCE_ADAPTER Adapter
	);

STATIC
VOID
LanceSetExtPhyMedia(
	IN ULONG IoBaseAddress,
	IN ULONG ExtPhyMode
	);


VOID
LanceLinkMonitor(
	IN PVOID        SystemSpecific1,
	IN PLANCE_ADAPTER Adapter,
	IN PVOID        SystemSpecific2,
	IN PVOID        SystemSpecific3
	);

STATIC
BOOLEAN
IntPhyLinkStatus (
	IN ULONG IoBaseAddress
	);

STATIC
BOOLEAN
ExtPhyLinkStatus (
	IN ULONG IoBaseAddress
	);

STATIC
BOOLEAN
LanceInitRxPacketPool(
	IN PLANCE_ADAPTER Adapter
	);

STATIC
VOID
LanceFreeRxPacketPool(
	IN PLANCE_ADAPTER Adapter
	);

#endif /* NDIS40_MINIPORT */

NTSTATUS
DriverEntry(
	IN PDRIVER_OBJECT DriverObject,
	IN PUNICODE_STRING RegistryPath
	)

/*++

Routine Description:

	This is the primary initialization routine for the Lance driver.
	It is simply responsible for the intializing the wrapper and registering
	the MAC.        It then calls a system and architecture specific routine that
	will initialize and register each adapter.

Arguments:

	DriverObject - Pointer to driver object created by the system.

Return Value:

	The status of the operation.

--*/

{

	//
	// Receives the status of the NdisRegisterMac operation.
	//
	NDIS_STATUS Status;

	//
	// Allocate memory for data structure
	//
	NDIS_MINIPORT_CHARACTERISTICS LanceChar;

	//
	// Driver name
	//
	NDIS_STRING MacName = NDIS_STRING_CONST("PCNTN4M");

	#ifdef MYDBG
		if (LanceDbg)
		{
			DbgPrint("==>DriverEntry\n");
			if (LanceBreak)
				_asm int 3;
		}
	#endif

	//
	// Initialize the wrapper.
	//
	NdisMInitializeWrapper(
		&LanceWrapperHandle,
		DriverObject,
		RegistryPath,
		NULL
		);
	
	//
	// Initialize the MAC characteristics for the call to
	// NdisRegisterMac.
	//
	LanceChar.MajorNdisVersion                      = LANCE_NDIS_MAJOR_VERSION;
	LanceChar.MinorNdisVersion                      = LANCE_NDIS_MINOR_VERSION;
	LanceChar.CheckForHangHandler           = NULL; //LanceCheckForHang;
	LanceChar.DisableInterruptHandler       = LanceDisableInterrupt;
	LanceChar.EnableInterruptHandler        = LanceEnableInterrupt;
	LanceChar.HaltHandler                           = LanceHalt;
	LanceChar.HandleInterruptHandler        = LanceHandleInterrupt;
	LanceChar.InitializeHandler                     = LanceInitialize;
	LanceChar.ISRHandler                            = LanceISR;
	LanceChar.QueryInformationHandler       = LanceQueryInformation;
	LanceChar.ReconfigureHandler            = NULL;
	LanceChar.ResetHandler                          = LanceReset;
	LanceChar.SetInformationHandler         = LanceSetInformation;
	LanceChar.TransferDataHandler           = LanceTransferData;

#ifdef NDIS40_MINIPORT

	// Extensions for NDIS 4.0
	// Keep the fields above consistent with NDIS 3.0 miniport characteristics
	//
	LanceChar.ReturnPacketHandler           = LanceReturnPacket;
	LanceChar.AllocateCompleteHandler       = NULL;
	LanceChar.SendHandler                           = NULL;
	LanceChar.SendPacketsHandler            = LanceSendPackets;

#else

	LanceChar.ReturnPacketHandler           = NULL;
	LanceChar.AllocateCompleteHandler       = NULL;
	LanceChar.SendHandler                           = LanceSend;
	LanceChar.SendPacketsHandler            = NULL;

#endif

	//
	// Register the miniport driver
	//
	Status = NdisMRegisterMiniport(
		LanceWrapperHandle,
		&LanceChar,
		sizeof(LanceChar)
		);

	//
	// If ok, done
	//
	if (Status == NDIS_STATUS_SUCCESS)
	{
		#ifdef MYDBG
		if (LanceDbg)
			DbgPrint("<==DriverEntry\n");
		#endif

		return Status;
	}

	//
	// We can only get here if something went wrong with registering
	// the miniport driver or *all* of the adapters.
	//
	NdisTerminateWrapper(LanceWrapperHandle, NULL);

	#ifdef MYDBG
		if (LanceDbg)
		DbgPrint("DriverEntry Fail, NdisMRegisterMiniport\n");
	#endif

	return NDIS_STATUS_FAILURE;

}

STATIC
NDIS_STATUS
LanceInitialize(
	OUT PNDIS_STATUS OpenErrorStatus,
	OUT PUINT SelectedMediumIndex,
	IN PNDIS_MEDIUM MediumArray,
	IN UINT MediumArraySize,
	IN NDIS_HANDLE LanceMiniportHandle,
	IN NDIS_HANDLE ConfigurationHandle
	)

/*++

Routine Description:

	Initialize an adapter.

Arguments:

	OpenErrorStatus - Extra status bytes for opening adapters.

	SelectedMediumIndex - Index of the media type chosen by the driver.

	MediumArray - Array of media types for the driver to chose from.

	MediumArraySize - Number of entries in the array.

	LanceMiniportHandle - Handle for passing to the wrapper when
		referring to this adapter.

	ConfigurationHandle - A handle to pass to NdisOpenConfiguration.

Return Value:

	NDIS_STATUS_SUCCESS
	NDIS_STATUS_PENDING

--*/

{

	//
	// Text id strings
	//
	NDIS_STRING NetworkAddressString = NDIS_STRING_CONST("NETADDRESS");
	NDIS_STRING BaseAddressString = NDIS_STRING_CONST("IOAddress");
	NDIS_STRING InterruptVectorString = NDIS_STRING_CONST("Interrupt");
	NDIS_STRING DmaChannelString = NDIS_STRING_CONST("DmaChannel");
	NDIS_STRING BusTypeString = NDIS_STRING_CONST("BusType");
	NDIS_STRING BusScanString = NDIS_STRING_CONST("BUS_TO_SCAN");
	NDIS_STRING FullDuplexString = NDIS_STRING_CONST("FDUP");
	NDIS_STRING RedundantModeString = NDIS_STRING_CONST("REDMODE");
	NDIS_STRING ExternalPhyModeString = NDIS_STRING_CONST("EXTPHY");
	NDIS_STRING TPString = NDIS_STRING_CONST("TP");
	NDIS_STRING MPString = NDIS_STRING_CONST("MPMODE");
	NDIS_STRING LED0String = NDIS_STRING_CONST("LED0");
	NDIS_STRING LED1String = NDIS_STRING_CONST("LED1");
	NDIS_STRING LED2String = NDIS_STRING_CONST("LED2");
	NDIS_STRING LED3String = NDIS_STRING_CONST("LED3");
	NDIS_STRING BusTimerString = NDIS_STRING_CONST("BUSTIMER");
	NDIS_STRING SlotNumberString = NDIS_STRING_CONST("SlotNumber");
	NDIS_STRING BusNumberString = NDIS_STRING_CONST("BusNumber");

	NDIS_STRING BusComppci = NDIS_STRING_CONST("PCI");
	NDIS_STRING BusComppnp = NDIS_STRING_CONST("PNP");
	NDIS_STRING BusCompvesa = NDIS_STRING_CONST("VESA");
	NDIS_STRING BusCompisa = NDIS_STRING_CONST("ISA");

	//
	// Local variables
	//
	NDIS_STATUS Status;
	NDIS_HANDLE ConfigHandle;
	PNDIS_CONFIGURATION_PARAMETER ReturnedValue;
	UCHAR NetAddress[ETH_LENGTH_OF_ADDRESS];
	PLANCE_ADAPTER Adapter;
	UCHAR sum, count;
	PUCHAR NetworkAddress;
	UINT NetworkAddressLength;
	ULONG anchorid = 0;
	ULONG anchorio = 0;
	USHORT  Data;

	//
	// Set break point here.        This routine may be called
	// several times depending on the number of device entries
	// inside registry
	//
	
	#ifdef MYDBG
		if (LanceDbg)
			DbgPrint("==>LanceInitialize\n");
	#endif

	/* Search for correct medium.   */
	for (; MediumArraySize > 0; MediumArraySize--)
	{
		if (MediumArray[MediumArraySize - 1] == NdisMedium802_3)
		{
			MediumArraySize--;
			break;
		}
	}
	#ifdef MYDBG
		if (LanceBreak)
			_asm int 3;
	#endif

	if (MediumArray[MediumArraySize] != NdisMedium802_3)
	{
#ifdef MYDBG
		if (LanceDbg)
			DbgPrint("<==LanceInitialize\nABORT: NDIS_STATUS_UNSUPPORTED_MEDIA\n");
#endif
		return NDIS_STATUS_UNSUPPORTED_MEDIA;
	}

	*SelectedMediumIndex = MediumArraySize;

	/* Allocate the adapter block   */
	LANCE_ALLOC_MEMORY(&Status, &Adapter, sizeof(LANCE_ADAPTER));

	if (Status == NDIS_STATUS_SUCCESS)
	{
		NdisZeroMemory(Adapter, sizeof(LANCE_ADAPTER));
	
		/* Save driver handles  */
		Adapter->LanceMiniportHandle = LanceMiniportHandle;
		Adapter->LanceWrapperHandle = LanceWrapperHandle;

		/* Set default values.  All the fields are initialized  */
		/* to zero by NdisZeroMemory before     */
		Adapter->led0 = LED_DEFAULT;
		Adapter->led1 = LED_DEFAULT;
		Adapter->led2 = LED_DEFAULT;
		Adapter->led3 = LED_DEFAULT;
		Adapter->FullDuplex = FDUP_DEFAULT;
		Adapter->BusTimer = BUSTIMER_DEFAULT;
		Adapter->LanceHardwareConfigStatus = LANCE_INIT_OK;
		Adapter->LineSpeed = LINESPEED_DEFAULT;                 
//for DMI
		Adapter->DmiIdRev[0] = 'D';
		Adapter->DmiIdRev[1] = 'M';
		Adapter->DmiIdRev[2] = 'I';
		Adapter->DmiIdRev[3] = ADAPTER_STRUCTURE_DMI_VERSION;   
	}
	else
	{
		#ifdef MYDBG
		if (LanceDbg)
			DbgPrint("<==LanceInitialize\nABORT: Failed allocating adapter memory!\n");
		#endif
		return NDIS_STATUS_RESOURCES;
	}

	/* Open the configuration info  */
	NdisOpenConfiguration(
		&Status,
		&ConfigHandle,
		ConfigurationHandle
		);

	if (Status != NDIS_STATUS_SUCCESS)
	{
		#ifdef MYDBG
		if (LanceDbg)
			DbgPrint("<==LanceInitialize\nABORT: Failed opening configuration registry!\n");
		#endif
		return Status;
	}

	/* Get the Slot Number Keyword  */
	NdisReadConfiguration(
		&Status,
		&ReturnedValue,
		ConfigHandle,
		&SlotNumberString,
		NdisParameterHexInteger
		);

	if (Status == NDIS_STATUS_SUCCESS)
	{
		Adapter->LanceSlotNumber = ReturnedValue->ParameterData.IntegerData;
	}
	else
	{
		#ifdef MYDBG
		if (LanceDbg)
			DbgPrint("Slot number not read. NdisReadConfiguration returned %8.8X\n", Status);
		#endif
	}

	/* Get the Slot Number Keyword  */
	NdisReadConfiguration(
		&Status,
		&ReturnedValue,
		ConfigHandle,
		&BusNumberString,
		NdisParameterHexInteger
		);

	if (Status == NDIS_STATUS_SUCCESS)
	{
		Adapter->LanceBusNumber = ReturnedValue->ParameterData.IntegerData;
	}
	else
	{
		#ifdef MYDBG
		if (LanceDbg)
			DbgPrint("Bus number not read. NdisReadConfiguration returned %8.8X\n", Status);
		#endif
	}

	#ifdef MYDBG
		if (LanceDbg)
		DbgPrint("SLOT NUMBER = %x\n",Adapter->LanceSlotNumber);
	#endif

	//
	// Get the interrupt vector number
	//
	NdisReadConfiguration(
		&Status,
		&ReturnedValue,
		ConfigHandle,
		&InterruptVectorString,
		NdisParameterInteger
		);
	
	if (Status == NDIS_STATUS_SUCCESS)
	{
		Adapter->LanceInterruptVector = 
		(CCHAR)ReturnedValue->ParameterData.IntegerData;
	}

	/* Get the DMA channel number   */
	NdisReadConfiguration(
		&Status,
		&ReturnedValue,
		ConfigHandle,
		&DmaChannelString,
		NdisParameterInteger
		);
	
	if (Status == NDIS_STATUS_SUCCESS)
	{
		Adapter->LanceDmaChannel = 
			(CCHAR)ReturnedValue->ParameterData.IntegerData;
	}
	
	/* Get the IO base address      */
	NdisReadConfiguration(
		&Status,
		&ReturnedValue,
		ConfigHandle,
		&BaseAddressString,
		NdisParameterHexInteger
		);
	
	if (Status == NDIS_STATUS_SUCCESS)
	{
/*
	MJ removed duplicated port io address checking for Redundant NIC.
*/
		Adapter->PhysicalIoBaseAddress = ReturnedValue->ParameterData.IntegerData;
	}

	//
	// Get the bus type
	//
	NdisReadConfiguration(
		&Status,
		&ReturnedValue,
		ConfigHandle,
		&BusTypeString,
		NdisParameterInteger
		);

	if (Status == NDIS_STATUS_SUCCESS)
	{
		Adapter->BusType = (USHORT)ReturnedValue->ParameterData.IntegerData;
	}

	#ifdef MYDBG
	if (LanceDbg)
	{
		DbgPrint("Registry information:\n");
		DbgPrint("%s %x\n",msg14, Adapter->PhysicalIoBaseAddress);
		DbgPrint("%s %d\n",msg15, Adapter->LanceInterruptVector);
		DbgPrint("%s %d\n",msg16, Adapter->LanceDmaChannel);
	}
	#endif

	//
	// Get the bus_scan keyword
	//
	NdisReadConfiguration(                  
		&Status,                                        
		&ReturnedValue,                         
		ConfigHandle,                           
		&BusScanString,                         
		NdisParameterString                     
		);

	if (Status == NDIS_STATUS_SUCCESS)
	{
		if(NdisEqualString(&BusComppci,&(ReturnedValue->ParameterData.StringData),0))
		{
			Adapter->BusScan = PCI_DEV;
		}
		else if(NdisEqualString(&BusComppnp,&(ReturnedValue->ParameterData.StringData),0))
		{
			Adapter->BusScan = PLUG_PLAY_DEV;
		}
		else if(NdisEqualString(&BusCompvesa,&(ReturnedValue->ParameterData.StringData),0))
		{
			Adapter->BusScan = LOCAL_DEV;
		}
		else if(NdisEqualString(&BusCompisa,&(ReturnedValue->ParameterData.StringData),0))
		{
			Adapter->BusScan = ISA_DEV;
		}
		else
		{
			Adapter->BusScan = 0;
		}
	}

	#ifdef MYDBG
		if (LanceDbg)
		DbgPrint("%s %x\n",msg2, Adapter->BusScan);
	#endif

	//
	// Get the Full Duplex keyword
	//
	NdisReadConfiguration(
		&Status,
		&ReturnedValue,
		ConfigHandle,
		&FullDuplexString,
		NdisParameterHexInteger
		);

	if (Status == NDIS_STATUS_SUCCESS)
	{
		Adapter->FullDuplex = (SHORT)ReturnedValue->ParameterData.IntegerData;
		switch (Adapter->FullDuplex)
		{
			case 0:
				Adapter->FullDuplex = FDUP_DEFAULT;
				break;

			case 1:
				Adapter->FullDuplex = FDUP_OFF;
				break;

			case 2:
				Adapter->FullDuplex = FDUP_AUI;
				break;

			case 3:
				Adapter->FullDuplex = FDUP_10BASE_T;
				break;

			default:
				Adapter->FullDuplex = FDUP_DEFAULT;
		}
	}

	#ifdef MYDBG
		if (LanceDbg)
		DbgPrint("FDUP = %x\n",Adapter->FullDuplex);
	#endif

#ifdef NDIS40_MINIPORT

/***********************************
 *      Get the External Phy Mode keyword *
 ***********************************/

	NdisReadConfiguration(
		&Status,
		&ReturnedValue,
		ConfigHandle,
		&ExternalPhyModeString,
		NdisParameterInteger
		);

	if (Status == NDIS_STATUS_SUCCESS)
	{
		Adapter->ExtPhyMode = ReturnedValue->ParameterData.IntegerData;
#ifdef MYDBG
			if (LanceDbg)
				DbgPrint("EXTPHY = %x\n",Adapter->ExtPhyMode);
#endif

	}
#endif /* NDIS40_MINIPORT */

	//
	// Get the Bus Timer keyword
	//
	NdisReadConfiguration(
		&Status,
		&ReturnedValue,
		ConfigHandle,
		&BusTimerString ,
		NdisParameterInteger
		);

	if (Status == NDIS_STATUS_SUCCESS)
	{
		Adapter->BusTimer = (SHORT)ReturnedValue->ParameterData.IntegerData * 10;
		if (Adapter->BusTimer < 50 || Adapter->BusTimer > 130)
		{
			Adapter->BusTimer = BUSTIMER_DEFAULT;
		}
	}

	#ifdef MYDBG
	if (LanceDbg)
		DbgPrint("BUS TIMER = %x\n",Adapter->BusTimer);
	#endif

	//
	// Get the LED0 keyword
	//
	NdisReadConfiguration(
		&Status,
		&ReturnedValue,
		ConfigHandle,
		&LED0String,
		NdisParameterHexInteger
		);

	if (Status == NDIS_STATUS_SUCCESS)
	{
		Adapter->led0 = ReturnedValue->ParameterData.IntegerData;
		if (Adapter->led0 == 0x10000)
		{
			Adapter->led0 = LED_DEFAULT;
		}
	}

	#ifdef MYDBG
		if (LanceDbg)
		DbgPrint("%s	%x\n",msg3, Adapter->led0);
	#endif

	//
	// Get the LED1 keyword
	//
	NdisReadConfiguration(
		&Status,
		&ReturnedValue,
		ConfigHandle,
		&LED1String,
		NdisParameterHexInteger
		);

	if (Status == NDIS_STATUS_SUCCESS)
	{
		Adapter->led1 = ReturnedValue->ParameterData.IntegerData;
		if (Adapter->led1 == 0x10000)
		{
			Adapter->led1 = LED_DEFAULT;
		}
	}

	#ifdef MYDBG
		if (LanceDbg)
		DbgPrint("%s	%x\n",msg4, Adapter->led1);
	#endif

	//
	// Get the LED2 keyword
	//
	NdisReadConfiguration(
		&Status,
		&ReturnedValue,
		ConfigHandle,
		&LED2String,
		NdisParameterHexInteger
		);

	if (Status == NDIS_STATUS_SUCCESS)
	{
		Adapter->led2 = ReturnedValue->ParameterData.IntegerData;
		if (Adapter->led2 == 0x10000)
		{
			Adapter->led2 = LED_DEFAULT;
		}
	}

	#ifdef MYDBG
		if (LanceDbg)
		DbgPrint("%s	%x\n",msg5, Adapter->led2);
	#endif

	//
	// Get the LED3 keyword
	//
	NdisReadConfiguration(
		&Status,
		&ReturnedValue,
		ConfigHandle,
		&LED3String,
		NdisParameterHexInteger
		);

	if (Status == NDIS_STATUS_SUCCESS)
	{
		Adapter->led3 = ReturnedValue->ParameterData.IntegerData;
		if (Adapter->led3 == 0x10000)
		{
			Adapter->led3 = LED_DEFAULT;
		}
	}

	#ifdef MYDBG
		if (LanceDbg)
		DbgPrint("%s	%x\n",msg6, Adapter->led3);
	#endif

	//
	// Get the TP parametr
	//
	#ifdef MYDBG
		DbgPrint("TP keyword\n");
	#endif

	NdisReadConfiguration(
		&Status,
		&ReturnedValue,
		ConfigHandle,
		&TPString,
		NdisParameterInteger
		);

	if (Status == NDIS_STATUS_SUCCESS)
	{
		Adapter->tp = ReturnedValue->ParameterData.IntegerData;
	}

	#ifdef MYDBG
		if (LanceDbg)
		DbgPrint("%s	%d\n",msg7, Adapter->tp);
	#endif

	//
	// Get the MPMODE parameter
	//
	#ifdef MYDBG
		DbgPrint("MPMODE keyword\n");
	#endif

	NdisReadConfiguration(
		&Status,
		&ReturnedValue,
		ConfigHandle,
		&MPString,
		NdisParameterInteger
		);

	if (Status == NDIS_STATUS_SUCCESS)
	{
		Adapter->MpMode = ReturnedValue->ParameterData.IntegerData;
	}

	#ifdef MYDBG
		if (LanceDbg)
		DbgPrint("MPMode = %d\n", Adapter->MpMode);
	#endif


	//
	// Read network address
	//
	NdisReadNetworkAddress(
		&Status,
		(PVOID *)&NetworkAddress,
		&NetworkAddressLength,
		ConfigHandle
		);

	//
	// Make sure that the address is the right length asnd
	// at least one of the bytes is non-zero.
	//
	if (Status == NDIS_STATUS_SUCCESS &&
		NetworkAddressLength == ETH_LENGTH_OF_ADDRESS)
	{
		sum = 0;
		for (count = 0; count < ETH_LENGTH_OF_ADDRESS; count++)
		{
			NetAddress[count] = *(NetworkAddress + count);
			sum |= NetAddress[count];
		}

		//
		// If network address is zero, set the address to NULL
		// so we are going to use the burned in address
		//
		NetworkAddress = sum ? NetAddress : NULL;

		#ifdef MYDBG
		if(LanceDbg)
		{
			DbgPrint("%s %.x-%.x-%.x-%.x-%.x-%.x\n", msg8,
				(UCHAR)NetAddress[0],
				(UCHAR)NetAddress[1],
				(UCHAR)NetAddress[2],
				(UCHAR)NetAddress[3],
				(UCHAR)NetAddress[4],
				(UCHAR)NetAddress[5]);
		}
		#endif

	} else {

		//
		// Tells LanceRegisterAdapter to use the burned-in address.
		//
		NetworkAddress = NULL;

	}

//   NdisAllocateSpinLock(&Adapter->SendLock);
//   NdisAllocateSpinLock(&Adapter->Lock);
	//
	// Used passed-in adapter name to register.
	//
	NdisCloseConfiguration(ConfigHandle);

	Status = LanceRegisterAdapter(
				LanceMiniportHandle,
				ConfigurationHandle,
				NetworkAddress,
				Adapter
				);

	//
	// Check status.        If not ok, release allocated memory and 
	// return error status
	//
	if (Status != NDIS_STATUS_SUCCESS)
	{
//              NdisFreeSpinLock(&Adapter->Lock);
//              NdisFreeSpinLock(&Adapter->SendLock);
		LANCE_FREE_MEMORY(Adapter, sizeof(LANCE_ADAPTER));
		#ifdef MYDBG
		if (LanceDbg)
			DbgPrint("<==LanceInitialize\nABORT: LanceRegisterAdapter returned %8.8X\n", Status);
		#endif

		return Status;
	}

	NdisMRegisterAdapterShutdownHandler(
				LanceMiniportHandle,
				(PVOID)Adapter,
				LanceShutdownHandler
				);


	
#ifdef NDIS40_MINIPORT

	NdisMInitializeTimer (
		&(Adapter->CableTimer),
		LanceMiniportHandle,
		(PVOID)LanceCableTimerFunction,
		(PVOID)Adapter
		);

	Adapter->CableDisconnected = FALSE;
	Adapter->LinkInactive = FALSE;

	/* 
		Set the LinkStatus Timer only if this is Primary Adapter.
	*/



#endif
			
#ifdef MYDBG
	if (LanceDbg)
		DbgPrint("<==LanceInitialize\n");
#endif

	return Status;

}


STATIC
NDIS_STATUS
LanceRegisterAdapter(
	IN NDIS_HANDLE LanceMiniportHandle,
	IN NDIS_HANDLE ConfigurationHandle,
	IN PUCHAR NetworkAddress,
	IN PLANCE_ADAPTER Adapter
	)

/*++

Routine Description:

	This routine is responsible for the allocation of the data structures
	for the driver as well as any hardware specific details necessary
	to talk with the device.

Arguments:

	LanceMiniportHandle - The driver handle.

	ConfigurationHandle - Config handle passed to MacAddAdapter.

	NetworkAddress - The network address, or NULL if the default
					should be used.

	Adapter - The pointer for the received information from registry

Return Value:

	Returns a failure status if anything occurred that prevents the
	initialization of the adapter.

--*/

{
	UCHAR i;
	NDIS_ERROR_CODE ErrorCode;
	NDIS_INTERFACE_TYPE InterfaceType;
	NDIS_STATUS Status;
	ULONG HardwareDetailsStatus;
	//
	// Local Pointer to the Initialization Block.
	//
	PLANCE_INIT_BLOCK InitializationBlock;
	PLANCE_INIT_BLOCK_HI InitializationBlockHi;
	PNDIS_RESOURCE_LIST AssignedResources;

	#ifdef MYDBG
		if (LanceDbg)
		DbgPrint("==>LanceRegisterAdapter\n");
		if (LanceBreak)
			_asm int 3;
	#endif

	//
	// We put in this assertion to make sure that ushort are 2 bytes.
	// if they aren't then the initialization block definition needs
	// to be changed.
	//
	// Also all of the logic that deals with status registers assumes
	// that control registers are only 2 bytes.
	//
	ASSERT(sizeof(USHORT) == 2);

	//
	// Get hardware resource information
	//
	HardwareDetailsStatus = LanceHardwareDetails(
												Adapter,
												ConfigurationHandle
												);

	//
	// Check status returned from routine
	//
	if (HardwareDetailsStatus != LANCE_INIT_OK)
	{
		switch (HardwareDetailsStatus)
		{
			case LANCE_INIT_ERROR_9 :
				// IO base address already in use by another device
			case LANCE_INIT_ERROR_13:
				// IRQ and/or DMA already in use by another device
				ErrorCode = NDIS_ERROR_CODE_RESOURCE_CONFLICT;
				break;

			case LANCE_INIT_ERROR_18:
				// PCI scan specified, device not found
			case LANCE_INIT_ERROR_21:
				// Device not found
			case LANCE_INIT_ERROR_22:
				// PnP scan specified, device not found
			case LANCE_INIT_ERROR_23:
				// VESA scan specified, device not found
			case LANCE_INIT_ERROR_24:
				// ISA scan specified, device not found
				ErrorCode = NDIS_ERROR_CODE_ADAPTER_NOT_FOUND;
				break;

			case LANCE_INIT_ERROR_20:
				// Device at specified IO base address not found
				ErrorCode = NDIS_ERROR_CODE_BAD_IO_BASE_ADDRESS;
				break;

			default:
				ErrorCode = NDIS_ERROR_CODE_DRIVER_FAILURE;
		}

		//
		// Log error and exit
		//
		NdisWriteErrorLogEntry(Adapter->LanceMiniportHandle,
							ErrorCode,
							0
							);
		#ifdef MYDBG
		if (LanceDbg)
		{
			DbgPrint("LanceHarewareDetails return error status: ");
			DbgPrint("%x\n", HardwareDetailsStatus);
		}
		#endif

		//
		// Return with fail status
		//
		return NDIS_STATUS_FAILURE;

	}

	//
	// Set bus interface and DMA type
	//
	if (Adapter->BoardFound == PCI_DEV)
	{
		InterfaceType = NdisInterfacePci;
	}
	else if (Adapter->BoardFound == LOCAL_DEV)
	{
		InterfaceType = NdisInterfaceEisa;
	}
	else
	{
		InterfaceType = NdisInterfaceIsa;
	}

	//
	// Register the adapter with NDIS.
	//
	NdisMSetAttributes(
					Adapter->LanceMiniportHandle,
					(NDIS_HANDLE)Adapter,
					TRUE,
					InterfaceType
					);

	//
	// Register the adapter io range
	//
	Status = NdisMRegisterIoPortRange(
						(PVOID *)(&(Adapter->MappedIoBaseAddress)),
						Adapter->LanceMiniportHandle,
						Adapter->PhysicalIoBaseAddress,
						0x20
						);

	//
	// Return error status if failed
	//
	if (Status != NDIS_STATUS_SUCCESS)
	{
		#ifdef MYDBG
		if (LanceDbg)
			DbgPrint("Failed registering I/O address!\n");
		#endif

		//
		// Log error if io registation failed
		//
		NdisWriteErrorLogEntry(
			Adapter->LanceMiniportHandle,
			NDIS_ERROR_CODE_OUT_OF_RESOURCES,
			2,
			registerAdapter,
			LANCE_ERRMSG_HARDWARE_ADDRESS
			);

		goto InitErr0;
	}

	//
	// Allocate memory for all of the adapter structures.
	//
/*
	MJ modified. If secondary adapter, do not allocate descriptor rings.
	Primary adapter will allocate the necessary buffers and secondary 
	adapter will use the same ones.
*/
#ifdef _FAILOVER
if (Adapter->RedundantMode != 2) {
#endif //_FAILOVER
	if (!LanceAllocateAdapterMemory(Adapter))
	{
		#ifdef MYDBG
		if (LanceDbg)
			DbgPrint("Failed allocating memory for descriptor rings!\n");
		#endif

		//
		// Log error if memory allocation failed
		//
		NdisWriteErrorLogEntry(
		Adapter->LanceMiniportHandle,
		NDIS_ERROR_CODE_OUT_OF_RESOURCES,
		2,
		registerAdapter,
		LANCE_ERRMSG_ALLOC_MEMORY
		);

		Status = NDIS_STATUS_RESOURCES;
		goto InitErr1;

	}

	//
	// Initialize the current hardware address.
	//

	NdisMoveMemory(
		Adapter->CurrentNetworkAddress,
		(NetworkAddress != NULL) ? NetworkAddress : Adapter->PermanentNetworkAddress,
		ETH_LENGTH_OF_ADDRESS);

	//
	// Initialize the init-block structure for the adapter
	//
	if ((Adapter->BoardFound == PCI_DEV) || (Adapter->BoardFound == LOCAL_DEV))
	{
		InitializationBlockHi = (PLANCE_INIT_BLOCK_HI)Adapter->InitializationBlock;
		InitializationBlockHi->Mode = LANCE_NORMAL_MODE;

		//
		// Initialize the init-block network addresses
		//
		for (i = 0; i < ETH_LENGTH_OF_ADDRESS; i++)
		InitializationBlockHi->PhysicalNetworkAddress[i]
			= Adapter->CurrentNetworkAddress[i];
		for (i = 0; i < 8; i++)
		InitializationBlockHi->LogicalAddressFilter[i] = 0;

		//
		// Set the init-block receiving descriptor ring pointer low address
		//
		InitializationBlockHi->ReceiveDescriptorRingPhysicalLow
			= LANCE_GET_LOW_PART_ADDRESS(
				NdisGetPhysicalAddressLow(
					Adapter->ReceiveDescriptorRingPhysical));

		//
		// Set number of receiving descriptors in RLEN field
		//
		i = RECEIVE_BUFFERS;
		while (i >>= 1)
		InitializationBlockHi->RLen
			+= BUFFER_LENGTH_EXPONENT_H;

		//
		// Set the init-block receiving descriptor ring pointer high address
		//
		InitializationBlockHi->ReceiveDescriptorRingPhysicalHighL
			|= LANCE_GET_HIGH_PART_ADDRESS(
					NdisGetPhysicalAddressLow(
					Adapter->ReceiveDescriptorRingPhysical));

		InitializationBlockHi->ReceiveDescriptorRingPhysicalHighH
			|= LANCE_GET_HIGH_PART_ADDRESS_H(
					NdisGetPhysicalAddressLow(
					Adapter->ReceiveDescriptorRingPhysical));


		InitializationBlockHi->TransmitDescriptorRingPhysicalLow
			= LANCE_GET_LOW_PART_ADDRESS(
				NdisGetPhysicalAddressLow(Adapter->TransmitDescriptorRingPhysical));

		//
		// Set number of transmit descriptors in TLEN field
		//
		i = TRANSMIT_BUFFERS;
		while (i >>= 1)
		InitializationBlockHi->TLen
			+= BUFFER_LENGTH_EXPONENT_H;

		//
		// Set the init-block transmit descriptor ring pointer high address
		//
		InitializationBlockHi->TransmitDescriptorRingPhysicalHighL
			|= LANCE_GET_HIGH_PART_ADDRESS(
				NdisGetPhysicalAddressLow(
					Adapter->TransmitDescriptorRingPhysical));

		InitializationBlockHi->TransmitDescriptorRingPhysicalHighH
			|= LANCE_GET_HIGH_PART_ADDRESS_H(
				NdisGetPhysicalAddressLow(
					Adapter->TransmitDescriptorRingPhysical));
	}
	else {

		InitializationBlock = (PLANCE_INIT_BLOCK)Adapter->InitializationBlock;
		InitializationBlock->Mode = LANCE_NORMAL_MODE;

		//
		// Initialize the init-block network addresses
		//
		for (i = 0; i < ETH_LENGTH_OF_ADDRESS; i++)
		InitializationBlock->PhysicalNetworkAddress[i]
			= Adapter->CurrentNetworkAddress[i];

		for (i = 0; i < 8; i++)
		InitializationBlock->LogicalAddressFilter[i] = 0;

		//
		// Set the init-block receiving descriptor ring pointer low address
		//
		InitializationBlock->ReceiveDescriptorRingPhysicalLow
			= LANCE_GET_LOW_PART_ADDRESS(
				NdisGetPhysicalAddressLow(
					Adapter->ReceiveDescriptorRingPhysical));

		//
		// Set number of receiving descriptors in RLEN field
		//
		i = RECEIVE_BUFFERS;
		while (i >>= 1)
		InitializationBlock->RLen
			+= BUFFER_LENGTH_EXPONENT;

		//
		// Set the init-block receiving descriptor ring pointer high address
		//
		InitializationBlock->ReceiveDescriptorRingPhysicalHighL
			|= LANCE_GET_HIGH_PART_ADDRESS(
					NdisGetPhysicalAddressLow(
					Adapter->ReceiveDescriptorRingPhysical));

		//
		// Set the init-block transmit descriptor ring pointer low address
		//
		InitializationBlock->TransmitDescriptorRingPhysicalLow
			= LANCE_GET_LOW_PART_ADDRESS(
				NdisGetPhysicalAddressLow(Adapter->TransmitDescriptorRingPhysical));

		//
		// Set number of transmit descriptors in TLEN field
		//
		i = TRANSMIT_BUFFERS;
		while (i >>= 1)
		InitializationBlock->TLen
			+= BUFFER_LENGTH_EXPONENT;

		//
		// Set the init-block transmit descriptor ring pointer high address
		//
		InitializationBlock->TransmitDescriptorRingPhysicalHighL
			|= LANCE_GET_HIGH_PART_ADDRESS(
				NdisGetPhysicalAddressLow(
					Adapter->TransmitDescriptorRingPhysical));
	}

#ifdef NDIS40_MINIPORT
	if (!(LanceInitRxPacketPool (Adapter)))
	{
		#ifdef MYDBG
		if (LanceDbg)
			DbgPrint("Failed allocating packets &/or buffers for receive!\n");
		#endif

		//
		// Log error if buffer/packet allocation failed
		//
		NdisWriteErrorLogEntry(
		Adapter->LanceMiniportHandle,
		NDIS_ERROR_CODE_OUT_OF_RESOURCES,
		2,
		registerAdapter,
		LANCE_ERRMSG_HARDWARE_ALLOC_BUFFER
		);

		Status = NDIS_STATUS_RESOURCES;
		goto InitErr1;
	}
#endif

	NdisZeroMemory (Adapter->GeneralMandatory, GM_ARRAY_SIZE * sizeof(ULONG));
	NdisZeroMemory (Adapter->GeneralOptionalByteCount, GO_COUNT_ARRAY_SIZE * sizeof(LANCE_LARGE_INTEGER));
	NdisZeroMemory (Adapter->GeneralOptionalFrameCount, GO_COUNT_ARRAY_SIZE * sizeof(ULONG));
	NdisZeroMemory (Adapter->GeneralOptional, (GO_ARRAY_SIZE - GO_ARRAY_START) * sizeof(ULONG));
	NdisZeroMemory (Adapter->MediaMandatory, MM_ARRAY_SIZE * sizeof(ULONG));
	NdisZeroMemory (Adapter->MediaOptional, MO_ARRAY_SIZE * sizeof(ULONG));

	/* Initialize the interrupt.    */
	NdisZeroMemory (&Adapter->LanceInterruptObject, sizeof(NDIS_MINIPORT_INTERRUPT));

	/* Save interrupt mode  */
	Adapter->InterruptMode = ((InterfaceType == NdisInterfaceIsa) ?
					NdisInterruptLatched : NdisInterruptLevelSensitive);

	Adapter->LinkActive = TRUE;

	Status = NdisMRegisterInterrupt(
				(PNDIS_MINIPORT_INTERRUPT)&(Adapter->LanceInterruptObject),
					Adapter->LanceMiniportHandle,
					(UINT)Adapter->LanceInterruptVector,
					(UINT)Adapter->LanceInterruptVector,
					FALSE,
					(BOOLEAN)((InterfaceType == NdisInterfaceIsa) ? FALSE : TRUE),
					Adapter->InterruptMode
					);

	if (Status != NDIS_STATUS_SUCCESS)
	{
		#ifdef MYDBG
		if (LanceDbg)
			DbgPrint("Failed registering interrupt!\n");
		#endif

		/* Log error information        */
		NdisWriteErrorLogEntry(
			Adapter->LanceMiniportHandle,
			NDIS_ERROR_CODE_INTERRUPT_CONNECT,
			2,
			registerAdapter,
			LANCE_ERRMSG_INIT_INTERRUPT
			);

		goto InitErr2;
	}

	/* Initialize chip      */
	LanceInit(Adapter, TRUE);

	//
	// Set flag for initialization completion
	//
	Adapter->OpFlags |= INIT_COMPLETED;

	#ifdef MYDBG
		if (LanceDbg)
		DbgPrint("<==LanceRegisterAdapter\n");
	#endif

	return NDIS_STATUS_SUCCESS;

	/* Error handler code   */
InitErr2:

	LanceDeleteAdapterMemory(Adapter);

InitErr1:

	//
	// Deregister IO addresses
	//
	NdisMDeregisterIoPortRange(
		Adapter->LanceMiniportHandle,
		Adapter->PhysicalIoBaseAddress,
		0x20,
		(PVOID)(Adapter->MappedIoBaseAddress)
		);

InitErr0:

	//
	// Release allocated resources
	//
	LanceCleanResources(Adapter);
/*      LANCE_FREE_MEMORY(Adapter, sizeof(LANCE_ADAPTER)); 7-7-97 M. Steiger */
	return Status;

}


STATIC
ULONG
LanceHardwareDetails(
	IN PLANCE_ADAPTER Adapter,
	IN NDIS_HANDLE ConfigurationHandle
	)

/*++

Routine Description:

	This routine scans for PCNET family devices and gets the resources
	for the AMD hardware.

	ZZZ This routine is *not* portable.     It is specific to NT and
	to the Lance implementation.

Arguments:

	Adapter - Where to store the network address, base address, irq and dma.

Return Value:

	LANCE_INIT_OK if successful, LANCE_INIT_ERROR_<NUMBER> if failed,
	LANCE_INIT_WARNING_<NUMBER> if we continue but need to warn user.


--*/

{

	ULONG i;
	ULONG status;
	UCHAR dataByte;
	USHORT Id, Data;
	ULONG DeviceId;

	#ifdef MYDBG
		if (LanceDbg)
		DbgPrint("==>LanceHardwareDetails\n");
	#endif

	//
	// Clear board found flag
	//
	Adapter->BoardFound = 0;

	//
	// Search device based on user's choice and bus type
	// information detected and saved inside the registry
	//
	switch (Adapter->BusScan)
	{
			
			if (Adapter->BoardFound)
			{
				break;
			}

		return LANCE_INIT_ERROR_21;
	}

	//
	// Check if interrupt and DMA are valid
	//
	if (!LanceCheckIrqDmaValid (Adapter))
	{
		#ifdef MYDBG
		if(LanceDbg)
			DbgPrint("%s \n",msg13);
		#endif
		return LANCE_INIT_ERROR_13;
	}

	#ifdef MYDBG
	if (LanceDbg)
	{
		DbgPrint("%s %x\n",msg14, Adapter->PhysicalIoBaseAddress);
		DbgPrint("%s %d\n",msg15, Adapter->LanceInterruptVector);
		DbgPrint("%s %d\n",msg16, Adapter->LanceDmaChannel);
	}
	#endif

	//
	// Save network adapter address
	//
	for (i = 0; i < 6; i++)
	{
		NdisImmediateReadPortUchar(ConfigurationHandle,
								   Adapter->PhysicalIoBaseAddress + i,
								   &dataByte);
		Adapter->PermanentNetworkAddress[i] = dataByte;
	}

	#ifdef MYDBG
	if (LanceDbg)
	{
		DbgPrint("[ %.x-%.x-%.x-%.x-%.x-%.x ] ",
			(UCHAR)Adapter->PermanentNetworkAddress[0],
			(UCHAR)Adapter->PermanentNetworkAddress[1],
			(UCHAR)Adapter->PermanentNetworkAddress[2],
			(UCHAR)Adapter->PermanentNetworkAddress[3],
			(UCHAR)Adapter->PermanentNetworkAddress[4],
			(UCHAR)Adapter->PermanentNetworkAddress[5]);
		DbgPrint("\n");
	}
	#endif

	//
	// Reset the controller and stop the chip
	//
	NdisImmediateReadPortUshort(ConfigurationHandle,
								Adapter->PhysicalIoBaseAddress + LANCE_RESET_PORT, 
								&Data);

	//
	// Delay after reset
	//
	NdisStallExecution(500);

	{
		//
		// if DeviceType is PCNET_PCI3, then check External Phy
		//
		LANCE_READ_BCR_BEFORE_REGISTRATION(Adapter->PhysicalIoBaseAddress, 
													LANCE_BCR32, 
													&Data,
													ConfigurationHandle
													);
		if (Data & MIIPD)
			Adapter->MIIPhyDetected = TRUE; 

	}
	#ifdef MYDBG
		if (LanceDbg)
		DbgPrint("<==LanceHardwareDetails\n");
	#endif

	return Adapter->LanceHardwareConfigStatus;

}

STATIC
BOOLEAN
LanceCheckIoAddress(
	IN ULONG IoAddress
	)
{
	UCHAR   i;
	BOOLEAN ret_code = TRUE;

	#ifdef MYDBG
		if (LanceDbg)
		DbgPrint("==>LanceCheckIoAddress\n");
	#endif

	for (i = 0; i < MAX_ADAPTERS; i++)
	{
		if (AdapterIo[i] == IoAddress)
		{
			ret_code = FALSE;
		}
	}
	#ifdef MYDBG
		if (LanceDbg)
		DbgPrint("<==LanceCheckIoAddress\n");
	#endif
	return ret_code;
}


STATIC
BOOLEAN
LanceCheckIrqDmaValid(
	IN PLANCE_ADAPTER Adapter
	)

{

	UCHAR i;
	BOOLEAN ret_code = TRUE;

	#ifdef MYDBG
		if (LanceDbg)
			DbgPrint("==>LanceCheckIrqDmaValid\n");
	#endif

	//
	// Check conflict interrupt and dma resource.   Local and PCI bus 
	// interrupt can be shared, and no dma channel is used so not checked
	//
	
	//
	// Save resources.      The maximum number of adapters the driver
	// can handle is defined by MAX_ADAPTERS.
	//
		for (i = 0; i < MAX_ADAPTERS; i++)
		{
			if (!AdapterIo[i])
			{

				AdapterIo[i] = Adapter->PhysicalIoBaseAddress;
				AdapterIrq[i] = Adapter->LanceInterruptVector;
				AdapterDma[i] = Adapter->LanceDmaChannel;
				break;
			}
			else if (i == MAX_ADAPTERS-1)
			{
				ret_code = FALSE;
			}
		}
	}
	#ifdef MYDBG
		if (LanceDbg)
			DbgPrint("<==LanceCheckIrqDmaValid\n");
	#endif

	return ret_code;

}

STATIC
VOID
LanceCleanResources(
	IN PLANCE_ADAPTER Adapter
	)

{

	UCHAR i;

	#ifdef MYDBG
		if (LanceDbg)
		DbgPrint("==>LanceCleanResources\n");
	#endif

	//
	// Clean resources.     The maximum number of adapters the driver
	// can handle is set to 3
	//
	for (i = 0; i < MAX_ADAPTERS; i++)
	{
		if (AdapterIo[i] == Adapter->PhysicalIoBaseAddress)
		{
			AdapterIo[i] = 0;
			AdapterIrq[i] = 0;
			AdapterDma[i] = 0;
			break;
		}
	}
	#ifdef MYDBG
	if (LanceDbg)
		DbgPrint("<==LanceCleanResources\n");
	#endif
}
STATIC
VOID
LancePciEnableDma(
	IN PLANCE_ADAPTER Adapter
	)

/*++

Routine Description:

	This routine eables PCI device DMA by setting PCI command
	register DMA bit

Arguments:

	Adapter - The adapter for the hardware.

Return Value:

	None.

--*/

{

	#ifdef MYDBG
		if (LanceDbg)
		DbgPrint("==>LancePciEnableDma\n");
	#endif

	LanceSetPciDma(Adapter, TRUE);

	#ifdef MYDBG
		if (LanceDbg)
		DbgPrint("<==LancePciEnableDma\n");
	#endif
}


STATIC
VOID
LancePciDisableDma(
	IN PLANCE_ADAPTER Adapter
	)

/*++

Routine Description:

	This routine disables PCI device DMA by clearing PCI command
	register DMA bit

Arguments:

	Adapter - The adapter for the hardware.

Return Value:

	None.

--*/

{

	#ifdef MYDBG
		if (LanceDbg)
		DbgPrint("==>LancePciDisableDma\n");
	#endif

	LanceSetPciDma(Adapter, FALSE);

	#ifdef MYDBG
		if (LanceDbg)
		DbgPrint("<==LancePciDisableDma\n");
	#endif
}


STATIC
VOID
LanceSetPciDma(
	IN PLANCE_ADAPTER Adapter,
	BOOLEAN EnablePciDma
	)

/*++

Routine Description:

	This routine set/clean pci command register DMA bit

Arguments:

	Adapter - The adapter for the hardware.

	EnablePciDma - Flag for operation.

Return Value:

	None.

--*/

{

	ULONG Buffer;
	ULONG PatchData = 0;
	#ifdef MYDBG
		if (LanceDbg)
		DbgPrint("==>LanceSetPciDma\n");
	#endif
VOID
LanceInit(
	IN PLANCE_ADAPTER Adapter,
	IN BOOLEAN FullReset
	)

/*++

Routine Description:

	This routine sets up the initial init of the driver.

Arguments:

	Adapter - The adapter data structure pointer

	FullReset - Full reset flag for programming all the registers

Return Value:

	None.

--*/

{

	#ifdef MYDBG
		if (LanceDbg)
		DbgPrint("==>LanceInit\n");
	#endif

	/* Check if reset is allowed    */
	if (Adapter->OpFlags & (RESET_IN_PROGRESS | RESET_PROHIBITED))
		return;

	/* Disable interrupts and release any pending interrupt sources */
//      LanceAcquireSpinLock(&Adapter->Lock);
	LanceDisableInterrupt(Adapter);
//      LanceReleaseSpinLock(&Adapter->Lock);

	LANCE_WRITE_CSR(Adapter->MappedIoBaseAddress, LANCE_CSR0, 0xff04);

	/* Set reset flag for checking in isr. The isr will     */
	/* not read CSR0 register for the chip interrupt when   */
	/* programming other registers. Note: PCI device can    */
	/* share an interrupt so our isr can be called all the  */
	/* time; and programming CSR has two steps and the value        */
	/* set inside address register should not be changed    */
	/* by the isr   */
	Adapter->OpFlags |= RESET_IN_PROGRESS;

	if(Adapter->OpFlags & IN_LINK_TIMER)
	{
		NdisStallExecution(1);
	}

	/* First we make sure that the device is stopped and no */
	/* more interrupts come out.    Also some registers must be     */
	/* programmed with CSR0 STOP bit set    */
	LanceStopChip(Adapter);

	/* Initialize registers and data structure      */
	LanceSetupRegistersAndInit(Adapter, FullReset);

	/* Start the chip       */
	LanceStartChip(Adapter);
	#ifdef MYDBG
		if (LanceDbg)
		DbgPrint("<==LanceInit\n");
	#endif
}


STATIC
VOID
LanceStopChip(
	IN PLANCE_ADAPTER Adapter
	)

/*++

Routine Description:

	This routine is used to stop the controller and disable
	controller's interrupt

Arguments:

	Adapter - The adapter data structure pointer

Return Value:

	None.

--*/

{

	USHORT Time, Data;
	UINT Timeout = START_STOP_TIMEOUT;

	#ifdef MYDBG
		if (LanceDbg)
		DbgPrint("==>LanceStopChip\n");
	#endif

	//
	// For PCI device, disable PCI DMA engine before stopping
	// the chip.
	//
//      if (Adapter->BoardFound == PCI_DEV) {
	if (Adapter->DeviceType == PCNET_PCI1) {
		while (Timeout--) {

		//
		// Stop PCI DMA engine
		//
		LancePciDisableDma(Adapter);

		//
		// Set STOP bit to stop the chip
		//
		LANCE_WRITE_CSR(Adapter->MappedIoBaseAddress,LANCE_CSR0,LANCE_CSR0_STOP | 0xff00);

		//
		// Mask out IDONM interrupts
		//
		LANCE_WRITE_CSR(Adapter->MappedIoBaseAddress, LANCE_CSR3, LANCE_CSR3_IDONM);

		//
		// Set initialization block physical address registers
		//
		LANCE_WRITE_CSR(Adapter->MappedIoBaseAddress, LANCE_CSR2, LANCE_GET_HIGH_PART_PCI_ADDRESS(
									NdisGetPhysicalAddressLow(
										Adapter->InitializationBlockPhysical)));
		LANCE_WRITE_CSR(Adapter->MappedIoBaseAddress, LANCE_CSR1, LANCE_GET_LOW_PART_ADDRESS(
									NdisGetPhysicalAddressLow(
										Adapter->InitializationBlockPhysical)));

		//
		// Initialize the chip and load initialization block
		//
		LANCE_WRITE_CSR(Adapter->MappedIoBaseAddress, LANCE_CSR0, LANCE_CSR0_INIT);

		//
		// Enable PCI DMA engine
		//
		LancePciEnableDma(Adapter);

		//
		// Check if chip stopped successfully.  If initialization done
		// bit set, it is sure chip stopped.
		//
		for (Time = 0; Time < 1000; Time++) {

			//
			// Check IDON bit
			//
			LANCE_READ_CSR(Adapter->MappedIoBaseAddress, LANCE_CSR0, &Data);
			if (Data & LANCE_CSR0_IDON) {

			//
			// If IDON bit set, clear status bits
			//
			LANCE_WRITE_CSR(Adapter->MappedIoBaseAddress, LANCE_CSR0, 0xff00 | LANCE_CSR0_STOP);
			return;

			}

			//
			// Give more time before checking IDON bit again
			//
			NdisStallExecution(1);

			}
		}

	} else {

		//
		// Stop the chip
		//
		LANCE_WRITE_CSR(Adapter->MappedIoBaseAddress, LANCE_CSR0, LANCE_CSR0_STOP);

		//
		// Ensure that the chip stops completely with interrupts disabled.
		//
		for (Time = 0; Time < 5; Time++)
		NdisStallExecution(1);
	}

	#ifdef MYDBG
		if (LanceDbg)
		DbgPrint("<==LanceStopChip\n");
	#endif
}


STATIC
VOID
LanceSetupRegistersAndInit(
	IN PLANCE_ADAPTER Adapter,
	IN BOOLEAN FullReset
	)

/*++

Routine Description:

	It is this routines responsibility to intialize transmit and
	receive descriptor rings but not start the chip.

Arguments:

	Adapter:        The adapter whose hardware is to be initialized.
	FullReset:      Full reset flag for programming all the registers

Return Value:

	None.

NOTES:

	LanceInitRxPacketPool *MUST* be called before this routine to set up 
	the NDIS packet array in the adapter structure. 

--*/
{

	UCHAR   i;
	USHORT  Data;

	PLANCE_TRANSMIT_DESCRIPTOR              TransmitDescriptorRing;
	PLANCE_TRANSMIT_DESCRIPTOR_HI   TransmitDescriptorRingHi;
	NDIS_PHYSICAL_ADDRESS                   TransmitBufferPointerPhysical;
	PLANCE_RECEIVE_DESCRIPTOR               ReceiveDescriptorRing;
	PLANCE_RECEIVE_DESCRIPTOR_HI    ReceiveDescriptorRingHi;
	NDIS_PHYSICAL_ADDRESS                   ReceiveBufferPointerPhysical;

	#ifdef MYDBG
	if (LanceDbg)
		DbgPrint("==>LanceSetupRegistersAndInit\n");
	#endif
	/* Initialize the Rx/Tx descriptor ring structures      */
	TransmitBufferPointerPhysical = Adapter->TransmitBufferPointerPhysical;
	ReceiveBufferPointerPhysical = Adapter->ReceiveBufferPointerPhysical;

	/* Set the Software Style to 32 Bits (PCNET-PCI).       */

	if((Adapter->BoardFound == PCI_DEV) || (Adapter->BoardFound == LOCAL_DEV))
	{
		TransmitDescriptorRingHi =
		(PLANCE_TRANSMIT_DESCRIPTOR_HI)Adapter->TransmitDescriptorRing;

		/* Set the software style to 32 Bits    */

		LANCE_READ_CSR(Adapter->MappedIoBaseAddress, LANCE_CSR58, &Data);

		/* Mask IOStyle bits.   */

		Data &= 0xff00 ;

		/* Enable SSIZE32       */
		/* CSRPCNET + PCNET PCI II Sw style, will be set by the controller.     */
		Data |= SW_STYLE_2 ;
		Adapter->SwStyle = SW_STYLE_2;

		LANCE_WRITE_CSR(Adapter->MappedIoBaseAddress, LANCE_CSR58, Data);

		/* Initialize transmit descriptors      */
		for (i = 0; i < TRANSMIT_BUFFERS; i++,TransmitDescriptorRingHi++)
		{
			//
			// Initialize transmit buffer pointer
			//
			TransmitDescriptorRingHi->LanceBufferPhysicalLow =
			LANCE_GET_LOW_PART_ADDRESS(NdisGetPhysicalAddressLow(
			TransmitBufferPointerPhysical) + (i * TRANSMIT_BUFFER_SIZE));

			TransmitDescriptorRingHi->LanceBufferPhysicalHighL =
			LANCE_GET_HIGH_PART_ADDRESS(NdisGetPhysicalAddressLow(
			TransmitBufferPointerPhysical) + (i * TRANSMIT_BUFFER_SIZE));

			TransmitDescriptorRingHi->LanceBufferPhysicalHighH =
			LANCE_GET_HIGH_PART_ADDRESS_H(NdisGetPhysicalAddressLow(
			TransmitBufferPointerPhysical) + (i * TRANSMIT_BUFFER_SIZE));

			TransmitDescriptorRingHi->ByteCount = (SHORT)0xF000;
			TransmitDescriptorRingHi->TransmitError = 0;

			// Set STP and ENP bits.
			TransmitDescriptorRingHi->LanceTMDFlags = (STP | ENP);
		} /* END "for" LOOP */

		ReceiveDescriptorRingHi =
		(PLANCE_RECEIVE_DESCRIPTOR_HI)Adapter->ReceiveDescriptorRing;

		/* Initialize receiving descriptors     */
		for (i = 0; i < RECEIVE_BUFFERS; i++, ReceiveDescriptorRingHi++)
		{
			ReceiveDescriptorRingHi->LanceBufferPhysicalLow =
			LANCE_GET_LOW_PART_ADDRESS(NdisGetPhysicalAddressLow(
			ReceiveBufferPointerPhysical) + (i * RECEIVE_BUFFER_SIZE));

			ReceiveDescriptorRingHi->LanceBufferPhysicalHighL =
			LANCE_GET_HIGH_PART_ADDRESS(NdisGetPhysicalAddressLow(
			ReceiveBufferPointerPhysical) + (i * RECEIVE_BUFFER_SIZE));

			ReceiveDescriptorRingHi->LanceBufferPhysicalHighH =
			LANCE_GET_HIGH_PART_ADDRESS_H(NdisGetPhysicalAddressLow(
			ReceiveBufferPointerPhysical) + (i * RECEIVE_BUFFER_SIZE));

			/* Make Lance the owner of the descriptor       */
			ReceiveDescriptorRingHi->LanceRMDFlags = OWN;
			ReceiveDescriptorRingHi->BufferSize = -RECEIVE_BUFFER_SIZE;
			ReceiveDescriptorRingHi->ByteCount = 0;
			ReceiveDescriptorRingHi->LanceRMDReserved1 = 0;
#ifdef NDIS40_MINIPORT
			/* Write the address of this descriptor's LanceRMDFlags member */
			/* to the MiniportReserved field of the relative NDIS packet */
			*((UCHAR **)&(Adapter->pNdisPacket[i]->MiniportReserved[0])) = &(ReceiveDescriptorRingHi->LanceRMDFlags);
#endif
		} /* END "for" LOOP */

	} /* END "if" Section */
	

	//
	// Reset Power Management STOP flag
	//

	Adapter->OpFlags &= RESET_MASK;

#ifdef NDIS40_MINIPORT
	Adapter->TxBufsUsed=0;  /* Reset outstanding tx buffer count */
#endif

	//
	// Initialize NextTransmitDescriptorIndex.
	//

	Adapter->NextTransmitDescriptorIndex = 0;

	//
	// Initialize TailTransmitDescriptorIndex.
	//

	Adapter->TailTransmitDescriptorIndex = 0;

	//
	// Initialize NextReceiveDescriptorIndex.
	//

	Adapter->NextReceiveDescriptorIndex = 0;

	//
	// Initialize Csr0Value to 0
	//

	Adapter->Csr0Value = 0;

	/* Global setting for csr4 register     */
	LANCE_READ_CSR(Adapter->MappedIoBaseAddress, LANCE_CSR4, &Data);

	Data |= (LANCE_CSR4_AUTOPADTRANSMIT | LANCE_CSR4_DPOLL | 0x0004);
	
	LANCE_WRITE_CSR(Adapter->MappedIoBaseAddress, LANCE_CSR4, Data);



	switch (Adapter->DeviceType)
	{
		case LANCE:
	
		case PCNET_PCI1:
			break;

		default:
			/* write dma burst and bus control register bcr18       */
			LANCE_READ_BCR(Adapter->MappedIoBaseAddress, LANCE_BCR18, &Data);
			Data |= (LANCE_BCR18_BREADE | LANCE_BCR18_BWRITE );
			LANCE_WRITE_BCR(Adapter->MappedIoBaseAddress, LANCE_BCR18, Data);
			LANCE_READ_CSR(Adapter->MappedIoBaseAddress, LANCE_CSR4, &Data);
			Data |= LANCE_CSR4_DMAPLUS;
			LANCE_WRITE_CSR(Adapter->MappedIoBaseAddress, LANCE_CSR4, Data);
			break;
	}

	switch (Adapter->DeviceType)
	{
		case LANCE:
		case PCNET_PCI2_A4:
		case PCNET_PCI2_B2:
		case PCNET_PCI1:
			break;

		default:
			/* Transmit Start Point setting(csr80)  */
			LANCE_READ_CSR(Adapter->MappedIoBaseAddress, 80, &Data);
			Data |= 0x0800;
			LANCE_WRITE_CSR(Adapter->MappedIoBaseAddress, 80, Data);
			break;
	}

	LANCE_READ_CSR(Adapter->MappedIoBaseAddress, LANCE_CSR3, &Data);
	switch (Adapter->DeviceType)
	{
		case LANCE:
		case PCNET_ISA:
		case PCNET_ISA_PLUS:
		case PCNET_PCI1:
		case PCNET_PCI2_A4:
			Data |= (LANCE_CSR3_MERRM | LANCE_CSR3_BABLM);
			break;

		default:
			Data |= (LANCE_CSR3_IDONM | LANCE_CSR3_MERRM | LANCE_CSR3_DXSUFLO | LANCE_CSR3_BABLM);
			break;
	}



	LANCE_WRITE_CSR(Adapter->MappedIoBaseAddress, LANCE_CSR3, Data);

	/* Initialize CSR registers */

	if (Adapter->BoardFound != PCI_DEV && Adapter->BoardFound != LOCAL_DEV)
	{
		/* Enable and set bus activity timer    */

		LANCE_READ_CSR(Adapter->MappedIoBaseAddress, LANCE_CSR4, &Data);
		LANCE_WRITE_CSR(Adapter->MappedIoBaseAddress, LANCE_CSR4, Data | LANCE_CSR4_TIMER);

		LANCE_WRITE_CSR(Adapter->MappedIoBaseAddress, LANCE_CSR82, Adapter->BusTimer);

		#ifdef MYDBG
		LANCE_READ_CSR(Adapter->MappedIoBaseAddress, LANCE_CSR4, &Data);
		DbgPrint("LanceSetupRegistersAndInit: CSR4 = %x\n",Data);
		#endif
	} /* END "if" SECTION */

	if (FullReset)
	{
		//
		// Program LEDs
		//

		InitLEDs(Adapter);

		//
		// Initialize Full Duplex and "TP" modes
		//

		InitFullDuplexMode(Adapter);



		//
		// Program CSR1 and CSR2 with initialization block physical address
		//

		if (Adapter->BoardFound == PCI_DEV || Adapter->BoardFound == LOCAL_DEV)
		{
			LANCE_WRITE_CSR(Adapter->MappedIoBaseAddress, LANCE_CSR2,
			LANCE_GET_HIGH_PART_PCI_ADDRESS(NdisGetPhysicalAddressLow(
			Adapter->InitializationBlockPhysical)));
		}
		else
		{
			LANCE_WRITE_CSR(Adapter->MappedIoBaseAddress, LANCE_CSR2,
			LANCE_GET_HIGH_PART_ADDRESS(NdisGetPhysicalAddressLow(
			Adapter->InitializationBlockPhysical)));
		}

		LANCE_WRITE_CSR(Adapter->MappedIoBaseAddress, LANCE_CSR1,
		LANCE_GET_LOW_PART_ADDRESS(NdisGetPhysicalAddressLow(
		Adapter->InitializationBlockPhysical)));
	}

#ifdef MYDBG
	if (LanceDbg)
		DbgPrint("<==LanceSetupRegistersAndInit\n");
#endif
}


STATIC
VOID
InitFullDuplexMode (
	IN PLANCE_ADAPTER Adapter
	)
{

	USHORT  Data;

	//
	// Local Pointer to the Initialization Block.
	//

	PLANCE_INIT_BLOCK                               InitializationBlock;
	PLANCE_INIT_BLOCK_HI                    InitializationBlockHi;

#ifdef MYDBG
	if (LanceDbg)
		DbgPrint("==>InitFullDuplexMode\n");
#endif
	if((Adapter->BoardFound == PCI_DEV) || (Adapter->BoardFound == LOCAL_DEV))
	{
		InitializationBlockHi = (PLANCE_INIT_BLOCK_HI)Adapter->InitializationBlock;
	}
	else
	{
		InitializationBlock = (PLANCE_INIT_BLOCK)Adapter->InitializationBlock;
	}


	//
	// Program Full Duplex Mode
	//

	if (Adapter->FullDuplex != FDUP_DEFAULT)
	{
		LANCE_WRITE_BCR(Adapter->MappedIoBaseAddress, 9, Adapter->FullDuplex);
	}

	
	/* Disable "TP" mode on Laguna (P3) and newer boards */

	switch (Adapter->DeviceType)
	{       
		case PCNET_PCI2_B2:
		case PCNET_PCI2_A4:
		case PCNET_PCI1:
			break;
		default:
			Adapter->tp = 0;
	}

	if (Adapter->tp)
	{
		/* Disable link status test */
		InitializationBlockHi->Mode |= 0x1080;
		LANCE_READ_BCR(Adapter->MappedIoBaseAddress, 2, &Data);
		LANCE_WRITE_BCR(Adapter->MappedIoBaseAddress, 2, (Data & 0xfffc));
	}

#ifdef MYDBG
	if (LanceDbg)
		DbgPrint("<==InitFullDuplexMode\n");
#endif
} /* End of function InitFullDuplexMode () */
STATIC
VOID
InitLEDs (
	IN PLANCE_ADAPTER Adapter
	)
{
	ULONG * pLEDs[4];
	USHORT  n;
	USHORT  Data;

#ifdef MYDBG
	if (LanceDbg)
		DbgPrint("==>InitLEDs\n");
#endif
	pLEDs[0] = &(Adapter->led0);
	pLEDs[1] = &(Adapter->led1);
	pLEDs[2] = &(Adapter->led2);
	pLEDs[3] = &(Adapter->led3);

	if (Adapter->DeviceType == PCNET_PCI3)
	{
		LANCE_READ_BCR(Adapter->MappedIoBaseAddress, LANCE_BCR2, &Data);
		LANCE_WRITE_BCR(Adapter->MappedIoBaseAddress, LANCE_BCR2, Data|LANCE_BCR2_LEDPE);
	}

	if (((Adapter->BoardFound == ISA_DEV   &&
		(Adapter->DeviceType != PCNET_ISA_PLUS  &&
		Adapter->DeviceType != PCNET_ISA_PLUS_PLUS)) ||
		Adapter->BoardFound == LOCAL_DEV        ||
		(Adapter->BoardFound == PCI_DEV         &&
		Adapter->DeviceType == PCNET_PCI1)))
	{
		for (n=0; n<4; n++)
		{
			if (*pLEDs[n] == LED_DEFAULT)
			{
				switch (n) {
					case 0 : 
								if (Adapter->BoardFound == ISA_DEV)
									break;
								else
									*pLEDs[n] = LED_defaults[n];
								break;
					case 1 :
								if (Adapter->BoardFound == ISA_DEV)
									*pLEDs[n] = LED_defaults[n];
								else
									*pLEDs[n] = LED_defaults[n] | 0x0020;
								break;
					case 2 : 
								if (Adapter->BoardFound == PCI_DEV)
									break;
								else
									*pLEDs[n] = LED_defaults[n];
								break;
					default : 
								*pLEDs[n] = LED_defaults[n];
								break;
				}
			}
		}
	}

	for (n=0; n<4; n++)
	{
		if (*pLEDs[n] != LED_DEFAULT)
		{
			LANCE_WRITE_BCR(Adapter->MappedIoBaseAddress,
			LANCE_LED0_STAT + n ,
			*pLEDs[n]);
		}
	}

#ifdef MYDBG
	if (LanceDbg)
		DbgPrint("<==InitLEDs\n");
#endif

} /* End of function InitLEDs () */

STATIC
VOID
LanceStartChip(
	IN PLANCE_ADAPTER Adapter
	)

/*++

Routine Description:

	This routine loads initialization block and starts the chip.

Arguments:

	Adapter - The adapter data structure pointer

Return Value:

	None.

--*/

{

	UINT Data;
	UINT Timeout = START_STOP_TIMEOUT;
	#ifdef MYDBG
		if (LanceDbg)
		DbgPrint("==>LanceStartChip\n");
	#endif

	//
	// Mask IDON
	//
	LANCE_READ_CSR(Adapter->MappedIoBaseAddress, LANCE_CSR3, &Data);
	Data |= LANCE_CSR3_IDONM;
	LANCE_WRITE_CSR(Adapter->MappedIoBaseAddress, LANCE_CSR3, Data);

	/* Clear the IDON bit and other interrupt bits in CSR0 with     */
	/* chip interrupt disabled      */
	LANCE_WRITE_CSR(Adapter->MappedIoBaseAddress, LANCE_CSR0, 0xFF00);

	/* Load initialization block into controller    */
	LANCE_WRITE_CSR(Adapter->MappedIoBaseAddress, LANCE_CSR0, LANCE_CSR0_INIT);

	/* Waiting until IDON bit set   */
	while (Timeout--) {

		/* Read CSR0    */
		LANCE_READ_CSR(Adapter->MappedIoBaseAddress, LANCE_CSR0, &Data);
		if (LanceDbg) {
			DbgPrint("IDON bit = %x\n",Data);
			DbgPrint("Timeout = %x\n",Timeout);
		}
		/* Check if IDON bit set        */
		if (Data & LANCE_CSR0_IDON) {

		/* Now clear reset flag to allow the isr reading        */
		/* CSR0 for the interrupt       */
		Adapter->OpFlags &= ~RESET_IN_PROGRESS;

		/* Clear all interrupt status bits and start chip       */
		if (Adapter->OpFlags & IN_INTERRUPT_DPC) {

			LANCE_WRITE_CSR(Adapter->MappedIoBaseAddress, 
					LANCE_CSR0,
					LANCE_CSR0_START | LANCE_CSR0_INIT);

		} else {

			LANCE_WRITE_CSR(Adapter->MappedIoBaseAddress, 
					LANCE_CSR0,
					LANCE_CSR0_START | LANCE_CSR0_INIT | LANCE_CSR0_IENA);

		}
		
		//
		// Exit the loop
		//
		break;

		}

		//
		// Give some delay
		//
		NdisStallExecution(1);
	}

	#ifdef MYDBG
		if (LanceDbg)
		DbgPrint("<==LanceStartChip\n");
	#endif
}


NDIS_STATUS
LanceReset(
	OUT PBOOLEAN AddressingReset,
	IN NDIS_HANDLE MiniportAdapterContext
	)

/*++

Routine Description:

	The LanceReset request instructs the miniport to issue a hardware reset
	to the network adapter. The miniport also resets its software state.    See
	the description of MiniportReset for a detailed description of this request.

Arguments:

	AddressingReset - Set to TRUE if LanceSetInformation has to be called

	MiniportAdapterContext - The context value set by this mini-port.

Return Value:

	The function value is the status of the operation.

--*/

{

	USHORT Data;

	//
	// Holds the status that should be returned to the caller.
	//
	PLANCE_ADAPTER Adapter = (PLANCE_ADAPTER)MiniportAdapterContext;

	#ifdef MYDBG
		if (LanceDbg)
		DbgPrint("==>LanceReset\n");
	#endif



	ASSERT(!(Adapter->OpFlags & RESET_IN_PROGRESS));

	if (Adapter->OpFlags & INIT_COMPLETED) {

		//
		// Stop chip
		//
		LanceStopChip(Adapter);

		//
		// Do software reset
		//
		// NdisRawReadPortUshort(Adapter->MappedIoBaseAddress + LANCE_RESET_PORT, &Data);

		//
		// Clear operation flags for reset
		//
		Adapter->OpFlags &= ~(RESET_IN_PROGRESS | RESET_PROHIBITED);

		//
		// Restart the chip
		//
		LanceInit(Adapter, TRUE);

	}

// for DMI
	++Adapter->DmiSpecific[DMI_RESET_CNT];

	#ifdef MYDBG
		if (LanceDbg)
		DbgPrint("<==LanceReset\n");
	#endif

	NdisMSendResourcesAvailable(Adapter->LanceMiniportHandle);
	return NDIS_STATUS_SUCCESS;

}

STATIC
VOID
LanceHalt(
	IN NDIS_HANDLE Context
	)

/*++

Routine Description:

	This routine stops the controller and release all the allocated
	resources.      Interrupts are enabled and no new request is sent to
	the driver.     It is not necessary to complete all outstanding
	requests before stopping the controller.

Arguments:

	Context - A pointer to the adapter data structure.

Return Value:

	None.

--*/

{

	PLANCE_ADAPTER Adapter = (PLANCE_ADAPTER)Context;
	USHORT Data;
	BOOLEAN IsCancelled;

	#ifdef MYDBG
		if (LanceDbg)
		DbgPrint("==>LanceHalt\n");
	#endif
 
	NdisMCancelTimer (&(Adapter->CableTimer),&IsCancelled);
	//
	// Stop the controller and disable the controller interrupt
	//
	LanceStopChip (Adapter);







	// Clean resources anyway
	LanceCleanResources(Adapter);

	//
	// Deregister interrupt number
	//
	NdisMDeregisterInterrupt (&(Adapter->LanceInterruptObject));

	//
	// Deregister io address range
	//
	NdisMDeregisterIoPortRange(
			Adapter->LanceMiniportHandle,
			Adapter->PhysicalIoBaseAddress,
			0x20,
			(PVOID)(Adapter->MappedIoBaseAddress)
			);

//      NdisFreeSpinLock(&Adapter->Lock);




#ifdef NDIS40_MINIPORT
	LanceFreeRxPacketPool(Adapter);
#endif          

	//
	// Clean resources data anyway
	//
	LanceCleanResources(Adapter);

	//
	// Release allocated memory for initiation block, 
	// descriptors and buffers
	//
	LanceDeleteAdapterMemory (Adapter);

	//
	// Release allocated memory for driver data storage
	//
	LANCE_FREE_MEMORY(Adapter, sizeof(LANCE_ADAPTER));


	#ifdef MYDBG
		if (LanceDbg)
		DbgPrint("<==LanceHalt\n");
	#endif
}


STATIC
NDIS_STATUS
LanceTransferData(
	OUT PNDIS_PACKET Packet,
	OUT PUINT BytesTransferred,
	IN NDIS_HANDLE MiniportAdapterContext,
	IN NDIS_HANDLE MiniportReceiveContext,
	IN UINT ByteOffset,
	IN UINT BytesToTransfer
	)

/*++

Routine Description:

	A protocol calls the LanceTransferData request (indirectly via
	NdisTransferData) from within its Receive event handler
	to instruct the MAC to copy the contents of the received packet
	a specified paqcket buffer.

Arguments:

	Status - Status of the operation.

	MiniportAdapterContext - The context value set by the Miniport.

	MiniportReceiveContext - The context value passed by the MAC on its call
	to NdisMEthIndicateReceive.

	ByteOffset - An unsigned integer specifying the offset within the
	received packet at which the copy is to begin.  If the entire packet
	is to be copied, ByteOffset must be zero.

	BytesToTransfer - An unsigned integer specifying the number of bytes
	to copy.        It is legal to transfer zero bytes; this has no effect. If
	the sum of ByteOffset and BytesToTransfer is greater than the size
	of the received packet, then the remainder of the packet (starting from
	ByteOffset) is transferred, and the trailing portion of the receive
	buffer is not modified.

	Packet - A pointer to a descriptor for the packet storage into which
	the MAC is to copy the received packet.

	BytesTransfered - A pointer to an unsigned integer.     The MAC writes
	the actual number of bytes transferred into this location.      This value
	is not valid if the return status is STATUS_PENDING.

Return Value:

	The function value is the status of the operation.

--*/

{

	PLANCE_ADAPTER Adapter = (PLANCE_ADAPTER)MiniportAdapterContext;

	#ifdef MYDBG
		if (LanceDbg)
		DbgPrint("==>LanceTransferData routine : %i\n",Adapter->RedundantMode);
	#endif

	//
	// If reset flag set, return with reset status
	//
	if (Adapter->OpFlags & RESET_IN_PROGRESS) {

		#ifdef MYDBG
		if (LanceDbg)
			DbgPrint("LanceTransferData routine: found reset and return.\n");
		#endif

		return NDIS_STATUS_RESET_IN_PROGRESS;

	}

	//
	// Copy packet data into data buffers provided by system
	//
	LanceMovePacket(Adapter,
				Packet,
				BytesToTransfer,
				(PCHAR)MiniportReceiveContext + ByteOffset,
				BytesTransferred,
				FALSE
				);
		
	#ifdef MYDBG
		if (LanceDbg)
		DbgPrint("<==LanceTransferData routine\n");
	#endif

	return NDIS_STATUS_SUCCESS;     

}


VOID
LanceMovePacket(
	IN PLANCE_ADAPTER Adapter,
	PNDIS_PACKET Packet,
	IN UINT BytesToCopy,
	PCHAR Buffer,
	OUT PUINT BytesCopied,
	IN BOOLEAN CopyFromPacketToBuffer
	)

/*++

Routine Description:

	Copy from an ndis packet into a buffer while CopyFromPacketToBuffer
	flag set.       Otherwise copy from a buffer into an ndis packet buffer

Arguments:

	Adapter - The adapter data structure pointer.

	Packet - The packet pointer.

	BytesToCopy - The number of bytes to copy.

	Buffer - The buffer pointer.

	BytesCopied - The number of bytes actually copied.      Can be less then
	BytesToCopy if the packet is shorter than BytesToCopy.

	CopyFromPacketToBuffer - Copy direction

	AccessSram - Flag for SRAM / DRAM access

Return Value:

	None

--*/

{

	UINT NdisBufferCount;
	PNDIS_BUFFER CurrentBuffer;
	PVOID VirtualAddress;
	UINT CurrentLength;
	UINT LocalBytesCopied = 0;
	UINT AmountToMove;

	#ifdef MYDBG
		if (LanceDbg)
		DbgPrint("==>LanceMovePacket\n");
	#endif

	//
	// Take care of boundary condition of zero length copy.
	//
	*BytesCopied = 0;
	if (!BytesToCopy) {

		#ifdef MYDBG
		if (LanceDbg)
			DbgPrint("LanceMovePacket routine: number of bytes to move is 0.\n");
		#endif

		return;

	}

	//
	// Get the first buffer.
	//
	NdisQueryPacket(
		Packet,
		NULL,
		&NdisBufferCount,
		&CurrentBuffer,
		NULL
		);

	//
	// Could have a null packet.
	//
	if (!NdisBufferCount) {

		#ifdef MYDBG
		if (LanceDbg)
			DbgPrint("LanceMovePacket routine: buffer count is 0.\n");
		#endif

		return;

	}

	NdisQueryBuffer(
		CurrentBuffer,
		&VirtualAddress,
		&CurrentLength
		);

	while (LocalBytesCopied < BytesToCopy) {

		if (!CurrentLength) {
		
		NdisGetNextBuffer(
			CurrentBuffer,
			&CurrentBuffer
			);

		//
		// We've reached the end of the packet. We return
		// with what we've done so far. (Which must be shorter
		// than requested.
		//
		if (!CurrentBuffer) 
			break;

		NdisQueryBuffer(
			CurrentBuffer,
			&VirtualAddress,
			&CurrentLength
			);

		continue;

		}

		//
		// Copy the data.
		//
		AmountToMove = (CurrentLength <= (BytesToCopy - LocalBytesCopied)) ?
						CurrentLength : (BytesToCopy - LocalBytesCopied);


		if (CopyFromPacketToBuffer) 

		LANCE_MOVE_MEMORY(
				Buffer,
				VirtualAddress,
				AmountToMove
				);

		else 

		LANCE_MOVE_MEMORY(
				VirtualAddress,
				Buffer,
				AmountToMove
				);

		Buffer = (PCHAR)Buffer + AmountToMove;
		LocalBytesCopied += AmountToMove;
		CurrentLength -= AmountToMove;

	}

	*BytesCopied = LocalBytesCopied;

	#ifdef MYDBG
		if (LanceDbg)
		DbgPrint("<==LanceMovePacket\n");
	#endif
}

#ifdef NDIS40_MINIPORT
STATIC
VOID
LanceGetActiveMediaInfo (
	IN PLANCE_ADAPTER Adapter
	)
{

USHORT  MyAbility;
USHORT  LinkPartnerAbility;
USHORT  BitMask = 0x8000;
USHORT  Bcr33Value;
USHORT  NewBcr33Value;


	if (ExtPhyLinkStatus (Adapter->MappedIoBaseAddress))
	{       /* Read speed and duplex mode from ext phy */
		/* Write address of MII register to be read */
		LANCE_READ_BCR (Adapter->MappedIoBaseAddress, MII_ADDR, &Bcr33Value);
		Bcr33Value &= PHYADDR_MASK;
		NewBcr33Value = Bcr33Value | MII_MY_ABILITY;
		LANCE_WRITE_BCR (Adapter->MappedIoBaseAddress, MII_ADDR, NewBcr33Value);

		/* Read Data from MII */
		LANCE_READ_BCR (Adapter->MappedIoBaseAddress, MII_MDR, &MyAbility);
		MyAbility &= ABILITY_MASK;
		MyAbility >>= 5;        /* Align LSB of the ability field at bit 0 */

		/* Write address of MII register to be read */
		LANCE_READ_BCR (Adapter->MappedIoBaseAddress, MII_ADDR, &Bcr33Value);
		Bcr33Value &= PHYADDR_MASK;
		NewBcr33Value = Bcr33Value | MII_LNK_PRTNR;
		LANCE_WRITE_BCR (Adapter->MappedIoBaseAddress, MII_ADDR, NewBcr33Value);

		/* Read Data from MII */
		LANCE_READ_BCR (Adapter->MappedIoBaseAddress, MII_MDR, &LinkPartnerAbility);

		LinkPartnerAbility &= ABILITY_MASK;
		LinkPartnerAbility >>= 5;       /* Align LSB of the ability field at bit 0 */
		MyAbility &= LinkPartnerAbility;        /* AND the two together */
											/* and save in MyAbility */

		if (MyAbility)
		{
			while (!(BitMask & MyAbility))
			{
				BitMask >>= 1;
			}
		}

		switch (BitMask)        /* The order of bit testing is deliberate and is defined */
		{                                       /* in IEEE 802.3 Supplement 802.3u/D5.3 Dated June 12, 1995 */
							/* annex 28B, specifically 28B.2 and 28B.3 */
			case 0x08:      /* 100BASE-TX FD */
				Adapter->FullDuplex = TRUE;
				Adapter->LineSpeed = 100;
				break;

			case 0x10:      /* 100BASE-T4 */
			case 0x04:      /* 100BASE-TX */
				Adapter->FullDuplex = FALSE;
				Adapter->LineSpeed = 100;
				break;

			case 0x02:      /* 10BASE-T FD */
				Adapter->FullDuplex = TRUE;
				Adapter->LineSpeed = 10;
				break;

//                      case 0x01:      /* 10BASE-T */
			default:        /* 10BASE-T */
				Adapter->FullDuplex = FALSE;
				Adapter->LineSpeed = 10;
				break;

		}
	}
	else
	{       /* Read duplex mode from internal phy (10Mbps) */
		Adapter->LineSpeed = 10;
		/* Determine if the internal PHY is in Full Duplex mode */
		LANCE_READ_BCR (Adapter->MappedIoBaseAddress, LANCE_FDC_REG, &Adapter->FullDuplex);
		Adapter->FullDuplex &= LANCE_FDC_FDEN;  /* Non-zero (TRUE) if full duplex */
	}

}
STATIC
BOOLEAN
LanceReadLink (
	IN ULONG IoBaseAddress,
	IN UCHAR DevType,
	IN PLANCE_ADAPTER Adapter
	)
{
	BOOLEAN RetCode;

	/* Read the internal PHY's link status */
	if(Adapter->OpFlags & RESET_IN_PROGRESS)
	{
		return TRUE;
	}
	RetCode = IntPhyLinkStatus (IoBaseAddress);

	switch (DevType)
	{
		case PCNET_PCI2_B2:     /* Internal PHY only on P2 card */
			break;

		case PCNET_PCI3:
			/* If the internal PHY reports link not present */
			/* Try the external PHY ... */
			if (RetCode == FALSE) 
			{
				if(Adapter->OpFlags & RESET_IN_PROGRESS)
				{
					return TRUE;
				}

				RetCode = ExtPhyLinkStatus (IoBaseAddress);
			}
			break;

		default:        /* All other devices are unsupported, so to avoid calamity, */
					/* report valid link always */
			break; //RetCode = TRUE;
	}

	return RetCode;

} /* End of LanceReadLink () */


STATIC
BOOLEAN
ExtPhyLinkStatus (
	IN ULONG IoBaseAddress
	)
{
	USHORT  TempValue;
	USHORT  Bcr33Value;
	USHORT  NewBcr33Value;

	/* Check to see if an external PHY is indicating its presence via       */
	/* the MDIO pin value. Us regular software folk know the MDIO pin       */
	/* as the MIIPD bit, read from BCR32.                                                           */
	/*                                                                                                                                      */
	/* Since some PHYs do not pull up the MDIO pin, reading a zero from     */
	/* MIIPD does not necessarily indicate the absence of an external       */
	/* PHY. Logically, if MDIO = 1 there is an external PHY present,        */
	/* else if MIIPD = 0, there *may* be an external PHY present. (DOH!)*/
	/*                                                                                                                                      */
	/* In summary,                                                                                                          */
	/*      MIIPD = 1       Indicates an external PHY *is* present                          */
	/*      MIIPD = 0       External PHY? Maybe, maybe not!                                         */
	/* Of course, all of this is theory and conjecture anyway...            */

	LANCE_READ_BCR (IoBaseAddress, LANCE_BCR32, &TempValue);
	/* If no PHY is detected, double-check by reading the IEEE ID */
	/* register. If it's all ones (0xFFFF), we'll assume that there's no */
	/* external PHY present */
	if (!(TempValue & MIIPD))
	{
		/* Write address of IEEE ID register */
		LANCE_READ_BCR (IoBaseAddress, MII_ADDR, &Bcr33Value);
		Bcr33Value &= PHYADDR_MASK;
		NewBcr33Value = Bcr33Value | MII_IEEE_ID;
		LANCE_WRITE_BCR (IoBaseAddress, MII_ADDR, NewBcr33Value);

		/* Read IEEE ID from MII data register */
		LANCE_READ_BCR (IoBaseAddress, MII_MDR, &TempValue);

		/* Two assumtions are made here:        */
		/* 1. There is no IEEE ID == 0 or 0xFFFF */
		/* 2. Reading from a PHY that doesn't exist yields either 0 or 0xFFFF */

		if ((TempValue == 0xFFFF) || (TempValue == 0))
		{
			return FALSE;   /* Apparently there is no external PHY. */
		}                                       /* No PHY means no link, right? Bail out here */
	}
	/* Write address of MII register to be read */
	LANCE_READ_BCR (IoBaseAddress, MII_ADDR, &Bcr33Value);
	Bcr33Value &= PHYADDR_MASK; 
	NewBcr33Value = Bcr33Value | MII_STAT_REG;
	LANCE_WRITE_BCR (IoBaseAddress, MII_ADDR, NewBcr33Value);

	/* Read Status from MII */
	LANCE_READ_BCR (IoBaseAddress, MII_MDR, &TempValue);

	/* Test link status bit and exit FALSE if necessary */
	if (!(TempValue & LS0))
	{
		return FALSE;
	}
	return TRUE;
}
STATIC
BOOLEAN
IntPhyLinkStatus (
	IN ULONG IoBaseAddress
	)
{
	USHORT  TempValue;
	USHORT  LinkStatus;
	BOOLEAN RetCode = TRUE; 
	USHORT  FullDuplex;

	/* Determine if the internal PHY is in Full Duplex mode */
	/* and set local variable to indicate the correct state. */
	LANCE_READ_BCR (IoBaseAddress, LANCE_FDC_REG, &FullDuplex);
	FullDuplex &= LANCE_FDC_FDEN;   /* Non-zero (TRUE) if full duplex */

	/* Read and save contents of LED0 status register. */
	LANCE_READ_BCR (IoBaseAddress, LANCE_LED0_STAT, &TempValue);

	/* Mask off any read-only bits */
	TempValue &= LANCE_LINKSE_MASK;

	/* Configure LED0 status reg to indicate link status */
	if (FullDuplex) /* Full duplex link status */
	{
		LANCE_WRITE_BCR (IoBaseAddress, LANCE_LED0_STAT, LANCE_LINK_FDE);
	}
	else    /* Half duplex link status */
	{
		LANCE_WRITE_BCR (IoBaseAddress, LANCE_LED0_STAT, LANCE_LINKSE);
	}

	/* Read link status from LED0 register */
	LANCE_READ_BCR (IoBaseAddress, LANCE_LED0_STAT, &LinkStatus);

	/* Restore LED0 Stat register configuration */
	LANCE_WRITE_BCR (IoBaseAddress, LANCE_LED0_STAT, TempValue);

	/* Test relevant bit(s) and return the proper state. */
	if (!(LinkStatus & LANCE_LED_ON))
	{
		RetCode = FALSE;
	}

	return RetCode;

} /* End IntPhyLinkStatus () */

VOID
LanceCableTimerFunction(
	IN PVOID SystemSpecific1,
	IN PLANCE_ADAPTER Adapter,
	IN PVOID SystemSpecific2,
	IN PVOID SystemSpecific3
	)
{
	BOOLEAN IsCancelled;
	PLANCE_ADAPTER TempAdapter;
	
	#ifdef MYDBG
		if (LanceDbg)
		DbgPrint("==>LanceCableTimerFunction\n");
	#endif

	if (Adapter->OpFlags & RESET_IN_PROGRESS)
		return;
	
#ifdef _FAILOVER
	if (Adapter->RedundantMode == 1)
	{
		if (ActiveAdapter == PRI)
			return;
	}
#endif

	if (Adapter->LinkActive) 
	{
		Adapter->LinkActive = FALSE;
		if (Adapter->CableDisconnected)
		{
			Adapter->CableDisconnected = FALSE;
			NdisMIndicateStatus (Adapter->LanceMiniportHandle,
								NDIS_STATUS_MEDIA_CONNECT,
								NULL,
								0
								);
		}
		return;
	}

	Adapter->OpFlags |= IN_LINK_TIMER;

	if (LanceReadLink (Adapter->MappedIoBaseAddress, Adapter->DeviceType, Adapter))
	{               
		if (Adapter->CableDisconnected)
		{
			Adapter->CableDisconnected = FALSE;
			NdisMIndicateStatus (Adapter->LanceMiniportHandle,
								NDIS_STATUS_MEDIA_CONNECT,
								NULL,
								0
								);
		}
	}
	else
	{
		if (!Adapter->CableDisconnected)
		{
			Adapter->CableDisconnected = TRUE;
			NdisMIndicateStatus (Adapter->LanceMiniportHandle,
								NDIS_STATUS_MEDIA_DISCONNECT,
								NULL,
								0
								);
		}
	}

	Adapter->OpFlags &= ~IN_LINK_TIMER;

	#ifdef MYDBG
		if (LanceDbg)
		DbgPrint("<==LanceCableTimerFunction\n");
	#endif
}

STATIC
VOID
LanceSetExtPhyMedia(
	IN ULONG IoBaseAddress,
	IN ULONG ExtPhyMode
	)
{

	UCHAR   TempByte;
	USHORT Data;

 #ifdef MYDBG
	if (LanceDbg)
		DbgPrint("==>LanceSetExtPhyMedia\n");
 #endif

	LANCE_READ_BCR(IoBaseAddress, LANCE_BCR2, &Data);
	LANCE_WRITE_BCR(IoBaseAddress, LANCE_BCR2, Data|LANCE_BCR2_ASEL);

//      LanceResetExtPhy(IoBaseAddress);

	LANCE_READ_BCR(IoBaseAddress, LANCE_BCR32, &TempByte);
 #ifdef MYDBG
	if (LanceExtPhyDbg)
		DbgPrint("BCR32 : %x\n",TempByte);
 #endif
	TempByte &= FORCED_PHY_MASK;
	TempByte |= (DANAS|XPHYRST);            /* Disable auto-neg. auto setup. */

	switch  (ExtPhyMode)
	{

		case 0:         // Auto-negotiate (Is this correct???)
			TempByte |= XPHYANE;    /* Enable auto-neg. */
			break;

		case 1:         // 100Mb H.D.
			TempByte |= XPHYSP;             /* Set the speed bit */
			break;

		case 2:         // 100Mb F.D.
			TempByte |= (XPHYSP | XPHYFD); /* Set speed and full duplex bits */
			break;

		case 3:         // 10Mb H.D.
			break;

		case 4:         // 10Mb F.D.
			TempByte |= XPHYFD;             /* Set the full duplex bit */
			break;

	}
	/* Write the new BCR32 value with DANAS Set */
	LANCE_WRITE_BCR(IoBaseAddress, LANCE_BCR32, TempByte);

	/* Clear the DANAS bit */
	TempByte &= ~DANAS;             /* Enable auto-neg. auto setup. */

	/* Write the new BCR32 value with DANAS Clear */
	LANCE_WRITE_BCR(IoBaseAddress, LANCE_BCR32, TempByte);

	/* Clear the Reset bit */
	TempByte &= ~XPHYRST;           /* Enable auto-neg. auto setup. */

	/* Write the new BCR32 value with DANAS Clear */
	LANCE_WRITE_BCR(IoBaseAddress, LANCE_BCR32, TempByte);

 #ifdef MYDBG
	if (LanceExtPhyDbg) {
		LANCE_READ_BCR(IoBaseAddress, LANCE_BCR32, &TempByte);
		DbgPrint("BCR32 : %x\n",TempByte);
	}
 #endif
 #ifdef MYDBG
	if (LanceDbg)
		DbgPrint("<==LanceSetExtPhyMedia\n");
 #endif
}

STATIC
BOOLEAN
LanceInitRxPacketPool(
	IN PLANCE_ADAPTER Adapter
	)
/*
 ** LanceInitRxPacketPool
 *
 *  FILENAME:           lance.c
 *
 *  PARAMETERS:         Adapter -- Pointer to the adapter structure.
 *
 *  DESCRIPTION:        Creates the ornate framework required for NDIS 4 to manage
 *                                      a few received packets.
 *
 *  RETURNS:            TRUE if successful, else FALSE. Simple enough, eh?
 *
 */
{
	NDIS_STATUS                                     Status;
	NDIS_HANDLE                                     PktPoolHandle;
	NDIS_HANDLE                                     BufPoolHandle;
	PNDIS_PACKET                                    Packet;
	PNDIS_BUFFER                                    Buffer;
	PNDIS_PACKET_OOB_DATA                   OobyDoobyData;
	PUCHAR                                                  BufferVirtAddr;
	int                                                             n;

 #ifdef MYDBG
	if (LanceDbg)
	{
		DbgPrint("==>LanceInitRxPacketPool\n");
	}
 #endif
	for (n=0; n<RECEIVE_BUFFERS; n++)
	{
		Adapter->pNdisPacket[n] = NULL;
	}

    NdisAllocatePacketPool (&Status, &PktPoolHandle, RECEIVE_BUFFERS, PROT_RESERVED_AREA_SIZE);

	/* Later on , we may elect to reduce the requested number of buffers    */
	/* if the first allocation attempt fails. For now, just match the               */
	/* number of hardware rx descriptors                                                                    */

	if (Status != NDIS_STATUS_SUCCESS)
	{
		return FALSE;
 #ifdef MYDBG
		if (LanceDbg)
		{
			DbgPrint("NdisAllocatePacketPool () Failed.\n<==LanceInitRxPacketPool\n");
		}
 #endif
	}

    NdisAllocateBufferPool (&Status, &BufPoolHandle, RECEIVE_BUFFERS);
	if (Status != NDIS_STATUS_SUCCESS)
	{
 #ifdef MYDBG
		if (LanceDbg)
		{
			DbgPrint("NdisAllocateBufferPool () Failed.\n<==LanceInitRxPacketPool\n");
		}
 #endif
		/* Free the packet pool before exiting. */
		NdisFreePacketPool (PktPoolHandle);
		return FALSE;
	}

	/* The buffers linked to the buffer pool are the same as the receive    */
	/* buffers allocated for the Lance chip. There is one buffer per                */
	/* descriptor.                                                                                                                  */

	/* Get the virtual address of the start of rx buffer space */
	BufferVirtAddr = Adapter->ReceiveBufferPointer;

	for (n=0; n<RECEIVE_BUFFERS; n++)
	{
		/* First, allocate an NDIS packet descriptor*/
		NdisAllocatePacket (&Status, &Packet, PktPoolHandle);

		/* If that worked, allocate an NDIS buffer descriptor */
		if (Status == NDIS_STATUS_SUCCESS)
		{
			NdisAllocateBuffer (&Status, &Buffer, BufPoolHandle, BufferVirtAddr, RECEIVE_BUFFER_SIZE);
		}

		/* If either packet or buffer allocation calls fail, we'll end up here */
		if (Status != NDIS_STATUS_SUCCESS)
		{
			/* Free NDIS packet resources */
			LanceFreeNdisPkts (Adapter);

			/* Free the packet pool before exiting. */
			NdisFreePacketPool (PktPoolHandle);

			/* Free the buffer pool before exiting, too. */
			NdisFreeBufferPool (BufPoolHandle);

 #ifdef MYDBG
			if (LanceDbg)
			{
				DbgPrint("NdisAllocatePacket() and/or NdisAllocateBuffer() Failed.\n<==LanceInitRxPacketPool\n");
			}
 #endif
			return FALSE;
		}

/* initialize the NDIS_PACKET_OOB_DATA block HeaderSize to 14  */

		OobyDoobyData = NDIS_OOB_DATA_FROM_PACKET (Packet);
		NdisZeroMemory (OobyDoobyData, sizeof(PNDIS_PACKET_OOB_DATA));
		OobyDoobyData->HeaderSize = 14;
		OobyDoobyData->SizeMediaSpecificInfo = 0;
		OobyDoobyData->MediaSpecificInformation = NULL;


	   NdisChainBufferAtFront(Packet, Buffer);
		Adapter->pNdisPacket[n] = Packet;
		Adapter->pNdisBuffer[n] = Buffer;
		BufferVirtAddr += RECEIVE_BUFFER_SIZE;
	}
	Adapter->NdisPktPoolHandle = PktPoolHandle;
	Adapter->NdisBufPoolHandle = BufPoolHandle;
 #ifdef MYDBG
			if (LanceDbg)
			{
				DbgPrint("<==LanceInitRxPacketPool\n");
			}
 #endif
	return TRUE;

} /* End of function LanceInitRxPacketPool() */

STATIC
VOID
LanceFreeRxPacketPool(
	IN PLANCE_ADAPTER Adapter
	)
/*
 ** LanceFreeRxPacketPool
 *
 *  FILENAME:           lance.c
 *
 *  PARAMETERS:         Adapter -- Pointer to the adapter structure.
 *
 *  DESCRIPTION:        Creates the ornate framework required for NDIS 4 to manage
 *                                      a few received packets.
 *
 */
{
	/* Free NDIS packet resources */
	LanceFreeNdisPkts (Adapter);

	/* Free the packet pool before exiting. */
	if (Adapter->NdisPktPoolHandle)
		NdisFreePacketPool (Adapter->NdisPktPoolHandle);

	/* Free the buffer pool before exiting, too. */
	if (Adapter->NdisBufPoolHandle)
		NdisFreeBufferPool (Adapter->NdisBufPoolHandle);
}

STATIC
VOID
LanceFreeNdisPkts (
	IN PLANCE_ADAPTER Adapter
	)
/*
 ** LanceFreeNdisPkts
 *
 *  FILENAME:           lance.c
 *
 *  PARAMETERS:         Adapter --      A pointer to the adapter structure
 *
 *  DESCRIPTION:        Frees any and all packet descriptors allocated to us.
 *                                      If a given packet pointer is null, it is skipped.
 *
 *  RETURNS:            void
 *
 */
{
	USHORT                  n;
	PNDIS_PACKET    ThisPacket;
	PNDIS_BUFFER    ThisBuffer;

	for (n=0; n<RECEIVE_BUFFERS; n++)
	{
		ThisPacket = Adapter->pNdisPacket[n];

		if (ThisPacket != NULL)
		{
		    NdisUnchainBufferAtFront (ThisPacket, &ThisBuffer);

			if (ThisBuffer != NULL)
			{
				NdisFreeBuffer (ThisBuffer);
			}
			NdisFreePacket (ThisPacket);
			Adapter->pNdisPacket[n] = NULL;
		}
	}
} /* End of function LanceFreeNdisPkts () */

#endif  /* NDIS40_MINIPORT */

VOID
LanceShutdownHandler(
	IN PVOID a
	)
{
	PLANCE_ADAPTER Adapter = (PLANCE_ADAPTER)a;
	USHORT Data;
	BOOLEAN IsCancelled;

	#ifdef MYDBG
		if (LanceDbg)
		DbgPrint("==>LanceShutdownHandler\n");
	#endif

	NdisMCancelTimer (&(Adapter->CableTimer),&IsCancelled);
	LanceStopChip(Adapter);

	//
	// Do hardware reset
	//
	NdisRawReadPortUshort(Adapter->MappedIoBaseAddress + LANCE_RESET_PORT, &Data);



	//
	// magic packet mode
	//
	if (Adapter->MpMode)
	{
		if (Adapter->DeviceType == PCNET_ISA_PLUS_PLUS || 
			(Adapter->BoardFound == PCI_DEV && Adapter->DeviceType != PCNET_PCI1))
		{
			#ifdef MYDBG
				if (LanceDbg)
					DbgPrint ("I am at Magic Packet Mode code\n");
			#endif

			NdisRawReadPortUshort (Adapter->MappedIoBaseAddress, &Data);            
			LANCE_WRITE_CSR(Adapter->MappedIoBaseAddress, 12, Data);
			NdisRawReadPortUshort (Adapter->MappedIoBaseAddress+2, &Data);          
			LANCE_WRITE_CSR(Adapter->MappedIoBaseAddress, 13, Data);
			NdisRawReadPortUshort (Adapter->MappedIoBaseAddress+4, &Data);          
			LANCE_WRITE_CSR(Adapter->MappedIoBaseAddress, 14, Data);

			//
			// Disable Transmit and Receiver which will not access the 
			// descritor ring
			//
			LANCE_WRITE_CSR(Adapter->MappedIoBaseAddress, LANCE_CSR15, 0x0003);

			LANCE_WRITE_BCR(Adapter->MappedIoBaseAddress, LANCE_BCR7, 0x0200);

			//
			// Set DPOLL in CSR4(bit12) to disable polling
			//
		
			LANCE_WRITE_CSR(Adapter->MappedIoBaseAddress, LANCE_CSR4, 0x1115)
			

			//
			// Magic Packet Mode, Magic Packet Physical Logical Broadcast Accept
			// and Magic Packet Enable(Software Control)
			//
			LANCE_WRITE_CSR(Adapter->MappedIoBaseAddress, LANCE_CSR5, 0x0022);

			//
			// Set START bit in CSR0 to 1
			//
			LANCE_WRITE_CSR(Adapter->MappedIoBaseAddress, LANCE_CSR0, 0x0002);
		}

	}

//SkipMPMode:
	#ifdef MYDBG
	if (LanceDbg)
		DbgPrint("<==LanceShutdownHandler\n");
	#endif
	return;
}
/* Handy code for detecting reentrancy problems */
/* NEVER use in free builds ! */
#ifdef MYDBG
UCHAR LanceMutex (USHORT Flag)
{
UCHAR   OldFlag;
	_asm
	{
		mov             ax, Flag
		xchg    al, Mutex
		mov             OldFlag, al
	}
	if ((Flag == LOCK) && OldFlag)
		_asm    int 3;
	return OldFlag;
}
#endif
