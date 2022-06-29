
#include <stdio.h>

#include "SystemConfig.h"

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include "tusb.h"
#include "usbd.h"

#include "FTL_up.h"
#include "board_up.h"
#include "display_up.h"
#include "keyboard_up.h"
#include "llapi.h"
#include "llapi_code.h"
#include "mtd_up.h"
#include "vmMgr.h"

#include "../debug.h"

#include "stmp37xxNandConf.h"
#include "stmp_NandControlBlock.h"

#include "regs.h"
#include "regspower.h"

void vTaskTinyUSB(void *pvParameters);
void vMTDSvc(void *pvParameters);
void vFTLSvc(void *pvParameters);
void vKeysSvc(void *pvParameters);
void vVMMgrSvc(void *pvParameters);
void vLLAPISvc(void *pvParameters);
void vDispSvc(void *pvParameters);

TaskHandle_t pDispTask = NULL;
TaskHandle_t pSysTask = NULL;
TaskHandle_t pLLAPITask = NULL;
TaskHandle_t pLLIRQTask = NULL;
TaskHandle_t pLLIOTask = NULL;
TaskHandle_t pBattmon = NULL;

extern uint32_t g_mtd_write_cnt;
extern uint32_t g_mtd_read_cnt;
extern uint32_t g_mtd_erase_cnt;

uint32_t CurMount = 0;
uint32_t g_FTL_status = 10;
bool g_sysfault_auto_reboot = true;
uint32_t g_MSC_Configuration = MSC_CONF_OSLOADER_EDB;
extern volatile uint32_t g_latest_key_status;
volatile uint32_t g_vm_status = VM_STATUS_SUSPEND;
bool vm_needto_reset = false;

char pcWriteBuffer[4096 + 1024];
uint32_t HCLK_Freq;

extern uint32_t g_page_vram_fault_cnt;
extern uint32_t g_page_vrom_fault_cnt;

uint32_t check_frequency() {
    volatile uint32_t s0, s1;
    s0 = *((volatile uint32_t *)0x8001C020);
    vTaskDelay(pdMS_TO_TICKS(1000));
    s1 = *((volatile uint32_t *)0x8001C020);
    return (s1 - s0) / 1;
}
void printTaskList() {
    vTaskList((char *)&pcWriteBuffer);
    printf("=============================================\r\n");
    printf("任务名                 任务状态   优先级   剩余栈   任务序号\n");
    printf("%s\n", pcWriteBuffer);
    printf("任务名                运行计数           CPU使用率\n");
    vTaskGetRunTimeStats((char *)&pcWriteBuffer);
    printf("%s\n", pcWriteBuffer);
    printf("任务状态:  X-运行  R-就绪  B-阻塞  S-挂起  D-删除\n");
    printf("内存剩余:   %d Bytes\n", (unsigned int)xPortGetFreeHeapSize());
    printf("VRAM PageFault:   %ld \n", g_page_vram_fault_cnt);
    printf("VROM PageFault:   %ld \n", g_page_vrom_fault_cnt);
    printf("HCLK Freq:%ld MHz\n", HCLK_Freq / 1000000);
    printf("CPU Freq:%ld MHz\n", (HCLK_Freq / 1000000) * (*((uint32_t *)0x80040030) & 0x1F));
    printf("Flash IO_Writes:%lu\n", g_mtd_write_cnt);
    printf("Flash IO_Reads:%lu\n", g_mtd_read_cnt);
    printf("Flash IO_Erases:%lu\n", g_mtd_erase_cnt);
}

void vTask1(void *pvParameters) {
    // printf("Start vTask1\n");
    int c = 0;
    for (;;) {
        HCLK_Freq = check_frequency();
        c++;
        if (c == 10) {
            printTaskList();
            c = 0;
        }
    }
}

uint16_t tud_hid_get_report_cb(uint8_t report_id, hid_report_type_t report_type, uint8_t *buffer, uint16_t reqlen) {
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)reqlen;

    return 0;
}
void tud_hid_set_report_cb(uint8_t report_id, hid_report_type_t report_type, uint8_t const *buffer, uint16_t bufsize) {
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)bufsize;
}

