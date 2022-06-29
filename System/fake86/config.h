//when CPU_V20 is defined, Fake86's CPU emulator acts like an 80186/V20.
//otherwise, it acts like a true 8086/8088
#define CPU_V20

//when compiled with network support, fake86 needs libpcap/winpcap.
//if it is disabled, the ethernet card is still emulated, but no actual
//communication is possible -- as if the ethernet cable was unplugged.
#define NETWORKING_OLDCARD //planning to support an NE2000 in the future

//when DISK_CONTROLLER_ATA is defined, fake86 will emulate a true IDE/ATA1 controller
//card. if it is disabled, emulated disk access is handled by directly intercepting
//calls to interrupt 13h.
//*WARNING* - the ATA controller is not currently complete. do not use!
//#define DISK_CONTROLLER_ATA

#define AUDIO_DEFAULT_SAMPLE_RATE 48000
#define AUDIO_DEFAULT_LATENCY 100

//#define DEBUG_BLASTER
//#define DEBUG_DMA

#define RO_MEM_SIZE        0x100000
#define RAM_MEM_SIZE       0x100000

#define VRAM_SIZE          256000

#define PORTS_MEM_SIZE     0x1000

#define TIMING_INTERVAL     7

#define  LCD_PIXEL_WIDTH    ((uint16_t)640)
#define  LCD_PIXEL_HEIGHT   ((uint16_t)480)
