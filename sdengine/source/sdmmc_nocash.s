
	.text
	.align	4
	.arm

	.global	sdmmc_init_device
	.type	sdmmc_init_device STT_FUNC

sdmmc_curr_device:
	.word	0x00000000
sdmmc_curr_is_sdhc:
	.word	0x00000001
sdmmc_sha1_cid:
	.word	0x00000000

@------------------
sdmmc_init_device:
 push {r0-r12,lr}
 ldr  r0,=sdmmc_sha1_cid   @dst @\
 ldr  r1,=0x2FFD7BC  @cid   @src @ SHA1 init+update+fin
 mov  r2,#0x10              @len @
 swi  0x270000   @0x27 shl 16 sha1        @/
 @---
 ldr  r9,=0x4004800      @-base
 ldrh r0,[r9,#0x0e0]      @\
 bic  r0,#3              @ SD_SOFT_RESET clear bit0-1
 strh r0,[r9,#0x0e0]      @/
 ldrh r0,[r9,#0x0e0]      @\
 orr  r0,#3              @ SD_SOFT_RESET set bit0-1
 strh r0,[r9,#0x0e0]      @/
 ldrh r0,[r9,#0x008]      @\
 bic  r0,#1              @ SD_STOP_INTERNAL clear bit0
 orr  r0,#0x100 @autostop @
 strh r0,[r9,#0x008]      @/
 ldrb r0,=sdmmc_curr_device            @\
 orr  r0,#0x400  @bit0: 0=sd slot, 1=eMMC @  SD_CARD_PORT_SELECT
 strh r0,[r9,#0x002]                      @/

 mov  r0,#0x0040          @\SD_CARD_CLK_CTL = 0040h
@mov r0,#0x0020  @try affect timer...?
@mov r0,#0x002
mov r0,#0
 strh r0,[r9,#0x024]      @/
 ldr  r0,=#0x00d0         @\SD_CARD_OPTION = 80D0h (timeouts, buswidth)
 strh r0,[r9,#0x028]      @/
 ldrh r0,[r9,#0x028]      @\
 orr  r0,#0x8000          @ SD_CARD_OPTION set bit15
   bic r0,#0x8000               @clear --> want 4bit DATA mode !!!
 strh r0,[r9,#0x028]      @/
 @---part2b
 ldrh r0,[r9,#0x028]      @\
 orr  r0,#0x0100          @ SD_CARD_OPTION set bit8
 strh r0,[r9,#0x028]      @/
 ldrh r0,[r9,#0x028]      @\
 bic  r0,#0x0100          @ SD_CARD_OPTION clear bit8
 strh r0,[r9,#0x028]      @/
 @---part3a
 ldrh r0,[r9,#0x024]      @\
 orr  r0,#0x0100          @ SD_CARD_CLK_CTL set bit8
mov r0,#0x100  @<-- this works, unlike ORing zero by 100h ???
@@@@add r0,#0x40
 strh r0,[r9,#0x024]      @/

 mov  r0,#0x002                           @\want data32 mode, step 1
 strh r0,[r9,#0x0d8] @SD_DATA_CTL         @/
 ldr  r0,=#0x402 @clear fifo, data32 mode @\want data32 mode, step 2
 str  r0,[r9,#0x100] @SD_IRQ32            @/

 pop  {r0-r12,pc}

.pool
 
 	.global	sdmmc_load_sector
	.type	sdmmc_load_sector STT_FUNC
@------------------
sdmmc_load_sector:       @in: r0=sector, r1=dest
 mov  r2,#0x200  @len
 
  	.global	sdmmc_load_sectors
	.type	sdmmc_load_sectors STT_FUNC
@- - - - - --------
sdmmc_load_sectors:      @in: r0=sector, r1=dest, r2=len
 push {r0-r12,lr}
 mov  r10,r0  @sector
 mov  r11,r1  @dest
 mov  r12,r2  @len
 ldr  r9,=0x4004800                              @\
 mov  r0,r12,lsr 9  @num_blks = len/200h       @ apply num blks
 strh r0,[r9,#0x00a] @SD_NUMBLK16                 @
 str  r0,[r9,#0x108] @SD_NUMBLK32                 @/
 mov  r0,#0x200                                   @\
 strh r0,[r9,#0x026] @SD_BLKLEN16                 @ apply blk_len
 str  r0,[r9,#0x104] @SD_BLKLEN32                 @/
 ldrb r0,=sdmmc_curr_is_sdhc                   @\
 cmp  r0,#0                                      @
 moveq r0,r10,lsl #9  @sector*200h (SDSC)        @ issue READ_MULTIPLE
 movne r0,r10        @sector (SDHC/SDXC)        @
 mov  r1,#0x12      @CMD18 READ_MULTIPLE          @
 bl   sdmmc_read_register                       @/
blk_lop_1:                                      @-lop blks...
 mov  r1,r11    @dest                           @\
 mov  r2,#0x200   @len                            @ read sector
 bl   sdmmc_read_data                           @/
 ldrb r0,=sdmmc_curr_device                    @\
 cmp  r0,#1      @1=eMMC (need decrypt)          @ decrypt (if eMMC)
 mov  r0,r10    @sector                         @
 mov  r1,r11    @src/dst                        @
@ bleq sdmmc_decrypt_sector                      @/ only needed for nand
 add  r10,#1     @sector                         @\
 add  r11,#0x200  @dest                           @ lop next blk
 subs r12,#0x200  @len                            @
 bne  blk_lop_1                                 @/
@ mov  r0,#0x0001 shl 16    @XXX RCA               @\
@ mov  r1,#0x0d    @CMD13 SEND_STATUS              @ issue GET_STATUS
@ bl   sdmmc_read_register                       @/  XXX for SD: need other RCA?
 pop  {r0-r12,pc} 
 
.pool
 
  	.global	sdmmc_write_sector
	.type	sdmmc_write_sector STT_FUNC
@------------------
sdmmc_write_sector:       @in: r0=sector, r1=src
 mov  r2,#0x200  @len
 
  	.global	sdmmc_write_sectors
	.type	sdmmc_write_sectors STT_FUNC
@- - - - - --------
sdmmc_write_sectors:      @in: r0=sector, r1=src, r2=len
 push {r0-r12,lr}
 mov  r10,r0  @sector
 mov  r11,r1  @src
 mov  r12,r2  @len
 ldr  r9,=0x4004800                              @\
 mov  r0,r12,lsr #9  @num_blks = len/200h       @ apply num blks
 strh r0,[r9,#0x00a] @SD_NUMBLK16                 @
 str  r0,[r9,#0x108] @SD_NUMBLK32                 @/
 mov  r0,#0x200                                   @\
 strh r0,[r9,#0x026] @SD_BLKLEN16                 @ apply blk_len
 str  r0,[r9,#0x104] @SD_BLKLEN32                 @/
 ldrb r0,=sdmmc_curr_is_sdhc                   @\
 cmp  r0,#0                                      @
 moveq r0,r10,lsl #9  @sector*200h (SDSC)        @ issue WRITE_MULTIPLE
 movne r0,r10        @sector (SDHC/SDXC)        @
 mov  r1,#0x19      @CMD25 WRITE_MULTIPLE          @
 bl   sdmmc_read_register                       @/
blk_lop:                                      @-lop blks...
 ldrb r0,=sdmmc_curr_device                    @\
 cmp  r0,#1      @1=eMMC (need decrypt)          @ encrypt (if eMMC)
 mov  r0,r10    @sector                         @
 mov  r1,r11    @src/dst                        @
@ bleq sdmmc_encrypt_sector                      @/ only needed for nand
 mov  r1,r11    @src                            @\
 mov  r2,#0x200   @len                            @ write sector
 bl   sdmmc_write_data                          @/
 add  r10,#1     @sector                         @\
 add  r11,#0x200  @dest                           @ lop next blk
 subs r12,#0x200  @len                            @
 bne  blk_lop                                 @/
@ mov  r0,#0x0001 shl 16    @XXX RCA               @\
@ mov  r1,#0x0d    @CMD13 SEND_STATUS              @ issue GET_STATUS
@ bl   sdmmc_read_register                       @/  XXX for SD: need other RCA?
 pop  {r0-r12,pc}
 
.pool


@------------------
sdmmc_read_register:   @in: r0=param, r1=cmd
 push {r1-r12,lr}
@@@ bl   sdmmc_verify_fixed_values @@@ Verify_fixed_values would be some optional selfcheck for rev-engineering the hardware 
 mov r2,#0x100
 pre_wait:
 subs r2,#1
 bne pre_wait

 ldr  r9,=0x4004800
 strh r0,[r9,#0x004]      @\
 mov  r0,r0,lsr 16      @ SD_CMD_PARAM
 strh r0,[r9,#0x06]       @/
 ldr  r0,[r9,#0x01c]      @\SD_IRQ_STAT
 bic  r0,#0x83000000      @ clear bit31,25,24 (error, txrq, rxrdy)
 bic  r0,#0x007f0000      @ clear bit22..16   (error)
 bic  r0,#0x00000005      @ clear bit2,0      (dataend,cmdrespend)
 mov r0,#0
 str  r0,[r9,#0x01c]      @/

@@@mov r0,#-1              @\
@@@str  r0,[r9,#0x020]      @/
@@@ bl   sdmmc_verify_fixed_values @@@ Verify_fixed_values would be some optional selfcheck for rev-engineering the hardware 
 ldrh r0,[r9,#0x008]      @\
 bic  r0,#1              @ SD_STOP_INTERNAL clear bit0
 strh r0,[r9,#0x008]      @/
@@@ bl   sdmmc_verify_fixed_values @@@ Verify_fixed_values would be some optional selfcheck for rev-engineering the hardware 
 strh r1,[r9,#0x000]      @-SD_CMD
 @- - -
 mov  r2,#0x100000 @timeout (ca. 1 Million) @<-- ENDLESS SLOW (at least several MINUTES... or HOURS) (ie. in fact NEVER)
 mov r2,#0x1000                          @<-- REASONABLE SLOW (hmmm, even for 256-bytes, this is WAYS FASTER than above delay, which is 256-times slower, how is THAT possible??)
 mov r2,#0x10000
 
busy_lop:
@@@ bl   sdmmc_verify_fixed_values @@@ Verify_fixed_values would be some optional selfcheck for rev-engineering the hardware 
 ldrh r0,[r9,#0x02c]      @\SD_ERROR_DETAIL_LO
 tst   r0,#0x005          @  bit0, CMD CmdIndex-Error
 tsteq r0,#0x100          @  bit2, CMD End-Bit-Error
 bne  error_detail    @/ bit8, CMD CRC-Error
 ldrh r0,[r9,#0x01c]      @\SDIO_IRQ_STAT_LO
 tst  r0,#1              @  bit0, CMDRESPEND
 bne  busy_done       @/
 subs r2,#1              @\lop next, till timeout
 bne  busy_lop        @/
 b    error_sw_timeout     @XXX accessing DISABLED function 1 does HANG (rather than reaching timeout?)
 @---
busy_done:
@@@ bl   sdmmc_verify_fixed_values @@@ Verify_fixed_values would be some optional selfcheck for rev-engineering the hardware 
 ldrh r0,[r9,#0x01e]      @\SD_IRQ_STAT_HI
 tst  r0,#0x40            @  bit22, CMDTIMEOUT
 bne  error_hw_timeout @/
 ldr  r0,[r9,#0x00c]      @\SD_REPLY (00001000h, ie. state = "dis"@ if its "CSR")
@@@ and  r0,#0x00ff          @/
done:
        @@ bl   wrhex32bit
        @@bl wrcrlf
@@@ bl   sdmmc_verify_fixed_values @@@ Verify_fixed_values would be some optional selfcheck for rev-engineering the hardware 
 pop  {r1-r12,pc}
@---
error_detail:
error_sw_timeout:
error_hw_timeout:
 mov  r0,#-1
 b    done
.pool

@------------------
sdmmc_read_data:    @in: r1=dst, r2=len
 @based on experimental "sdmmc_data_xfer"...
 push {r1-r12,lr}
 mov  r11,r1
 mov  r12,r2
 ldr  r9,=0x4004800              @-
 ldrh r0,[r9,#0x0d8] @SD_DATA_CTL         @\
 tst  r0,#2                              @
 beq  dta16                           @ redirect data16 / data32
 ldr  r0,[r9,#0x100] @SD_DATA32_IRQ       @
 tst  r0,#2                              @
 beq  dta16                           @/
@---DATA32...
 mov  r2,#0x10000                 @\
 
dta32_wait_rxrdy:             @
 subs r2,#1                      @ wait for data
 beq  error_sw_timeout_r        @

@ldrh r0,[r9,#0x01e] @SD_IRQ_STAT_HI
@tst  r0,#0x100  @RXRDY32 (full)  @

 ldr  r0,[r9,#0x100] @SD_IRQ32    @
 tst  r0,#0x100 @RXRDY32 (full)   @
 beq  dta32_wait_rxrdy            @/

dta32_rx_lop:                 @\
 ldr  r0,[r9,#0x10c]  @SD_DATA32  @
 str  r0,[r11],#4                @ read data
 subs r12,#4                     @
 bne  dta32_rx_lop            @/
 b    finish
@-------
dta16:
 mov  r2,#0x10000                 @\
 
dta16_wait_rxrdy:             @
 subs r2,#1                      @ wait for data
 beq  error_sw_timeout_r        @
 ldrh r0,[r9,#0x01e] @SD_IRQ_STAT_HI
 tst  r0,#0x100  @RXRDY16         @
 beq  dta16_wait_rxrdy        @/

dta16_rx_lop:                 @\
 ldrh r0,[r9,#0x030]  @SD_DATA16  @
 strh r0,[r11],#2                @ read data
 subs r12,#2                     @
 bne  dta16_rx_lop            @/
 b    finish
@- - - -
finish:
wait_dataend:             @\
 subs r2,#1                  @
 beq  error_sw_timeout_r    @
 ldrh r0,[r9,#0x01c]          @ SD_IRQ_STAT_LO
 tst  r0,#4     @dataend     @          (works BETTER with this!)
 beq  wait_dataend        @/
 mov  r0,#0   @okay
done_r:
 pop  {r1-r12,pc}
@---
error_sw_timeout_r:
 mov  r0,#-1  @error/timeout
 b    done_r
@------------------
.pool


sdmmc_write_data:     @in: r1=src, r2=len
 push {r1-r12,lr}
 mov  r11,r1     @src
 mov  r12,r2     @len
 ldr  r9,=0x4004800
 mov  r2,#0x1000000                       @\
 
wait_txrq:                            @
 subs r2,#1                              @
 beq  error_sw_timeout_w                @ wait ready
@@@ ldrh r0,[r9,#0x01e] @SD_IRQ_STAT_HI
@@@ tst  r0,#0x200  @txrq
@@@ mov r0,not #0x200
@@@ strh r0,[r9,#0x01e]     @ACK now
 ldr  r0,[r9,#0x100]                      @
 tst  r0,#0x200  @TXRDY32 (0=empty)       @
 bne  wait_txrq                       @/
dta_tx_lop:                           @\
 ldr  r0,[r11],#4                        @
 str  r0,[r9,#0x10c]  @SD_DATA32          @ write data
 subs r12,#4                             @
 bne  dta_tx_lop                      @/
wait_dataend_w:                         @\
 subs r2,#1                              @
 beq  error_sw_timeout_w                @ wait end
 ldrh r0,[r9,#0x01c] @SD_IRQ_STAT         @
 tst  r0,#4     @dataend                 @          (works BETTER with this!)
 beq  wait_dataend_w                    @/
 mov  r0,#0   @okay
done_w:
 pop  {r1-r12,pc}
@---
error_sw_timeout_w:
 mov  r0,#-1  @error/timeout
 b    done_w
 .pool