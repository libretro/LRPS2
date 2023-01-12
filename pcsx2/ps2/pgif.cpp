/*  PCSX2 - PS2 Emulator for PCs
*  Copyright (C) 2016-2016  PCSX2 Dev Team
*  Copyright (C) 2016 Wisi
*
*  PCSX2 is free software: you can redistribute it and/or modify it under the terms
*  of the GNU Lesser General Public License as published by the Free Software Found-
*  ation, either version 3 of the License, or (at your option) any later version.
*
*  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
*  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
*  PURPOSE.  See the GNU General Public License for more details.
*
*  You should have received a copy of the GNU General Public License along with PCSX2.
*  If not, see <http://www.gnu.org/licenses/>.
*/

#include "ps2/Iop/IopHw_Internal.h"
#include "ps2/HwInternal.h"
#include "ps2/pgif.h"

#define psxHu(mem) (*(u32 *)&iopHw[(mem)&0xffff])

//Constants here control code that is either not certainly correct or may affect compatibility:

//Default=1. Enables changing the data FIFO/DMA direction bit in the Status register by the PGIF Ctrl register.
#define STAT_DIR_BY_IF_CTRL 1
//Specifies which bits (29 and 30) from the "Set DMA direction" command to copy to the PGPU Status reg.
//Default(works better)=0x40000000, but makes more sense to be =0x20000000 or =0x60000000.
//Status reg. bits 30:29: DMA Direction (0=Off, 1=FIFO, 2=CPUtoGP0, 3=GPUREADtoCPU) ;GPUSTAT.29-30
#define STAT_DIR_BY_CMD_BITS 0x40000000

//Default=1.
#define PREVENT_IRQ_ON_NORM_DMA_TO_GPU 1

//When intercepted, GP1() commands are acted-upon and the their effects are emulated by this code, besides PS1DRV.
//This seems to be what the PGIF does, but disabling it (=0) doesn't break too much stuff either.
//Default=0. Do intercept when IOP code writes (sends) to FIFO the CMD.
#define CMD_INTERCEPT_AT_WR 0
//Default=1. Do intercept when PS1DRV reads from FIFO the CMD.
#define CMD_INTERCEPT_AT_RD 1

//PGIF_DAT_RB_LEAVE_FREE - How many elements of the FIFO buffer to leave free in DMA.
//Can be 0 and no faults are observed.
//As the buffer has 32 elements, and normal DMA reads are usually done in 4 qwords (16 words),
//this must be less than 16, otherwise the PS1DRV will never read from the FIFO.
//At one point (in Linked-List DMA), PS1DRV will expect at least a certain number of elements, that is sent as argument to the func.
#define PGIF_DAT_RB_LEAVE_FREE 1

//NOTES (TODO):
/*
- 8 and 16 bit access to the PGPU regs is not emulated... is it ever used? Emulating it would be tricky.


------

Much of the code is ("very") unoptimized, because it is a bit cleaner and more complete this way.

All the PS1 GPU info comes from psx-spx: http://problemkaputt.de/psx-spx.htm


*/

//Registers 0x00 - 0x40 are similar, in that they all serve as immediate-response registers to commands GP0(E1) - GP0(E5).
//PGPU_STAT 0x1000F300 The GP1 - Status register, whgich PS1DRV writes (emulates) for the IOP to read.
#define PGPU_STAT 0x1000F300
//PGIF_1-PGIF_4 - "immediate response registers" - hold the return values for commands that require immediate response.
//They correspond to GP0() E2-E5 commands.
#define PGIF_1 			0x1000F310
#define PGIF_2 			0x1000F320
#define PGIF_3 			0x1000F330
#define PGIF_4 			0x1000F340
//PGIF_CTRL 0x1000F380 Main register for PGIF status info & control.
#define PGIF_CTRL 		0x1000F380
//PGPU_CMD_FIFO FIFO buffer for GPU GP1 (CMD reg) CMDs IOP->EE only (unknown if reverse direction is possible).
#define PGPU_CMD_FIFO 		0x1000F3C0
//PGPU_DAT_FIFO FIFO buffer for GPU GP0 (DATA reg) IOP->EE, but also EE->IOP. Direction is controlled by reg. 0x80/bit4 (most likely).
//Official name is "GFIFO", according to PS1DRV.
#define PGPU_DAT_FIFO 		0x1000F3E0
//Registers 0x50 - 0x70, 0x90 - 0xB0, 0xD0, 0xF0 are not known to be used for anything.
/*the following three are from the homebrew PS2SDK, but their names are somewhat misleading:
#define PGIF_GPU_STAT (0x1000F300)
#define PGIF_CFIFO_STAT (0x1000F380)
#define PGIF_CFIFO_DATA (0x1000F3C0)*/