extern bool g_vm_inited;
uint32_t *bootAddr;
void System(void *par) {

    bootAddr = (uint32_t *)VM_ROM_BASE;

    INFO("Booting...\n");
    DisplayClean();
    DisplayPutStr(0, 16 * 0, "System Booting...", 0, 255, 16);
    DisplayPutStr(0, 16 * 1, "Waiting for Flash GC...", 0, 255, 16);

    if ((*bootAddr != 0xEF5AE0EF) && *(bootAddr + 1) != 0xFECDAFDE) {
        DisplayClean();
        DisplayPutStr(0, 0, "Could not find the System!", 0, 255, 16);
        g_vm_status = VM_STATUS_SUSPEND;
        vTaskSuspend(NULL);
    }

    vTaskPrioritySet(pDispTask, configMAX_PRIORITIES - 5);

    DisplayPutStr(0, 16 * 2, "USB KeyBoard Emu Start.", 0, 255, 16);

#include "tusb_config.h"

    // key_register_notify(xTaskGetCurrentTaskHandle());

    uint32_t lastKey = 0;
    while (1) {
        uint8_t keycode[6] = {0};
        uint8_t modifier = 0;
        
        /*
        tud_hid_keyboard_report(REPORT_ID_KEYBOARD, modifier, keycode); //发送按键值给电脑
        vTaskDelay(pdMS_TO_TICKS(10));
        tud_hid_keyboard_report(REPORT_ID_KEYBOARD, modifier, NULL); //发送按键值给电脑
        */
        #define CHKEY(x,y) case x:keycode[0]=y;break

        if (lastKey != g_latest_key_status) {
            uint8_t curKey = g_latest_key_status & 0xFFFF;
            uint8_t kpress = g_latest_key_status >> 16;

            if(kpress == 0)
            {
                tud_hid_keyboard_report(REPORT_ID_KEYBOARD, modifier, NULL); 
                
            }else{
                switch (curKey)
                {
                CHKEY(KEY_VARS, HID_KEY_A);
                CHKEY(KEY_MATH, HID_KEY_B);
                CHKEY(KEY_ABC, HID_KEY_C);
                CHKEY(KEY_XTPHIN, HID_KEY_D);
                CHKEY(KEY_SIN, HID_KEY_E);
                CHKEY(KEY_COS, HID_KEY_F);
                CHKEY(KEY_TAN, HID_KEY_G);
                CHKEY(KEY_LN, HID_KEY_H);
                CHKEY(KEY_LOG, HID_KEY_I);
                CHKEY(KEY_X2, HID_KEY_J);
                CHKEY(KEY_XY, HID_KEY_K);
                CHKEY(KEY_LEFTBRACKET, HID_KEY_L);
                CHKEY(KEY_RIGHTBRACET, HID_KEY_M);
                CHKEY(KEY_DIVISION, HID_KEY_N);
                CHKEY(KEY_COMMA, HID_KEY_O);
                CHKEY(KEY_7, HID_KEY_P);
                CHKEY(KEY_8, HID_KEY_Q);
                CHKEY(KEY_9, HID_KEY_R);
                CHKEY(KEY_MULTIPLICATION, HID_KEY_S);
                CHKEY(KEY_4, HID_KEY_T);
                CHKEY(KEY_5, HID_KEY_U);
                CHKEY(KEY_6, HID_KEY_V);
                CHKEY(KEY_SUBTRACTION , HID_KEY_W);
                CHKEY(KEY_1, HID_KEY_X);
                CHKEY(KEY_2, HID_KEY_Y);
                CHKEY(KEY_3, HID_KEY_Z);
                CHKEY(KEY_PLUS, HID_KEY_SPACE);
                CHKEY(KEY_BACKSPACE, HID_KEY_BACKSPACE);
                CHKEY(KEY_ENTER, HID_KEY_RETURN);
                CHKEY(KEY_0, HID_KEY_APOSTROPHE);

                CHKEY(KEY_ALPHA, HID_KEY_CAPS_LOCK);
                CHKEY(KEY_SHIFT, HID_KEY_SHIFT_LEFT);
                CHKEY(KEY_ON, HID_KEY_GUI_LEFT);
                CHKEY(KEY_LEFT, HID_KEY_ARROW_LEFT);
                CHKEY(KEY_UP, HID_KEY_ARROW_UP);
                CHKEY(KEY_DOWN, HID_KEY_ARROW_DOWN);
                CHKEY(KEY_RIGHT, HID_KEY_ARROW_RIGHT);

                default:

                    break;
                }

                tud_hid_keyboard_report(REPORT_ID_KEYBOARD, modifier, keycode); 
            }


            // keys = ulTaskNotifyTake(pdFALSE, portMAX_DELAY);

            //INFO("k=%08lx\n", g_latest_key_status);
            lastKey = g_latest_key_status;
        }
        //vTaskDelay(pdMS_TO_TICKS(1));
    }

    vTaskSuspend(NULL);
    // vTaskSuspend(NULL);
    bootAddr += 2;
    g_vm_status = VM_STATUS_RUNNING;

    __asm volatile("mrs r1,cpsr_all");
    __asm volatile("bic r1,r1,#0x1f");
    __asm volatile("orr r1,r1,#0x10");
    __asm volatile("msr cpsr_all,r1");

    __asm volatile("mov r13,#0x02300000");
    __asm volatile("add r13,#0x000FA000");

    __asm volatile("ldr r0,=bootAddr");
    __asm volatile("ldr r0,[r0]");
    __asm volatile("mov pc,r0");

    while (1)
        ;
}

