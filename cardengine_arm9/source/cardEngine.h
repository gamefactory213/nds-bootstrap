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

#ifndef CARD_ENGINE_ARM9_H
#define CARD_ENGINE_ARM9_H

#define READ_SIZE_ARM7 0x8000

#define CACHE_ADRESS_START 0x03708000
#define CACHE_ADRESS_SIZE 0x78000
#define REG_MBK_CACHE_START	0x4004045
#define REG_MBK_CACHE_SIZE	15
#define PREFETCH_MARKER_SELECTED 0xffffffff

#ifdef __cplusplus
extern "C" {
#endif

#define is_aligned(POINTER, BYTE_COUNT) \
    (((uintptr_t)(const void *)(POINTER)) % (BYTE_COUNT) == 0)

void cardRead (u32* cacheStruct);

#ifdef __cplusplus
}
#endif

#endif // CARD_ENGINE_ARM9_H
