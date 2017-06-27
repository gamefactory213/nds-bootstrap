/*
    NitroHax -- Cheat tool for the Nintendo DS
    Copyright (C) 2008  Michael "Chishm" Chisholm

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <nds.h> 
#include <nds/fifomessages.h>
#include "cardEngine.h"

extern vu32* volatile cardStruct;
//extern vu32* volatile cacheStruct;
extern u32 sdk_version;
extern u32 needFlushDCCache;
vu32* volatile sharedAddr = (vu32*)0x027FFB08;
extern volatile int (*readCachedRef)(u32*); // this pointer is not at the end of the table but at the handler pointer corresponding to the current irq

static u32 cacheDescriptor [REG_MBK_CACHE_SIZE];
static u32 cacheCounter [REG_MBK_CACHE_SIZE];
static u32 accessCounter = 0;

int allocateCacheSlot() {
	int slot = 0;
	int lowerCounter = accessCounter;
	for(int i=0; i<REG_MBK_CACHE_SIZE; i++) {
		if(cacheCounter[i]<=lowerCounter) {
			lowerCounter = cacheCounter[i];
			slot = i;
			if(!lowerCounter) break;
		}
	}
	return slot;
}

int getSlotForSector(u32 sector) {
	for(int i=0; i<REG_MBK_CACHE_SIZE; i++) {
		if(cacheDescriptor[i]==sector) {
			if(cacheCounter[i]==PREFETCH_MARKER_SELECTED) return -2;
			else return i;
		}
	}
	return -1;
}

vu8* getCacheAddress(int slot) {
	return (vu32*)(CACHE_ADRESS_START+slot*READ_SIZE_ARM7);
}

void transfertToArm7(int slot) {
	*((vu8*)(REG_MBK_CACHE_START+slot)) |= 0x1;
}

void transfertToArm9(int slot) {
	*((vu8*)(REG_MBK_CACHE_START+slot)) &= 0xFE;
}

void updateDescriptor(int slot, u32 sector, bool prefetch) {
	cacheDescriptor[slot] = sector;
	if(!prefetch)
		cacheCounter[slot] = accessCounter;
	else
		cacheCounter[slot] = PREFETCH_MARKER_SELECTED;
}

static bool preftechInitialized = false;
static int prefechSlot = 0;

initializePreftech() {
	int oldIME = enterCriticalSection();	
	REG_IPC_SYNC |= IPC_SYNC_IRQ_ENABLE;
	REG_IE |= IRQ_IPC_SYNC;

	preftechInitialized = true;
	leaveCriticalSection(oldIME);
}

void doPrefecth() {
	#ifdef DEBUG		
	nocashMessage("doPrefecth");
	#endif	
	if(tryLockMutex()) {	
		if(prefechSlot!=0) {
			u32 sector = cacheDescriptor[prefechSlot];
			vu8* buffer = getCacheAddress(prefechSlot);
			// send a command to the arm7 to fill the WRAM cache
			u32 commandRead = 0x025FFB08;			

			#ifdef DEBUG		
				// send a log command for debug purpose
				// -------------------------------------
				commandRead = 0x026ff800;	
				
				sharedAddr[0] = buffer;
				sharedAddr[1] = 9999;
				sharedAddr[2] = sector;
				sharedAddr[3] = commandRead;
				
				IPC_SendSync(0xEE24);
				
				while(sharedAddr[3] != (vu32)0);
				// -------------------------------------*/
			#endif	
			
			buffer = getCacheAddress(prefechSlot);
			
			if(needFlushDCCache) DC_FlushRange(buffer, READ_SIZE_ARM7);
			
			// transfer the WRAM-B cache to the arm7
			transfertToArm7(prefechSlot);				
			
			// write the command
			sharedAddr[0] = buffer;
			sharedAddr[1] = READ_SIZE_ARM7;
			sharedAddr[2] = sector;
			sharedAddr[3] = commandRead;
			
			IPC_SendSync(0xEE24);	

			while(sharedAddr[3] != (vu32)0);	
			
			// transfer back the WRAM-B cache to the arm9
			transfertToArm9(prefechSlot);	

			updateDescriptor(prefechSlot, sector, false);	

			prefechSlot=0;			
		}
		unlockMutex();
	}
}