bool VMsavedIrq;
void VMSuspend() {
    if (g_vm_status == VM_STATUS_SUSPEND) {
        return;
    }
    if (pSysTask && pLLAPITask && pLLIRQTask) {

        vTaskSuspend(pLLIRQTask);
        vTaskSuspend(pLLAPITask);
        vTaskSuspend(pSysTask);
        VMsavedIrq = LLIRQ_enable(false);
        LLIRQ_ClearIRQs();

        g_vm_status = VM_STATUS_SUSPEND;
    }
}

void VMResume() {
    if (g_vm_status == VM_STATUS_RUNNING) {
        return;
    }
    if (pSysTask && pLLAPITask && pLLIRQTask) {
        LLIRQ_ClearIRQs();
        LLIRQ_enable(VMsavedIrq);
        vTaskResume(pLLIRQTask);
        vTaskResume(pLLAPITask);
        vTaskResume(pSysTask);

        g_vm_status = VM_STATUS_RUNNING;
    }
}

void VMCheckStatus() {

    uint32_t *pRegFram = (uint32_t *)(((uint32_t *)pSysTask)[1]);
    pRegFram -= 16;

    INFO("VM CPSR:%08lx\n", pRegFram[-1]);
    for (int i = 0; i < 16; i++) {
        INFO("VM REG[%d]:%08lx\n", i, pRegFram[i]);
    }
}

uint32_t sys_intStack;

extern bool g_llapi_fin;
void VMReset() {
    static bool in = false;

    if (pSysTask && pLLAPITask && pLLIRQTask && (in == false)) {
        in = true;

        while (eTaskStateGet(pSysTask) != eReady) {
            vTaskDelay(10);
        }

        vTaskSuspend(pSysTask);
        LLIRQ_enable(false);
        vTaskDelay(pdMS_TO_TICKS(100));

        vmMgr_ReleaseAllPage();

        taskENTER_CRITICAL();

        uint32_t *pRegFram = (uint32_t *)(((uint32_t *)pSysTask)[1]);
        pRegFram -= 16;

        pRegFram[-1] = 0x1F;
        pRegFram[13] = sys_intStack;
        pRegFram[15] = ((uint32_t)System) + 4;

        taskEXIT_CRITICAL();
        LLIRQ_ClearIRQs();
        vTaskDelay(pdMS_TO_TICKS(100));

        vTaskResume(pSysTask);
        vTaskResume(pLLIRQTask);
        vTaskResume(pLLAPITask);
        in = false;
    }
}

void VM_Unconscious(TaskHandle_t task, char *res, uint32_t address) {
    char buf[64];

    // if ((task == pSysTask) && g_sysfault_auto_reboot)
    {

        vTaskSuspend(pLLIRQTask);
        vTaskSuspend(pLLAPITask);
        LLIRQ_enable(false);

        uint32_t *pRegFram = (uint32_t *)(((uint32_t *)pSysTask)[1]);
        pRegFram -= 16;

        DisplayClean();

        DisplayPutStr(0, 16 * 0, "System Crash!", 0, 255, 16);
        if (res != NULL) {
            DisplayPutStr(14 * 8, 16 * 0, res, 0, 255, 16);
        }
        // DisplayPutStr(0, 16 * 1, "Press [ON]+[F5] Soft-reboot.", 0, 255, 16);

        memset(buf, 0, sizeof(buf));

        DisplayPutStr(0, 16 * 1, "[ON]>[F6] Reboot", 0, 255, 16);
        DisplayPutStr(0, 16 * 2, "[ON]>[0 ]>[F5] Clear ALL Data", 0, 255, 16);

        memset(buf, 0, sizeof(buf));
        sprintf(buf, "R12:%08lx R0:%08lx", pRegFram[12], pRegFram[0]);
        DisplayPutStr(0, 16 * 3, buf, 0, 255, 16);

        memset(buf, 0, sizeof(buf));
        sprintf(buf, "R13:%08lx  R1:%08lx", pRegFram[13], pRegFram[1]);
        DisplayPutStr(0, 16 * 4, buf, 0, 255, 16);

        memset(buf, 0, sizeof(buf));
        sprintf(buf, "R14:%08lx  R2:%08lx", pRegFram[14], pRegFram[2]);
        DisplayPutStr(0, 16 * 5, buf, 0, 255, 16);

        memset(buf, 0, sizeof(buf));
        sprintf(buf, "R15:%08lx  R3:%08lx", pRegFram[15], pRegFram[3]);
        DisplayPutStr(0, 16 * 6, buf, 0, 255, 16);

        memset(buf, 0, sizeof(buf));
        sprintf(buf, "CPSR:%08lx [FAR:%08lx]", pRegFram[-1], address);
        DisplayPutStr(0, 16 * 7, buf, 0, 255, 16);

        g_vm_status = VM_STATUS_UNCONSCIOUS;

        // portBoardReset();
    }
}