//Bit-fields definitions for the PGPU Status register:
#define PGPU_STAT_IRQ1 		0x01000000

//Specifies the bits in the PGIF Control (0xf380) register that are not writable from EE side, by the PS1DRV.
#define PGIF_CTRL_RO_MASK 	0x00000000

#define PGPU_DMA_MADR 		0x1F8010A0
#define PGPU_DMA_BCR 		0x1F8010A4
#define PGPU_DMA_CHCR 		0x1F8010A8
//is there a tadr?
#define PGPU_DMA_TADR 		0x1F8010AC

#define DMA_START_BUSY 		0x01000000
#define DMA_TRIGGER 		0x10000000
//write to peripheral
#define DMA_DIR_FROM_RAM 	0x1
#define DMA_LINKED_LIST 	0x00000400
#define DMA_LL_END_CODE 	0x00FFFFFF

//Local variables for emulation:

static u32 PGpuStatReg    = 0;  //Read-only to IOP
//Hold the values of the immediate-response registers.
static u32 pgif1reg       = 0;
static u32 pgif2reg       = 0;
static u32 pgif3reg       = 0;
static u32 pgif4reg       = 0;

static u32 PGifCtrlReg    = 0;  //PGifIfStat
static u32 PGifCtrlRegOld = 0;
static u32 PGpuDataReg    = 0;  //Read-only to IOP

static u32 lastGpuCmd     = 0;
static u32 lastGpuData    = 0;

#define pgpuDmaMadr 	HW_DMA2_MADR
#define pgpuDmaBcr 	HW_DMA2_BCR
#define pgpuDmaChcr 	HW_DMA2_CHCR
#define pgpuDmaTadr 	HW_DMA2_TADR

//Internal flags:
static int PgpuDmaLlAct      = 0;
static int PgpuDmaNrActToGpu = 0;
static int PgpuDmaNrActToIop = 0;

static u32 pgpuDmaTrAddr     = 0;
static u32 wCnt              = 0;    //
static u32 wordNr            = 0;  //total number of words
static u32 nextAddr          = 0;
static u32 dntPrt            = 0;

static u32 nrWordsN          = 0;  //total number of words in Normal DMA
static u32 crWordN           = 0;   //current word number in Normal DMA
static u32 addrNdma          = 0;

//Generic FIFO-related:
struct ringBuf_t
{
    u32 *buf;
    int size;
    int count;
    int head;
    int tail;
};

static void ringBufPut(struct ringBuf_t *rb, u32 *data)
{
    if (rb->count < rb->size)
    {
	    //there is available space
	    *(rb->buf + rb->head) = *data;
	    if ((++(rb->head)) >= rb->size)
		    rb->head = 0;  //wrap back when the end is reached
	    rb->count++;
    }
}

static void ringBufGet(struct ringBuf_t *rb, u32 *data)
{
    if (rb->count > 0)
    {  //there is available data
	    *data = *(rb->buf + rb->tail);
	    if ((++(rb->tail)) >= rb->size)
		    rb->tail = 0;  //wrap back when the end is reached
	    rb->count--;
    }
}

static void ringBufClear(struct ringBuf_t *rb)
{
    rb->head  = 0;
    rb->tail  = 0;
    rb->count = 0;
}

//Ring buffers definition and initialization:
//Command (GP1) FIFO, size= 0x8 words:
#define PGIF_CMD_RB_SZ 0x8
static struct ringBuf_t pgifCmdRbC;            //Ring buffer control variables.
static u32 pgiffCmdRbD[PGIF_CMD_RB_SZ] = {0};  //Ring buffer data.

//Data (GP0) FIFO - the so-called (in PS1DRV) "GFIFO", (real) size= 0x20 words:
//#define PGIF_DAT_RB_SZ 0x20
//FIXED: Values 0xD54 and higher cause PS logo to disappair.
//Using small (the real) FIFO size, disturbs MDEC video (and other stuff),
//because the MDEC does DMA instantly, while this emulation drains the FIFO,
//only when the PS1DRV gets data from it, which depends on IOP-EE sync, among other things.
//The reson it works in the real hardware, is that the MDEC DMA would run in the pauses of GPU DMA,
//thus the GPU DMA would never get data, MDEC hasn't yet written to RAM yet (too early).
#define PGIF_DAT_RB_SZ 0x20000
static struct ringBuf_t pgifDatRbC;            //Ring buffer control variables.
static u32 pgiffDatRbD[PGIF_DAT_RB_SZ] = {0};  //Ring buffer data.