void cardRead (u32* cacheStruct) {
	//nocashMessage("\narm9 cardRead\n");	
	
	/*if(!preftechInitialized) {
		initializePreftech();
	}*/
	
	accessCounter++;
	
	u8* cacheBuffer = (u8*)(cacheStruct + 8);
	u32* cachePage = cacheStruct + 2;
	u32 commandRead;
	u32 src = cardStruct[0];
	u8* dst = (u8*) (cardStruct[1]);
	u32 len = cardStruct[2];
	
	u32 page = (src/512)*512;
	
	u32 sector = (src/READ_SIZE_ARM7)*READ_SIZE_ARM7;
	
	#ifdef DEBUG
	// send a log command for debug purpose
	// -------------------------------------
	commandRead = 0x026ff800;	
	
	sharedAddr[0] = dst;
	sharedAddr[1] = len;
	sharedAddr[2] = src;
	sharedAddr[3] = commandRead;
	
	IPC_SendSync(0xEE24);
	
	while(sharedAddr[3] != (vu32)0);
	// -------------------------------------*/
	#endif

	
	if(page == src && len > READ_SIZE_ARM7 && dst < 0x02700000 && dst > 0x02000000 && ((u32)dst)%4==0) {
		// read directly at arm7 level
		commandRead = 0x025FFB08;
		
		cacheFlush();
		
		sharedAddr[0] = dst;
		sharedAddr[1] = len;
		sharedAddr[2] = src;
		sharedAddr[3] = commandRead;
		
		IPC_SendSync(0xEE24);
		
		while(sharedAddr[3] != (vu32)0);
		
	} else {
		// read via the WRAM cache
		while(len > 0) {			
			int slot = getSlotForSector(sector);
			vu8* buffer = getCacheAddress(slot);
			if(slot==-2) {		
				// marked for prefetch but not fully executed yet
				doPrefecth(); // ensure that the prefetch is executed if not already ongoig
				lockMutex(); // wait for prefetch to complete
				unlockMutex();
				
			} else if(slot==-1) {		
				// read max 32k via the WRAM cache	
				//lockMutex(); // wait for prefetch to complete, if any
				// send a command to the arm7 to fill the WRAM cache
				commandRead = 0x025FFB08;				
				
				slot = allocateCacheSlot();
				
				buffer = getCacheAddress(slot);
				
				if(needFlushDCCache) DC_FlushRange(buffer, READ_SIZE_ARM7);
				
				// transfer the WRAM-B cache to the arm7
				transfertToArm7(slot);				
				
				// write the command
				sharedAddr[0] = buffer;
				sharedAddr[1] = READ_SIZE_ARM7;
				sharedAddr[2] = sector;
				sharedAddr[3] = commandRead;
				
				IPC_SendSync(0xEE24);	

				while(sharedAddr[3] != (vu32)0);	
				
				// transfer back the WRAM-B cache to the arm9
				transfertToArm9(slot);					
				//unlockMutex();
			}

			// mark for prefetch the next sector unless some prefetch is already ongoing
			/*if(tryLockMutex()) {				
				int slotp = getSlotForSector(sector+READ_SIZE_ARM7);
				if(slotp==-1) {
					slotp = allocateCacheSlot();	
					updateDescriptor(slotp, sector+READ_SIZE_ARM7, true );
					prefechSlot = slotp;
				}
				unlockMutex();
			}*/
		
			updateDescriptor(slot, sector, false);

			
			u32 len2=len;
			if((src - sector) + len2 > READ_SIZE_ARM7){
			    len2 = sector - src + READ_SIZE_ARM7;
			}
			
			if(len2 > 512) {
				len2 -= src%4;
				len2 -= len2 % 32;
			}

			if(len2 >= 512 && len2 % 32 == 0 && ((u32)dst)%4 == 0 && src%4 == 0) {		
				#ifdef DEBUG		
				// send a log command for debug purpose
				// -------------------------------------
				commandRead = 0x026ff800;	
				
				sharedAddr[0] = dst;
				sharedAddr[1] = len2;
				sharedAddr[2] = buffer+src-sector;
				sharedAddr[3] = commandRead;
				
				IPC_SendSync(0xEE24);
				
				while(sharedAddr[3] != (vu32)0);
				// -------------------------------------*/
				#endif
			
				// copy directly
				fastCopy32(buffer+(src-sector),dst,len2);	
				
				// update cardi common
				cardStruct[0] = src + len2;
				cardStruct[1] = dst + len2;
				cardStruct[2] = len - len2;
			} else {				
				#ifdef DEBUG		
				// send a log command for debug purpose
				// -------------------------------------
				commandRead = 0x026ff800;	
				
				sharedAddr[0] = page;
				sharedAddr[1] = len2;
				sharedAddr[2] = buffer+page-sector;
				sharedAddr[3] = commandRead;
				
				IPC_SendSync(0xEE24);
				
				while(sharedAddr[3] != (vu32)0);
				// -------------------------------------*/
				#endif
					
				// read via the 512b ram cache
				fastCopy32(buffer+(page-sector), cacheBuffer, 512);
				*cachePage = page;
				(*readCachedRef)(cacheStruct);
			}
			len = cardStruct[2];
			if(len>0) {
				src = cardStruct[0];
				dst = cardStruct[1];
				page = (src/512)*512;
				sector = (src/READ_SIZE_ARM7)*READ_SIZE_ARM7;
				accessCounter++;
			}			
		}
	}
}

//---------------------------------------------------------------------------------
void myIrqHandlerFIFO(void) {
//---------------------------------------------------------------------------------
	#ifdef DEBUG		
	nocashMessage("myIrqHandlerFIFO");
	#endif	
	//doPrefecth();	
}


void myIrqHandlerVBlank(void) {
	#ifdef DEBUG		
	nocashMessage("myIrqHandlerVBlank");
	#endif	
	//doPrefecth();
}