unsigned char blockChksum(char *block, unsigned int blockSize) {
    unsigned char sum = 0x5A;
    for (int i = 0; i < blockSize; i++) {
        sum += block[i];
    }
    return sum;
}

#define CDC_BINMODE_BUFSIZE 32768
bool transBinMode = false;
char *binBuf = NULL;
uint32_t cdcBlockCnt;

void MscSetCmd(char *cmd) {
}

void mkSTMPNandStructure(uint32_t OLStartBlock, uint32_t OLPages);
void parseCDCCommand(char *cmd) {
    if (strcmp(cmd, "PING") == 0) {

        printf("CDC PING\n");
        MscSetCmd("PONG\n");

        tud_cdc_write_str("PONG\n");
        tud_cdc_write_flush();
        return;
    }

    if (strcmp(cmd, "RESETDBUF") == 0) {
        if (binBuf == NULL) {
            binBuf = (char *)VMMGR_GetCacheAddress();
        }
        printf("REC DATA BUF.\n");
        memset(binBuf, 0xFF, CDC_BINMODE_BUFSIZE);
        MscSetCmd("READY\n");
        tud_cdc_write_str("READY\n");
        tud_cdc_write_flush();
        return;
    }

    if (strcmp(cmd, "BUFCHK") == 0) {
        uint8_t chk;
        if (binBuf == NULL) {
            binBuf = (char *)VMMGR_GetCacheAddress();
        }
        char res[16];

        chk = blockChksum(binBuf, CDC_BINMODE_BUFSIZE);
        // printf("CHKSUM:%02x\n", chk);
        sprintf(res, "CHKSUM:%02x\n", chk);
        MscSetCmd(res);
        tud_cdc_write_str(res);
        tud_cdc_write_flush();
        return;
    }

    if (memcmp(cmd, "ERASEB", 6) == 0) {
        uint32_t erase_blk = 30;
        sscanf(cmd, "ERASEB:%ld", &erase_blk);
        printf("ERASEB:%ld\n", erase_blk);

        MTD_ErasePhyBlock(erase_blk);

        MscSetCmd("EROK\n");
        tud_cdc_write_str("EROK\n");
        tud_cdc_write_flush();
        return;
    }

    if (memcmp(cmd, "PROGP", 5) == 0) {
        uint32_t prog_page = 1111;
        uint32_t wrMeta;
        uint8_t *mtbuff = NULL;
        sscanf(cmd, "PROGP:%ld,%ld", &prog_page, &wrMeta);
        printf("PROGP:%ld,%ld\n", prog_page, wrMeta);

        if (wrMeta) {
            mtbuff = pvPortMalloc(19);
            memset(mtbuff, 0xFF, 19);
            mtbuff[1] = 0x00;
            mtbuff[2] = 0x53; // S
            mtbuff[3] = 0x54; // T
            mtbuff[4] = 0x4D; // M
            mtbuff[5] = 0x50; // P
        }

        for (int i = 0; i < CDC_BINMODE_BUFSIZE / 2048; i++) {
            if (wrMeta) {
                MTD_WritePhyPageWithMeta(prog_page + i, 6, (uint8_t *)&binBuf[i * 2048], mtbuff);
            } else {
                MTD_WritePhyPage(prog_page + i, (uint8_t *)&binBuf[i * 2048]);
            }
        }

        if (wrMeta) {
            vPortFree(mtbuff);
        }

        MscSetCmd("PGOK\n");
        tud_cdc_write_str("PGOK\n");
        tud_cdc_write_flush();
        return;
    }

    if (memcmp(cmd, "MKNCB", 5) == 0) {
        uint32_t stblock, pages;
        sscanf(cmd, "MKNCB:%ld,%ld", &stblock, &pages);
        printf("MKNCB:%ld,%ld\n", stblock, pages);
        mkSTMPNandStructure(stblock, pages);
        MscSetCmd("MKOK\n");
        tud_cdc_write_str("MKOK\n");
        tud_cdc_write_flush();
        return;
    }

    if (strcmp(cmd, "FINOPA") == 0) {

        return;
    }

    if (strcmp(cmd, "VMSUSPEND") == 0) {
        vTaskSuspend(pBattmon);
        VMSuspend();
        vTaskDelay(pdMS_TO_TICKS(30));
        return;
    }

    if (strcmp(cmd, "VMRESUME") == 0) {
        vTaskResume(pBattmon);
        VMResume();
        vTaskDelay(pdMS_TO_TICKS(30));
        return;
    }

    if (strcmp(cmd, "VMRESET") == 0) {
        vm_needto_reset = true;
        // vTaskDelay(pdMS_TO_TICKS(30));
        VMReset();
        vTaskDelay(pdMS_TO_TICKS(30));
        return;
    }

    if (strcmp(cmd, "VMCHKS") == 0) {
        VMCheckStatus();
        return;
    }

    if (strcmp(cmd, "REBOOT") == 0) {
        portBoardReset();
        return;
    }

    if (strcmp(cmd, "ERASEALL") == 0) {
        MTD_EraseAllBLock();
        return;
    }

    if (strcmp(cmd, "MSCDATA") == 0) {
        tud_disconnect();
        DisplayClean();
        DisplayPutStr(0, 0, "USB MSC Mode.", 0, 255, 16);
        vTaskDelay(pdMS_TO_TICKS(200));
        g_MSC_Configuration = MSC_CONF_SYS_DATA;
        vTaskDelay(pdMS_TO_TICKS(10));
        tud_connect();
        return;
    }
}