//TODO: Make this be called by IopHw.cpp / psxHwReset()... but maybe it should be called by the EE reset func,
//given that the PGIF is in the EE ASIC, on the other side of the SBUS.
void pgifInit(void)
{
    pgifCmdRbC.buf = pgiffCmdRbD;
    pgifCmdRbC.size = PGIF_CMD_RB_SZ;
    ringBufClear(&pgifCmdRbC);
    pgifDatRbC.buf = pgiffDatRbD;
    pgifDatRbC.size = PGIF_DAT_RB_SZ;
    ringBufClear(&pgifDatRbC);

    PGpuStatReg = 0;
    pgif1reg = 0;
    pgif2reg = 0;
    pgif3reg = 0;
    pgif4reg = 0;
    PGifCtrlReg = 0;
    PGpuDataReg = 0;


    pgpuDmaMadr = 0;
    pgpuDmaBcr = 0;
    pgpuDmaChcr = 0;
    pgpuDmaTadr = 0;

    PgpuDmaLlAct = 0;
    PgpuDmaNrActToGpu = 0;
    PgpuDmaNrActToIop = 0;

    pgpuDmaTrAddr = 0;
    wCnt = 0;    //
    wordNr = 0;  //total number of words
    nextAddr = 0;

    nrWordsN = 0;  //total number of words in Normal DMA
    crWordN = 0;   //current word number in Normal DMA
    addrNdma = 0;

    return;
}

//Basically resetting the PS1 GPU.
static void pgifReset(void)
{
    ringBufClear(&pgifDatRbC);
    ringBufClear(&pgifCmdRbC);

    PGpuStatReg = 0;
    pgif1reg = 0;
    pgif2reg = 0;
    pgif3reg = 0;
    pgif4reg = 0;
    PGifCtrlReg = 0;
    PGpuDataReg = 0;
}

//------

//Interrupt-related (IOP, EE and DMA):

static void triggerPgifInt(int subCause)
{
    hwIntcIrq(15);
    cpuSetEvent();
    return;
}

static void getIrqCmd(u32 data) { }

static void ackGpuIrq(void)
{  //Acknowledge for the IOP GPU interrupt.
    //This is a "null"-function in PS1DRV... maybe because hardware takes care of it... but does any software use it?
    PGpuStatReg &= ~PGPU_STAT_IRQ1;
}

static void pgpuDmaIntr(int trigDma)
{  //For the IOP GPU DMA channel.
//trigDma: 1=normal,ToGPU; 2=normal,FromGPU, 3=LinkedList

// psxmode: 25.09.2016 at this point, the emulator works even when removing this interrupt call. how? why?
#if PREVENT_IRQ_ON_NORM_DMA_TO_GPU == 1
    if (trigDma != 1)  //Interrupt on ToGPU DMA breaks some games. TODO: Why?
#endif
        psxDmaInterrupt(2);
}

//Pass-through & intercepting functions:
static u32 immRespHndl(u32 cmd, u32 data)
{  //Handles the GP1(10h) command, that requires immediate responce.
    //The data argument is the old data of the register (shouldn't be critical what it contains).
    //Descriptions here are taken directly from psx-spx.
    switch ((cmd & 0xFF)) {
        case 0x00:
        case 0x01:
            break;  //Returns Nothing (old value in GPUREAD remains unchanged)
        case 0x02:
            data = pgif1reg & 0x001FFFFF;
            break;  //Read Texture Window setting  ;GP0(E2h) ;20bit/MSBs=Nothing
        case 0x03:
            data = pgif2reg & 0x001FFFFF;
            break;  //Read Draw area top left      ;GP0(E3h) ;20bit/MSBs=Nothing
        case 0x04:
            data = pgif3reg & 0x001FFFFF;
            break;  //Read Draw area bottom right  ;GP0(E4h) ;20bit/MSBs=Nothing
        case 0x05:
            data = pgif4reg & 0x003FFFFF;
            break;  //Read Draw offset             ;GP0(E5h) ;22bit
    }
    return data;
}

