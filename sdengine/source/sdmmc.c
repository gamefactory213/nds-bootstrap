#include <nds.h>
#include <nds/arm7/sdmmc.h>
#include "disc_io.h"

static struct mmcdevice deviceSD;


//---------------------------------------------------------------------------------
int sdmmc_cardinserted() {
//---------------------------------------------------------------------------------
	return 1; //sdmmc_cardready;
}

//---------------------------------------------------------------------------------
static u32 calcSDSize(u8* csd, int type) {
//---------------------------------------------------------------------------------
    u32 result = 0;
    if (type == -1) type = csd[14] >> 6;
    switch (type) {
        case 0:
            {
                u32 block_len = csd[9] & 0xf;
                block_len = 1 << block_len;
                u32 mult = (csd[4] >> 7) | ((csd[5] & 3) << 1);
                mult = 1 << (mult + 2);
                result = csd[8] & 3;
                result = (result << 8) | csd[7];
                result = (result << 2) | (csd[6] >> 6);
                result = (result + 1) * mult * block_len / 512;
            }
            break;
        case 1:
            result = csd[7] & 0x3f;
            result = (result << 8) | csd[6];
            result = (result << 8) | csd[5];
            result = (result + 1) * 1024;
            break;
    }
    return result;
}

/*-----------------------------------------------------------------
startUp
Initialize the interface, geting it into an idle, ready state
returns true if successful, otherwise returns false
-----------------------------------------------------------------*/
bool startup(void) {	
	nocashMessage("startup internal");
	return true;	
}

/*-----------------------------------------------------------------
isInserted
Is a card inserted?
return true if a card is inserted and usable
-----------------------------------------------------------------*/
bool isInserted (void) {
	nocashMessage("isInserted internal");
	return true;
}


/*-----------------------------------------------------------------
clearStatus
Reset the card, clearing any status errors
return true if the card is idle and ready
-----------------------------------------------------------------*/
bool clearStatus (void) {
	nocashMessage("clearStatus internal");
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
	nocashMessage("readSectors internal");
	//dbg_printf("readSectors internal");
	return sdmmc_load_sectors(sector,buffer,numSectors)==0;
}



/*-----------------------------------------------------------------
writeSectors
Write "numSectors" 512-byte sized sectors from "buffer" to the card, 
starting at "sector".
The buffer may be unaligned, and the driver must deal with this correctly.
return true if it was successful, false if it failed for any reason
-----------------------------------------------------------------*/
bool writeSectors (u32 sector, u32 numSectors, void* buffer) {
	nocashMessage("writeSectors internal");
	//dbg_printf("writeSectors internal");
	return sdmmc_write_sectors(sector,buffer,numSectors)==0;
}


/*-----------------------------------------------------------------
shutdown
shutdown the card, performing any needed cleanup operations
Don't expect this function to be called before power off, 
it is merely for disabling the card.
return true if the card is no longer active
-----------------------------------------------------------------*/
bool shutdown(void) {
	nocashMessage("shutdown internal");
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