uint32_t g_CDC_TransTo = 100;
// Invoked when cdc when line state changed e.g connected/disconnected
void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts) {
    (void)itf;
    (void)rts;
    cdc_line_coding_t c;

    // connected
    if (dtr) {
        // print initial message when connected
        // tud_cdc_write_str("\r\nTinyUSB CDC MSC device example\r\n");
        printf("CDC RESET\n");
        // tud_cdc_write_flush();
    }
    tud_cdc_get_line_coding(&c);

    switch (c.bit_rate) {
    case 14400:

        printf("CDC LOADER PATH\n");
        g_CDC_TransTo = CDC_PATH_LOADER;
        // if(tud_cdc_write_available())
        {
            tud_cdc_write_str("USB CDC-ACM OPEN.\n");
            // tud_cdc_write_flush();
        }
        break;
    case 115200:
        printf("CDC EDB PATH\n");
        g_CDC_TransTo = CDC_PATH_EDB;
        tud_cdc_write_flush();
        break;
    case 9600:
        printf("CDC SYS PATH\n");
        g_CDC_TransTo = CDC_PATH_SYS;
        break;
    default:
        break;
    }

    tud_cdc_read_flush();
    transBinMode = rts;
    if (transBinMode) {
        cdcBlockCnt = 0;
        printf("CDC BIN MODE\n");
    } else {
        printf("CDC TEXT MODE\n");
    }
}