static void drainPgpuDmaLl(void)
{
    u32 data = 0;
    if (PgpuDmaLlAct == 0)
        return;  //process only on transfer active

    //Some games (Breath of Fire 3 US) set-up linked-list DMA, but don't immediatelly have the list correctly set-up,
    //so the result is that this function loops indefinitely, because of some links pointing back to themselves, forming a loop.
    //The solution is to only start DMA once the GP1(04h) - DMA Direction / Data Request command has been set to the value 0x2 (CPU->GPU DMA)
    //This func will be caled by this command as well.
    //if ((PGpuStatReg & 0x60000000) != 0x40000000) return;

    if (pgifDatRbC.count >= ((pgifDatRbC.size) - PGIF_DAT_RB_LEAVE_FREE))
        return;  //buffer full - needs to be drained first.

    if ((nextAddr == 0x000C8000) || (nextAddr == 0x0) || (nextAddr == 0x3))
        dntPrt++;

    if (wCnt >= wordNr) {  //Reached end of sequence, or the beginning of a new one
        //if ((nextAddr == DMA_LL_END_CODE) || (nextAddr == 0x00000000)) { //complete
        //why does BoF3 US get to a link pointing to address 0??
        if (nextAddr == DMA_LL_END_CODE) {
            dntPrt = 0;
            PgpuDmaLlAct = 0;
            pgpuDmaMadr = 0x00FFFFFF;
            //pgpuDmaMadr = nextAddr; //The END_CODE should go in the MADR reg.
            //But instead copy the whole header code, each time one is encuntered - makes a bit more sense.
            pgpuDmaChcr &= ~DMA_TRIGGER;     //DMA is no longer active (in transfer)
            pgpuDmaChcr &= ~DMA_START_BUSY;  //Transfer completed => clear busy flag
            pgpuDmaIntr(3);
        } else {
            data = iopMemRead32(nextAddr);
            pgpuDmaMadr = data;  //Copy the header word in MADR.
            //It is unknown whether the whole word should go here, but because this is a "Memory ADdress Register", leave only the address.
            pgpuDmaMadr &= 0x00FFFFFF;     // correct or not?
            pgpuDmaTrAddr = nextAddr + 4;  //start of data section of packet
            wCnt = 0;
            wordNr = (data >> 24) & 0xFF;  // Current length of packet and future address of header word.
            nextAddr = data & 0x00FFFFFF;
        }
    } else {
        //if (wCnt < wordNr) {
        data = iopMemRead32(pgpuDmaTrAddr);  //0; //psxHu32(pgpuDmaTrAddr); //Get the word of the packet from IOP RAM.
        ringBufPut(&pgifDatRbC, &data);
        pgpuDmaTrAddr += 4;
        wCnt++;
    }
}

static void drainPgpuDmaNrToGpu(void)
{
    u32 data = 0;

    if (PgpuDmaNrActToGpu == 0)
        return;  //process only on transfer active
    if (pgifDatRbC.count >= ((pgifDatRbC.size) - PGIF_DAT_RB_LEAVE_FREE))
        return;  //buffer full - needs to be drained first.

    if (crWordN < nrWordsN) {
        data = iopMemRead32(addrNdma);  //Get the word of the packet from IOP RAM.
        ringBufPut(&pgifDatRbC, &data);
        addrNdma += 4;
        crWordN++;
        //The following two are common to all DMA channels, and in future, some global hanler may be used... or not.
        //pgpuDmaMadr += 4; //It is unclear if this should be done exactly so...
        pgpuDmaMadr = 4 + pgpuDmaMadr;
        if ((crWordN % (pgpuDmaBcr & 0xFFFF)) == 0)  //Because this is actually the "next word number", it can be compared to the size of the block. //End of block reached.
            if (pgpuDmaBcr >= 0x10000)
                pgpuDmaBcr = pgpuDmaBcr - 0x10000;  //pgpuDmaBcr -= 0x10000;
                                                    //This is very inoptimized...
    }
    if (crWordN >= nrWordsN) {  //Reached end of sequence = complete
        PgpuDmaNrActToGpu = 0;
        pgpuDmaChcr &= ~DMA_TRIGGER;     //DMA is no longer active (in transfer)
        pgpuDmaChcr &= ~DMA_START_BUSY;  //Transfer completed => clear busy flag
        pgpuDmaIntr(1);
    }
}

static void drainPgpuDmaNrToIop(void)
{
    u32 data;
    if (PgpuDmaNrActToIop == 0)
        return;
    if (pgifDatRbC.count <= 0)
        return;
    /*	if (crWordN >= nrWordsN) {
		PgpuDmaNrActToIop = 0;
		pgpuDmaChcr &= ~DMA_TRIGGER; //DMA is no longer active (in transfer)
		pgpuDmaChcr &= ~DMA_START_BUSY; //Transfer completed => clear busy flag
		psxDmaInterrupt(2);
	} else { */
    if (crWordN < nrWordsN) {  //This is not the best way, but... is there another?
        ringBufGet(&pgifDatRbC, &data);
        iopMemWrite32(addrNdma, data);
        pgpuDmaMadr = 4 + pgpuDmaMadr;  //addrNdma += 4;
        crWordN++;
        //The following two are common to all DMA channels, and in future, some global hanler may be used... or not.
        pgpuDmaMadr += 4;                            //It is unclear if this should be done exactly so...
        if ((crWordN % (pgpuDmaBcr & 0xFFFF)) == 0)  //Because this is actually the "next word number", it can be compared to the size of the block. //End of block reached.
            if (pgpuDmaBcr >= 0x10000)
                pgpuDmaBcr = pgpuDmaBcr - 0x10000;  // pgpuDmaBcr -= 0x10000;
                                                    //This is very unoptimized... better use separate block counter.
    }
    if (crWordN >= nrWordsN) {
        PgpuDmaNrActToIop = 0;
        pgpuDmaChcr &= ~DMA_TRIGGER;     //DMA is no longer active (in transfer)
        pgpuDmaChcr &= ~DMA_START_BUSY;  //Transfer completed => clear busy flag
        pgpuDmaIntr(2);
    }

    if (pgifDatRbC.count > 0)
        drainPgpuDmaNrToIop();
}

