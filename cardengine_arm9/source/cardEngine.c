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

#define _32KB_READ_SIZE 0x8000
#define _64KB_READ_SIZE 0x10000
#define _128KB_READ_SIZE 0x20000
#define _192KB_READ_SIZE 0x30000
#define _256KB_READ_SIZE 0x40000
#define _512KB_READ_SIZE 0x80000
#define _768KB_READ_SIZE 0xC0000
#define _1MB_READ_SIZE 0x100000

#define REG_MBK_WRAM_CACHE_START	0x4004044
#define WRAM_CACHE_ADRESS_START 0x03708000
#define WRAM_CACHE_ADRESS_END 0x03778000
#define WRAM_CACHE_ADRESS_SIZE 0x78000
#define WRAM_CACHE_SLOTS 15

vu32* volatile cardStruct = 0x3707BC0;
//extern vu32* volatile cacheStruct;
extern u32 sdk_version;
extern u32 needFlushDCCache;
vu32* volatile sharedAddr = (vu32*)0x027FFB08;
extern volatile int (*readCachedRef)(u32*); // this pointer is not at the end of the table but at the handler pointer corresponding to the current irq

static u32 WRAM_cacheDescriptor [WRAM_CACHE_SLOTS];
static u32 WRAM_cacheCounter [WRAM_CACHE_SLOTS];
static u32 WRAM_accessCounter = 0;

void user_exception(void);

//---------------------------------------------------------------------------------
void setExceptionHandler2() {
//---------------------------------------------------------------------------------
	exceptionStack = (u32)0x23EFFFC ;
	EXCEPTION_VECTOR = enterException ;
	*exceptionC = user_exception;
}

int WRAM_allocateCacheSlot() {
	int slot = 0;
	int lowerCounter = WRAM_accessCounter;
	for(int i=0; i<WRAM_CACHE_SLOTS; i++) {
		if(WRAM_cacheCounter[i]<=lowerCounter) {
			lowerCounter = WRAM_cacheCounter[i];
			slot = i;
			if(!lowerCounter) break;
		}
	}
	return slot;
}

int WRAM_getSlotForSector(u32 sector) {
	for(int i=0; i<WRAM_CACHE_SLOTS; i++) {
		if(WRAM_cacheDescriptor[i]==sector) {
			return i;
		}
	}
	return -1;
}


vu8* WRAM_getCacheAddress(int slot) {
	return (vu32*)(WRAM_CACHE_ADRESS_END-slot*_32KB_READ_SIZE);
}

void transfertToArm7(int slot) {
	*((vu8*)(REG_MBK_WRAM_CACHE_START+slot)) |= 0x1;
}

void transfertToArm9(int slot) {
	*((vu8*)(REG_MBK_WRAM_CACHE_START+slot)) &= 0xFE;
}

void WRAM_updateDescriptor(int slot, u32 sector) {
	WRAM_cacheDescriptor[slot] = sector;
	WRAM_cacheCounter[slot] = WRAM_accessCounter;
}

int cardRead (u32* cacheStruct, u8* dst0, u32 src0, u32 len0) {
	//nocashMessage("\narm9 cardRead\n");

	setExceptionHandler2();

	u32 commandRead;
	
	u8* dst = dst0;
	u32 src = src0;
	u32 len = len0;

	cardStruct[0] = src;
	if(src==0) {
		return 0;	// If ROM read location is 0, do not proceed.
	}
	cardStruct[1] = dst;
	cardStruct[2] = len;

	u32 page = (src/512)*512;

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
	
		
	WRAM_accessCounter++;

	u32 sector = (src/_32KB_READ_SIZE)*_32KB_READ_SIZE;

	if(page == src && len > _32KB_READ_SIZE && dst < 0x02700000 && dst > 0x02000000 && ((u32)dst)%4==0) {
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
		// read via the main RAM/DSi WRAM cache
		while(len > 0) {
			//if(ROMinRAM==0) {
				int slot = 0;
				vu8* buffer = 0;
				slot = WRAM_getSlotForSector(sector);
				buffer = WRAM_getCacheAddress(slot);
				// read max CACHE_READ_SIZE via the main RAM cache
				if(slot==-1) {
					// send a command to the arm7 to fill the RAM cache
					commandRead = 0x025FFB08;

					slot = WRAM_allocateCacheSlot();

					buffer = WRAM_getCacheAddress(slot);

					if(needFlushDCCache) DC_FlushRange(buffer, _32KB_READ_SIZE);

					// transfer the WRAM-B cache to the arm7
					transfertToArm7(slot);				
					
					// write the command
					sharedAddr[0] = buffer;
					sharedAddr[1] = _32KB_READ_SIZE;
					sharedAddr[2] = sector;
					sharedAddr[3] = commandRead;

					IPC_SendSync(0xEE24);

					while(sharedAddr[3] != (vu32)0);
					
					// transfer back the WRAM-B cache to the arm9
					transfertToArm9(slot);
				}

				WRAM_updateDescriptor(slot, sector);

				u32 len2=len;
				if((src - sector) + len2 > _32KB_READ_SIZE){
					len2 = sector - src + _32KB_READ_SIZE;
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
					copy8(buffer+(src-sector),dst,len2);

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
					copy8(buffer+(page-sector)+(src%512), dst, len2);
					cardStruct[0] = src + len2;
                                        cardStruct[1] = dst + len2;
                                        cardStruct[2] = len - len2;
					//(*readCachedRef)(cacheStruct);
				}
				len = cardStruct[2];
				if(len>0) {
					src = cardStruct[0];
					dst = cardStruct[1];
					page = (src/512)*512;
					sector = (src/_32KB_READ_SIZE)*_32KB_READ_SIZE;
					WRAM_accessCounter++;
				}
			//}
		}
	}
	return 0;
}