char cdc_path_loader_buffer[64];
// Invoked when CDC interface received data from host
void tud_cdc_rx_cb(uint8_t itf) {
    (void)itf;
    int32_t c = 0;
    int32_t nRead;

    if (g_CDC_TransTo == CDC_PATH_SYS) {
        nRead = tud_cdc_available();
        if (nRead) {
            LLIO_NotifySerialRxAvailable();
        }
    }

    if (g_CDC_TransTo == CDC_PATH_LOADER) {
        nRead = tud_cdc_available();
        if (nRead < sizeof(cdc_path_loader_buffer)) {
            tud_cdc_read(cdc_path_loader_buffer, sizeof(cdc_path_loader_buffer));
            cdc_path_loader_buffer[nRead] = 0;
            printf("cmd:%s\n", cdc_path_loader_buffer);

            if (strcmp(cdc_path_loader_buffer, "getstatus") == 0) {
                printTaskList();
                goto fin;
            }

            if (strcmp(cdc_path_loader_buffer, "poweroff") == 0) {
                portBoardPowerOff();
                goto fin;
            }

            if (strcmp(cdc_path_loader_buffer, "clearall") == 0) {
                FTL_ClearAllSector();
                printf("clear all sector.\n");
                goto fin;
            }
        }

    fin:
        tud_cdc_read_flush();
    }

    if (g_CDC_TransTo == CDC_PATH_EDB) {
        if (transBinMode && binBuf) {
            if (cdcBlockCnt < CDC_BINMODE_BUFSIZE) {
                nRead = tud_cdc_available();
                // printf("nread:%d\n",nRead);
                tud_cdc_read(&binBuf[cdcBlockCnt], CDC_BINMODE_BUFSIZE);
                cdcBlockCnt += nRead;

            } else {
                tud_cdc_read_flush();
            }
        } else {
            char bufline[32];
            int bufcnt = 0;
            memset(bufline, 0, 32);
            nRead = tud_cdc_available();
            while (nRead--) {
                c = tud_cdc_read_char();
                if (c != -1) {
                    switch (c) {
                    case '\n':
                        bufline[bufcnt] = 0;
                        printf("CMD:%s\n", bufline);
                        parseCDCCommand(bufline);

                        bufcnt = 0;
                        break;

                    default:
                        if (bufcnt < 32) {
                            bufline[bufcnt++] = c;
                        }
                        break;
                    }
                }
            }
        }

        tud_cdc_read_flush();
    }
}

static void getKey(uint32_t *key, uint32_t *press) {
    *key = g_latest_key_status & 0xFFFF;
    *press = g_latest_key_status >> 16;
}

void vMainThread(void *pvParameters) {

    // vTaskDelay(pdMS_TO_TICKS(100));
    setHCLKDivider(2);
    setCPUDivider(1);

    // portLRADCEnable(1, 7);
    //  MTD_EraseAllBLock();
    //  while (g_FTL_status == 10) {
    //     vTaskDelay(pdMS_TO_TICKS(1000));
    // }

    printf("FTL Code:%ld\n", g_FTL_status);

    portLRADCConvCh(7, 1);

    g_CDC_TransTo = CDC_PATH_LOADER;
    // vTaskDelay(pdMS_TO_TICKS(1000));
    /*
    printf("Batt. voltage:%d mv, adc:%d\n", portGetBatterVoltage_mv(), portLRADCConvCh(7, 5));
    printf("VDDIO: %d mV\n", (int)(portLRADCConvCh(6, 5) * 0.9));
    printf("VDD5V: %d mV\n", (int)(portLRADCConvCh(5, 5) * 0.45 * 4));
    printf("Core Temp: %d ℃\n", (int)((portLRADCConvCh(4, 5) - portLRADCConvCh(3, 5)) * 1.012 / 4 - 273.15));
*/
    uint32_t key, key_press;

    for (;;) {
        getKey(&key, &key_press);
        vTaskDelay(pdMS_TO_TICKS(10));
        if ((key == KEY_ON) && key_press) {

        key_on_loop:
            vTaskDelay(pdMS_TO_TICKS(20));
            getKey(&key, &key_press);
            if ((key == KEY_F6) && key_press) {
                portBoardReset();
            }

            if ((key == KEY_0) && key_press) {
            key_zero_loop:
                vTaskDelay(pdMS_TO_TICKS(20));
                getKey(&key, &key_press);
                if ((key == KEY_F5) && key_press) {
                    VMSuspend();
                    DisplayClean();
                    DisplayPutStr(0, 0, "Erase All Data ...", 0, 255, 16);
                    vTaskDelay(pdMS_TO_TICKS(2000));
                    for (int i = FLASH_DATA_BLOCK; i < 1024; i++) {
                        MTD_ErasePhyBlock(i);
                    }
                    portBoardReset();
                } else {
                    goto key_zero_loop_exit;
                }
                goto key_zero_loop;
            key_zero_loop_exit:
                ((void)key);
            }

            extern uint32_t g_lcd_contrast;
            if ((key == KEY_SUBTRACTION) && key_press) {
                g_lcd_contrast--;
                portDispSetContrast(g_lcd_contrast);
                vTaskDelay(pdMS_TO_TICKS(40));
            }

            if ((key == KEY_PLUS) && key_press) {
                g_lcd_contrast++;
                portDispSetContrast(g_lcd_contrast);
                vTaskDelay(pdMS_TO_TICKS(40));
            }

            if ((key == KEY_ON) && !key_press) {
                goto key_on_release;
            }
            goto key_on_loop;
        }
    key_on_release:

        if ((key == KEY_SHIFT) && key_press) {
            vTaskDelay(pdMS_TO_TICKS(20));
            getKey(&key, &key_press);
            if ((key == KEY_BACKSPACE) && key_press) {
                portBoardPowerOff();
            }
        }

        if (vm_needto_reset) {
            vm_needto_reset = false;
            VMReset();
        }
    }
}