static void ckhCmdSetPgif(u32 cmd)
{  //Check GP1() command and configure PGIF accordingly.
    //Maybe all below are hanled by PS1DRV and this functions is only wasting time and memory and cusing errors...but that doesn't seem to be the case.
    u32 cmdNr = ((cmd >> 24) & 0xFF) & 0x3F;  //Higher commands are known to mirror lower commands.
    u32 data  = 0x00000000;
    switch (cmdNr) {
        case 0x00:
            ringBufClear(&pgifDatRbC);
            ackGpuIrq();
            //	pgpu1reg = 0; pgpu2reg = 0; pgpu3reg = 0; pgpu4reg = 0;
            //More stuff to clear here....
            pgifReset();
            ringBufPut(&pgifDatRbC, &data);  //write a reset command for the PS1DRV to get.
            break;
        case 0x01:  //Reset Command Buffer (only?)... This would actually mean the Data (GP0()) register buffer.
            ringBufClear(&pgifDatRbC);
            break;
        case 0x02:  //Acknowledge GPU IRQ
            ackGpuIrq();
            break;
        case 0x04:  //DMA Direction / Data Request
            //The PS1DRV doesn't seem to take care of this...
            //Removing the following two, "damages" the PS logo, so they are probably necessary.
            //The PS1 program will not start DMA GPU->IOP (only) if this is missing.
            PGpuStatReg &= ~STAT_DIR_BY_CMD_BITS;               //TODO: Which bits should be controlled here?
            PGpuStatReg |= (cmd << 29) & STAT_DIR_BY_CMD_BITS;  //0x60000000; //0x20000000 bit 30 is handled by PS1DRV
            //0-1  DMA Direction (0=Off, 1=FIFO, 2=CPUtoGP0, 3=GPUREADtoCPU) ;GPUSTAT.29-30
            //2-23 Not used (zero)
            drainPgpuDmaLl();  //See comment in this function.
            break;
    }
}

