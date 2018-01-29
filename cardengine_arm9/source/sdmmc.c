#include <nds.h>
#include "sdmmc.h"
#include "disc_io.h"

extern vu32* volatile sharedAddr;

//---------------------------------------------------------------------------------
int sdmmc_cardinserted() {
//---------------------------------------------------------------------------------
	return 1; //sdmmc_cardready;
}


//---------------------------------------------------------------------------------
int sdmmc_sdcard_init() {
//---------------------------------------------------------------------------------
    return 0;

}


int __attribute__((noinline)) sdmmc_sdcard_readsectors(u32 sector_no, u32 numsectors, void *out) {
	sharedAddr[0] = out;
	sharedAddr[1] = numsectors;
	sharedAddr[2] = sector_no;
	sharedAddr[3] = 0x52454144;	// READ

	IPC_SendSync(0xEE24);

	while(sharedAddr[3] == (vu32)0x52454144);

    return sharedAddr[3];
}

int __attribute__((noinline)) sdmmc_sdcard_writesectors(u32 sector_no, u32 numsectors, void *in) {
	sharedAddr[0] = in;
	sharedAddr[1] = numsectors;
	sharedAddr[2] = sector_no;
	sharedAddr[3] = 0x57524954;	// WRIT

	IPC_SendSync(0xEE24);

	while(sharedAddr[3] == (vu32)0x57524954);

    return sharedAddr[3];
}

/*-----------------------------------------------------------------
startUp
Initialize the interface, geting it into an idle, ready state
returns true if successful, otherwise returns false
-----------------------------------------------------------------*/
bool startup(void) {	
	return true;	
}

/*-----------------------------------------------------------------
isInserted
Is a card inserted?
return true if a card is inserted and usable
-----------------------------------------------------------------*/
bool isInserted (void) {
	return true;
}


/*-----------------------------------------------------------------
clearStatus
Reset the card, clearing any status errors
return true if the card is idle and ready
-----------------------------------------------------------------*/
bool clearStatus (void) {
	return true;
}


/*-----------------------------------------------------------------
readSectors
Read "numSectors" 512-byte sized sectors from the card into "buffer", 
starting at "sector". 
The buffer may be unaligned, and the driver must deal with this correctly.
return true if it was successful, false if it failed for any reason
-----------------------------------------------------------------*/
bool readSectors (u32 sector, u32 numSectors, void* buffer) {
	return sdmmc_sdcard_readsectors(sector,numSectors,buffer)==0;
}



/*-----------------------------------------------------------------
writeSectors
Write "numSectors" 512-byte sized sectors from "buffer" to the card, 
starting at "sector".
The buffer may be unaligned, and the driver must deal with this correctly.
return true if it was successful, false if it failed for any reason
-----------------------------------------------------------------*/
bool writeSectors (u32 sector, u32 numSectors, void* buffer) {
	return sdmmc_sdcard_writesectors(sector,numSectors,buffer)==0;
}


/*-----------------------------------------------------------------
shutdown
shutdown the card, performing any needed cleanup operations
Don't expect this function to be called before power off, 
it is merely for disabling the card.
return true if the card is no longer active
-----------------------------------------------------------------*/
bool shutdown(void) {
	return true;
}

const IO_INTERFACE __myio_dsisd = {
	DEVICE_TYPE_DSI_SD,
	FEATURE_MEDIUM_CANREAD | FEATURE_MEDIUM_CANWRITE,
	(FN_MEDIUM_STARTUP)&startup,
	(FN_MEDIUM_ISINSERTED)&isInserted,
	(FN_MEDIUM_READSECTORS)&readSectors,
	(FN_MEDIUM_WRITESECTORS)&writeSectors,
	(FN_MEDIUM_CLEARSTATUS)&clearStatus,
	(FN_MEDIUM_SHUTDOWN)&shutdown
};