void get_cpu_info() {
    register uint32_t val;
    __asm volatile("mrc p15,0,%0,c0,c0,0"
                   : "=r"(val));
    printf("CPU ID:%08lx\n", val);

    __asm volatile("mrc p15,0,%0,c0,c0,1"
                   : "=r"(val));

    printf("ICache Size[%08lx]:%d KB\n", val, 1 << ((((val >> 0) >> 6) & 0xF) - 1));
    printf("DCache Size[%08lx]:%d KB\n", val, 1 << ((((val >> 12) >> 6) & 0xF) - 1));

    __asm volatile("mrc p15,0,%0,c0,c0,2"
                   : "=r"(val));
    printf("DTCM:%ld, ITCM:%ld\n", (val >> 16) & 1, val & 1);

    __asm volatile("mrc p15,0,%0,c9,c0,1"
                   : "=r"(val));
    printf("Cache LOCK:%08lx\n", val);
}

void vBatteryMon(void *__n) {

    uint32_t vatt_adc = 0;
    uint32_t batt_voltage = 0;
    uint32_t vdd5v_voltage = 0;
    int coreTemp = 0;
    long t = 0;
    int n = 0;

    uint32_t show_bat_val;

    for (;;) {

        vatt_adc = portLRADCConvCh(7, 5);
        batt_voltage = portGetBatterVoltage_mv();
        vdd5v_voltage = (int)(portLRADCConvCh(5, 5) * 0.45 * 4);
        coreTemp = (int)((portLRADCConvCh(4, 5) - portLRADCConvCh(3, 5)) * 1.012 / 4 - 273.15);

        if (t % 10 == 0) {

            if (portGetBatteryMode() == 0) {
                printf("Battery = Li-ion\n");
            } else {
                printf("Battery = Single AA or AAA\n");
            }
            printf("Batt. voltage:%ld mv, adc:%ld\n", batt_voltage, vatt_adc);
            printf("VDDIO: %d mV\n", (int)(portLRADCConvCh(6, 5) * 0.9));
            printf("VDD5V: %ld mV\n", vdd5v_voltage);
            printf("VBG: %d mV\n", (int)(portLRADCConvCh(2, 5) * 0.45));
            printf("Core Temp: %d ℃\n", coreTemp);
            printf("Power Speed:%02lx\n", portGetPWRSpeed());
        }
        t++;

        if (vdd5v_voltage > 3500) {
        }
        show_bat_val = batt_voltage;
        if (show_bat_val > 1500) {
            show_bat_val = 1500;
        }
        if (show_bat_val < 800) {
            show_bat_val = 800;
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
        n = 0;
        if (((show_bat_val - 800) * 100 / (1500 - 800)) >= ((100 / 4) * 1))
            n |= (1 << 0);
        if (((show_bat_val - 800) * 100 / (1500 - 800)) >= ((100 / 4) * 2))
            n |= (1 << 1);
        if (((show_bat_val - 800) * 100 / (1500 - 800)) >= ((100 / 4) * 3))
            n |= (1 << 2);
        if (((show_bat_val - 800) * 100 / (1500 - 800)) >= ((100 / 4) * 4))
            n |= (1 << 3);
        DisplaySetIndicate(-1, n);
    }
}

extern uint32_t log_i;
extern uint32_t log_j;
extern char log_buf[SYS_LOG_BUFSIZE];

void TaskUSBLog(void *_) {

    vTaskDelay(pdMS_TO_TICKS(2000));
    for (;;) {

        int ava;
        if (g_CDC_TransTo == CDC_PATH_LOADER) {
            ava = tud_cdc_write_available();
            if (ava > 0) {
            retest:
                if (log_j < log_i) {
                    if (log_i - log_j <= ava) {
                        tud_cdc_write(&log_buf[log_j], log_i - log_j);
                        tud_cdc_write_flush();
                        log_j = log_i;
                    } else {
                        tud_cdc_write(&log_buf[log_j], ava);
                        tud_cdc_write_flush();
                        log_j += ava;
                    }

                } else if (log_j > log_i) {
                    if (SYS_LOG_BUFSIZE - log_j <= ava) {
                        tud_cdc_write(&log_buf[log_j], SYS_LOG_BUFSIZE - log_j + 1);
                        tud_cdc_write_flush();
                        log_j = 0;
                    } else {
                        tud_cdc_write(&log_buf[log_j], ava);
                        tud_cdc_write_flush();
                        log_j += ava;
                    }

                    vTaskDelay(pdMS_TO_TICKS(100));
                    goto retest;
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

extern int bootTimes;
void _startup() {

    printf("Starting.(rebootTimes: %d)\n", bootTimes);

    get_cpu_info();

    boardInit();
    printf("booting .....\n");
    xTaskCreate(vTask1, "Status Print", configMINIMAL_STACK_SIZE, NULL, configMAX_PRIORITIES - 1, NULL);

    xTaskCreate(vMTDSvc, "MTD Svc", configMINIMAL_STACK_SIZE, NULL, configMAX_PRIORITIES - 2, NULL);
    xTaskCreate(vFTLSvc, "FTL Svc", configMINIMAL_STACK_SIZE, NULL, configMAX_PRIORITIES - 3, NULL);
    xTaskCreate(vDispSvc, "Display Svc", configMINIMAL_STACK_SIZE, NULL, configMAX_PRIORITIES - 1, &pDispTask);
    xTaskCreate(TaskUSBLog, "USB Log", configMINIMAL_STACK_SIZE, NULL, configMAX_PRIORITIES - 3, NULL);
    xTaskCreate(vTaskTinyUSB, "TinyUSB", configMINIMAL_STACK_SIZE, NULL, configMAX_PRIORITIES - 4, NULL);
    xTaskCreate(vVMMgrSvc, "VM Svc", configMINIMAL_STACK_SIZE, NULL, configMAX_PRIORITIES - 4, NULL);

    xTaskCreate(vKeysSvc, "Keys Svc", configMINIMAL_STACK_SIZE, NULL, configMAX_PRIORITIES - 1, NULL);

    xTaskCreate(vLLAPISvc, "LLAPI Svc", configMINIMAL_STACK_SIZE, NULL, configMAX_PRIORITIES - 5, &pLLAPITask);
    xTaskCreate(LLIRQ_task, "LLIRQ Svc", configMINIMAL_STACK_SIZE, NULL, configMAX_PRIORITIES - 6, &pLLIRQTask);
    xTaskCreate(LLIO_ScanTask, "LLIO Svc", configMINIMAL_STACK_SIZE, NULL, configMAX_PRIORITIES - 6, &pLLIOTask);

    xTaskCreate(System, "System", configMINIMAL_STACK_SIZE, NULL, configMAX_PRIORITIES - 7, &pSysTask);

    xTaskCreate(vBatteryMon, "Battery Mon", configMINIMAL_STACK_SIZE, NULL, configMAX_PRIORITIES - 1, &pBattmon);
    xTaskCreate(vMainThread, "Main Thread", configMINIMAL_STACK_SIZE, NULL, configMAX_PRIORITIES - 1, NULL);

    sys_intStack = (uint32_t)(((uint32_t *)pSysTask)[0]);
    // pSysTask = xTaskCreateStatic( (TaskFunction_t)0x00100000, "System", VM_RAM_SIZE, NULL, 1, VM_RAM_BASE, pvPortMalloc(sizeof(StaticTask_t)));

    // vTaskSuspend(pSysTask);
    // vTaskSuspend(pLLAPITask);

    vTaskStartScheduler();
    printf("booting fail.\n");

    while (1)
        ;
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    PANNIC("StackOverflowHook:%s\n", pcTaskName);
}

void vAssertCalled(char *file, int line) {
    PANNIC("ASSERT %s:%d\n", file, line);
}

void vApplicationGetTimerTaskMemory(StaticTask_t **ppxTimerTaskTCBBuffer,
                                    StackType_t **ppxTimerTaskStackBuffer,
                                    uint32_t *pulTimerTaskStackSize) {
    *ppxTimerTaskTCBBuffer = (StaticTask_t *)pvPortMalloc(sizeof(StaticTask_t));
    *ppxTimerTaskStackBuffer = (StackType_t *)pvPortMalloc(configMINIMAL_STACK_SIZE * 4);
    *pulTimerTaskStackSize = configMINIMAL_STACK_SIZE;
}

void vApplicationMallocFailedHook() {
    PANNIC("ASSERT: Out of Memory.\n");
}