static u32 getUpdPgpuStatReg(void)
{
    //The code below is not necessary! ... but not always...why?
    //Does PS1DRV does set bit 27 - reverse direction (VRAM->CPU) in some conditions... but not always...TODO: Find why.
    //The PS1 program pools this bit to determine if there is data in the FIFO, it can get. Then starts DMA to get it.
    //The PS1 program will not send the DMA direction command (GP1(04h)) and will not start DMA until this bit (27) becomes set.
    //Maybe move this to the PGifCtrlReg write handler...?
    PGpuStatReg &= ~0x08000000;  //VRAM->CPU data ready
    PGpuStatReg &= ~0x02000000;  //common "ready" flag
                                 //PGpuStatReg |= 0x10000000;  //GPU is ready to receive DMA data. PS1DRV seems to set and clear this bit,
                                 //but it is cleared in the next code anyway, because it seems safer.
                                 //PGpuStatReg |= 0x04000000;  //GPU is ready to receive GP0() cmds.
                                 //Setting this bit is best left to the PS1DRV (PS logo polygons start missing otherwise).
#if GPU_STAT_DIR_BY_IF_CTRL == 0
    if ((PGifCtrlReg & 0x10) || (PGpuStatReg & 0x20000000)) {  //Reverse direction bit GPU->CPU.
#else
    PGpuStatReg &= ~0x20000000;
    if (PGifCtrlReg & 0x10) {
        PGpuStatReg |= 0x20000000;
#endif
        //Or maybe bit 29 of the GPU STATUS reg, should copy the state of bit 4 of the PGIF_CTRL reg?
        PGpuStatReg &= ~0x04000000;     //Same with receiving GP0() commands.
        if (pgifDatRbC.count > 0)       //Data FIFO has data.
            PGpuStatReg |= 0x08000000;  //Could skip the second check, but that could cause buffer underflow on direct read.
                                        //Should the above be set only when there are at least 16 words of data?
    }

    switch ((PGpuStatReg >> 29) & 0x3) {
        case 0x00:
            break;  // When GP1(04h)=0 ---> Always zero (0)
        case 0x01:  // When GP1(04h)=1 ---> FIFO State  (0=Full, 1=Not Full)
            if (pgifDatRbC.count < (pgifDatRbC.size - 0))
                PGpuStatReg |= 0x02000000;  //Not (yet) full, so set the bit.
            break;
        case 0x02:                         // When GP1(04h)=2 ---> Same as GPUSTAT.28
            if (PGpuStatReg & 0x10000000)  //Ready to receive DMA Block  (0=No, 1=Ready)  ;GP0(...) ;via GP0
                PGpuStatReg |= 0x02000000;
            break;
        case 0x03:                         //When GP1(04h)=3 ---> Same as GPUSTAT.27
            if (PGpuStatReg & 0x08000000)  //Ready to send VRAM to CPU   (0=No, 1=Ready)  ;GP0(C0h) ;via GPUREAD
                PGpuStatReg |= 0x02000000;
            break;
    }
    return PGpuStatReg;
}

//This returns "correct" element-in-FIFO count, even if extremely large buffer is used.
static int getDatRbC_Count(void)
{
    return std::min(pgifDatRbC.count, 0x1F);
}

static void setPgifCtrlReg(u32 data)
{
    PGifCtrlReg = (data & (~PGIF_CTRL_RO_MASK)) | (PGifCtrlReg & PGIF_CTRL_RO_MASK);
}

static u32 getUpdPgifCtrlReg(void)
{
    PGifCtrlReg &= 0xFFFFE0FF;
    if (pgifCmdRbC.count == 0)  //fake that DATA FIFO is empty, so that PS1DRV processes any commands on the GP1() register first.
        PGifCtrlReg |= ((getDatRbC_Count() & 0x1F) << 8) & 0x1F00;
    PGifCtrlReg &= 0xFFF8FFFF;
    PGifCtrlReg |= ((pgifCmdRbC.count & 0x07) << 16) & 0x70000;
    PGifCtrlReg &= 0x7FFFFFFF;  //bit 31= (DMA?) transfer active. Has to be clear, for PS1DRV to continue. -
    //The above is most likely the 9-th bit of the FIFO counter. It is 0x20 in size most likly... though it could be 0x2F.
    //It doesn't look as it is (ever) used as an overflow flag. It is usually added to the main part of the counter as a 9-th bit
    //and then comaprisons to some number of elements are made.
    //Always fill FIFO (for drain by PS1DRV) at least 0x10 words - it will refuse to drain it.
    if (pgifCmdRbC.count == 0)  //fake that DATA FIFO is empty, so that PS1DRV processes any commands on the GP1() register first.
        PGifCtrlReg |= ((getDatRbC_Count() & 0x20) << 26) & 0x80000000;
    return PGifCtrlReg;
}

static void cmdRingBufGet(u32 *data)
{  //Used by PGIF/PS1DRV.
    ringBufGet(&pgifCmdRbC, data);
#if CMD_INTERCEPT_AT_RD == 1
    ckhCmdSetPgif(*data);  //Let's assume that the PGIF hardware processes GPU commands just when the EE reads them from its FIFO.
#endif
}

static void datRingBufGet(u32 *data)
{  //
    if (pgifDatRbC.count > 0) {
        ringBufGet(&pgifDatRbC, data);
        getIrqCmd(*data);  //Checks if an IRQ CMD passes through here a triggers IRQ when it does.
    } else {               //Return last value in register:
        *data = PGpuDataReg;
    }
}



//PS1 GPU registers I/O handlers:

void psxGPUw(int addr, u32 data)
{
    if (addr == HW_PS1_GPU_DATA) {
        lastGpuData = data;
        ringBufPut(&pgifDatRbC, &data);
    } else if (addr == HW_PS1_GPU_STATUS) {  //Command register GP1 (write-only).
        lastGpuCmd = data;
        if ((((data >> 24) & 0xFF) & 0x30) == 0x10) {  //Handle the immediate-response command.
            //Cmds 0x10 - 0x1F are all 0x10, and 0x40-0xFF mirror 0x00-0x3F.
            PGpuDataReg = immRespHndl(lastGpuCmd, PGpuDataReg);  //Would it be better if this is done when data is read at datRingBufGet()?
        } else {                                                 //Should the "immediate-response" command not be sent to the GPU (PS1DRV)..?
            triggerPgifInt(0);
            ringBufPut(&pgifCmdRbC, &data);
        }
#if CMD_INTERCEPT_AT_WR == 1
        ckhCmdSetPgif(data);
#endif
    }
}

u32 psxGPUr(int addr)
{
    u32 data = 0;
    if (addr == HW_PS1_GPU_DATA)
        datRingBufGet(&data);
    else if (addr == HW_PS1_GPU_STATUS)
        data = getUpdPgpuStatReg();
    return data;
}

//This function is used as a global FIFO-DMA-fill function and both Linked-list normal DMA call it,
//regardless which DMA mode runs.
static void fillFifoOnDrain(void)
{
    if ((getUpdPgifCtrlReg() & 0x8) == 0)
        return;  //Skip filing FIFO with elements, if PS1DRV hasn't set this bit.
    //Maybe it could be cleared once FIFO has data?
    //This bit could be an enabler for the DREQ (EE->IOP for SIF2=PGIF) line. The FIFO is filled only when this is set
    //and the IOP acknowledges it.
    drainPgpuDmaLl();
    drainPgpuDmaNrToGpu();
    //This is done here in a loop, rather than recursively in each function, because a very large buffer causes stack oveflow.
    while ((pgifDatRbC.count < ((pgifDatRbC.size) - PGIF_DAT_RB_LEAVE_FREE)) && ((PgpuDmaNrActToGpu != 0) || (PgpuDmaLlAct != 0))) {
        drainPgpuDmaLl();
        drainPgpuDmaNrToGpu();
    }
    //Flags' names: PgpuDmaLlAct is for linked-list, PgpuDmaNrActToGpu is for normal IOP->GPU
    if ((PgpuDmaLlAct == 1) || (PgpuDmaNrActToGpu == 1))
        if (PgpuDmaNrActToIop != 1)
            PGifCtrlReg &= ~0x8;
    //Clear bit as DMA will be run - normally it should be cleared only once the current request finishes, but the IOP won't notice anything anyway.
    //WARNING: Check if GPU->IOP DMA uses this flag, and if it does (it would make sense for it to use it as well),
    //then only clear it here if the mode is not GPU->IOP.
}


//PGIF registers I/O handlers:

void PGIFw(int addr, u32 data)
{
    if (addr == PGPU_DAT_FIFO) {  //reverse write - mind the direction set by reg f380
        ringBufPut(&pgifDatRbC, &data);
        drainPgpuDmaNrToIop();
    } else if (addr == PGPU_STAT) {
        PGpuStatReg = data;          //Should all bits be writable?
    } else if (addr == PGIF_CTRL) {  //PGIF config
        setPgifCtrlReg(data);
        PGifCtrlRegOld = PGifCtrlReg;  //data
        fillFifoOnDrain();             //Now this checks the 0x8 bit of the PGIF_CTRL reg, so  it here too,
                                       //so that it gets updated immediately once it is set.
    } else if (addr == PGIF_1) {
        pgif1reg = data;  //GP0(E2h)
    } else if (addr == PGIF_2) {
        pgif2reg = data;
    } else if (addr == PGIF_3) {
        pgif3reg = data;
    } else if (addr == PGIF_4) {
        pgif4reg = data;
    }
}

u32 PGIFr(int addr)
{
	u32 data = 0;
	if (addr == PGPU_CMD_FIFO)
		cmdRingBufGet(&data);
	else if (addr == PGPU_DAT_FIFO)
	{
		fillFifoOnDrain();
		datRingBufGet(&data);
	}
	else if (addr == PGPU_STAT)
		data = PGpuStatReg;
	else if (addr == PGIF_CTRL)  //PGIF config
		data = getUpdPgifCtrlReg();
	else if (addr == PGIF_1)
		data = pgif1reg;  //GP0(E2h)
	else if (addr == PGIF_2)
		data = pgif2reg;
	else if (addr == PGIF_3)
		data = pgif3reg;
	else if (addr == PGIF_4)
		data = pgif4reg;
	return data;
}

void PGIFrQword(u32 addr, void *dat)
{
    if (addr == PGPU_DAT_FIFO)
    {
	    u32 *data = (u32 *)dat;

	    fillFifoOnDrain();
	    datRingBufGet(data + 0);
	    datRingBufGet(data + 1);
	    datRingBufGet(data + 2);
	    datRingBufGet(data + 3);

	    fillFifoOnDrain();
    }
}

void PGIFwQword(u32 addr, void *dat)
{
    if (addr == PGPU_DAT_FIFO)
    {
	    u32 *data = (u32 *)dat;

	    ringBufPut(&pgifDatRbC, (u32 *)(data + 0));
	    ringBufPut(&pgifDatRbC, (u32 *)(data + 1));
	    ringBufPut(&pgifDatRbC, (u32 *)(data + 2));
	    ringBufPut(&pgifDatRbC, (u32 *)(data + 3));
	    drainPgpuDmaNrToIop();
    }
}


//DMA-emulating functions:

static void processPgpuDma(void)
{  //For normal mode & linked list.
    if ((pgpuDmaChcr & DMA_START_BUSY) == 0)
        return;                  //not really neessary in this case.
    pgpuDmaChcr |= DMA_TRIGGER;  //DMA is active (in transfer)
    if (pgpuDmaChcr & DMA_LINKED_LIST) {
        if (pgpuDmaChcr & DMA_DIR_FROM_RAM) {  //Linked-list only supported on direction to GPU.
            PgpuDmaLlAct = 1;
            //for the very start, the address of the first word should come for the address, stored at MADR
            nextAddr = (pgpuDmaMadr & 0x1FFFFFFF);  //The address in IOP RAM where to load the first header word from
            wCnt = 0;    //current word
            wordNr = 0;  //total number of words
            fillFifoOnDrain();  //drainPgpuDmaLl(); //fill a single word in fifo now, because otherwise PS1DRV won't know that there is aything in it and it wouldn't know that a transfer is pending.
            return;
        }
	else
            return;
    }
    crWordN = 0;
    addrNdma = pgpuDmaMadr & 0x1FFFFFFF;
    nrWordsN = ((pgpuDmaBcr & 0xFFFF) * ((pgpuDmaBcr >> 16) & 0xFFFF));

    if (pgpuDmaChcr & DMA_DIR_FROM_RAM) {  //to peripheral
        PgpuDmaNrActToGpu = 1;
        fillFifoOnDrain();  //drainPgpuDmaNrToGpu();
    } else {
        PgpuDmaNrActToIop = 1;
        drainPgpuDmaNrToIop();
    }
}

//DMA registers I/O:

u32 psxDma2GpuR(u32 addr)
{
    addr &= 0x1FFFFFFF;
    if (addr == PGPU_DMA_MADR)
        return pgpuDmaMadr;
    else if (addr == PGPU_DMA_BCR)
        return pgpuDmaBcr;
    else if (addr == PGPU_DMA_CHCR) {
        if ((PgpuDmaLlAct != 1) && (PgpuDmaNrActToGpu != 1) && (PgpuDmaNrActToIop != 1))
            pgpuDmaChcr &= ~0x01000000;
        return pgpuDmaChcr;
        //When using the memory for registers, rather than the local variables (see the definition of pgpuDmaChcr),
        //bit 0x01000000 gets set again, very soon after transfer ended(by the game) and then when the game pools it to detect dma completion,
        //it times-out. the above condition works around that... but that is an indicator that st. is wrong. -how could the game reset it so soon?
        //maybe it just writes to it twice...? or maybe it comes as a high 16-bit write... - todo: find the writer.
    } else if (addr == PGPU_DMA_TADR)
        return pgpuDmaTadr;
    return 0;
}

void psxDma2GpuW(u32 addr, u32 data)
{
    addr &= 0x1FFFFFFF;
    if (addr == PGPU_DMA_MADR) {
        pgpuDmaMadr = (data & 0x00FFFFFF);
    } else if (addr == PGPU_DMA_BCR) {
        pgpuDmaBcr = data;
    } else if (addr == PGPU_DMA_CHCR) {
        pgpuDmaChcr = data;
        if (pgpuDmaChcr & DMA_START_BUSY)
            processPgpuDma();
    }else if (addr == PGPU_DMA_TADR)
        pgpuDmaTadr = data;
}

#if 0
//#########################################################################################################
//Other debug & test code, not directly relatedto the PGIF:

//-------------------------
//debug print from IOP:

//BIOS POST
static void ps12PostOut(u32 mem, u8 value) { }
static void psDuartW(u32 mem, u8 value) { }

static u8 psExp2R8(u32 mem)
{
    if ((mem & 0x1FFFFFFF) == 0x1F802000)
        return 0x18;
    return 0xFF;
}

//0x80051080
static u32 getIntTmrKReg(u32 mem, u32 data)
{
    if (PGifCtrlReg != 0)
        return 0x7FFFFFFF;
    return data;
}

static void HPCoS_print(u32 mem, u32 data)
{
    int i;
    if ((mem & 0x1FFFFFFF) == 0x1103A0) {
        char str[201];
        for (i = 0; i < 200; i++) {
            str[i] = iopMemRead8(0x117328 + i);
            if (str[i] == 0)
                break;
        }
        str[i] = 0;
    }
}

static void dma6_OTClear(void)
{
    if ((psxHu(0x1f8010e8) & 0x01000000) == 0)
        return;
    u32 madr    = psxHu(0x1f8010e0);
    u32 blkSz   = psxHu(0x1f8010e4) & 0xFFFF;
    u32 addrPnt = madr;
    u32 pntCnt  = 0;

    while (pntCnt < blkSz) {  //Very unoptimized...
        iopMemWrite32(addrPnt, ((addrPnt - 4) & 0x00FFFFFF));
        addrPnt -= 4;
        pntCnt++;
    }
    iopMemWrite32((addrPnt + 4), 0x00FFFFFF);

    psxHu(0x1f8010e8) &= ~0x01000000;
    psxDmaInterrupt(6);
}

//NOTE: use iopPhysMem(madr) for getting the physical (local) address of IOP RAM stuff.  ... does this account for the complete 16MB range?
#endif
