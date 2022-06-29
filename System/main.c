

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <math.h>

#include "keyboard.h"
#include "llapi_code.h"
#include "sys_llapi.h"

#include "SysConf.h"

#include "FreeRTOS.h"
#include "task.h"

#include "ff.h"

#include "lv_conf.h"
#define LV_CONF_INCLUDE_SIMPLE
#include "lvgl.h"

#include "debug.h"

#include "SystemFs.h"
#include "SystemUI.h"

#include "lv_demo_keypad_encoder.h"

#include "fake86/fake86_main.h"
#include "fake86/render.h"

volatile unsigned long ulHighFrequencyTimerTicks;

char pcWriteBuffer[4096];
void printTaskList() {
    vTaskList((char *)&pcWriteBuffer);
    printf("=============SYSTEM STATUS=================\r\n");
    printf("Task Name         Task Status   Priority   Stack   ID\n");
    printf("%s\n", pcWriteBuffer);
    printf("Task Name                Running Count         CPU %%\n");
    vTaskGetRunTimeStats((char *)&pcWriteBuffer);
    printf("%s\n", pcWriteBuffer);
    printf("Status:  X-Running  R-Ready  B-Block  S-Suspend  D-Delete\n");
    printf("Free memory:   %d Bytes\n", (unsigned int)xPortGetFreeHeapSize());
}

void vTask1(void *par1) {
    while (1) {
        printTaskList();
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void softDelayMs(uint32_t ms) {
    uint32_t cur = ll_get_time_ms();
    while ((ll_get_time_ms() - cur) < ms) {
        ;
    }
}

void vTask2(void *par1) {

    uint32_t ticks = 0;
    while (1) {
        /*
                static double f1 = 0.1;
                static double f2 = 0.2;
                f1 += 0.01;
                f1 /= 1.001;
                f2 = f2 + f1;
                printf("test:%d,%d\n", (int)(f1 * 10), (int)(f2 * 10));

        */
        printf("SYS Run Time: %d s\n", ticks);
        ticks++;

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

volatile uint32_t ms_tick = 0;

/*
void vTask3(void *par1)
{
    for(;;)
    {

        ms_tick += 100;
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
*/

#define LCD_PIXEL_WIDTH ((uint16_t)640)
#define LCD_PIXEL_HEIGHT ((uint16_t)480)
extern uint32_t prestretch[LCD_PIXEL_HEIGHT][LCD_PIXEL_WIDTH];

uint8_t vrambuf[256 * 128];
//[256][127];

void main_thread() {

    SystemUIInit();
    SystemFSInit();
    INFO("Run main_thread\n");
    SystemUISuspend();


    
    ms_tick = ll_get_time_ms();

    fake86_main_init();

    // lv_demo_benchmark();
    //  lv_demo_stress();
    //  lv_demo_music();
    //  lv_demo_widgets();

    // SystemUIMsgBox("测试?", "Unicode测试", SYSTEMUI_MSGBOX_BUTTON_CANCAL);
    // SystemUIMsgBox("测试?", "Unicode测试", SYSTEMUI_MSGBOX_BUTTON_CANCAL);

    // lv_obj_t *obj;
    // lv_obj_t *win;
    // win = lv_win_create(lv_scr_act(), 20);
    // lv_win_add_title(win, "测试窗口");
    // lv_obj_t * cont = lv_win_get_content(win);

    /*
        obj = lv_textarea_create(lv_scr_act());
        lv_textarea_add_text(obj, "字体：思源黑体 Light\n 字号：11\n");
        lv_textarea_add_text(obj, te);
        lv_textarea_set_align(obj, LV_TEXT_ALIGN_LEFT);
        lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLL_WITH_ARROW);
    */

    // char* argv[] = {"gb", "test.gb", NULL};
    // int argc = sizeof(argv) / sizeof(argv[0]) - 1;
    // extern int gb_main(int argc, char *argv[]);
    // gb_main(argc, argv);

    // SystemTest();

    // lv_scr_act();

    // lv_demo_keypad_encoder();

    volatile extern uint8_t updatedscreen;
    INFO("Runnung.\n");

    for (;;) {
        ms_tick = ll_get_time_ms();

        fake86_run_task();
        VideoThread();
        //INFO("CycFin\n");
        // vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void emu_draw() {
    uint32_t c;
    for (int y = 0; y < 127; y++) {
        for (int x = 0; x < 256; x++) {
            c = prestretch[(int)(y * (256.0 / 127))][(int)(x * (320.0 / 256))];

            vrambuf[256 * y + x] = (c);
        }
    }
    ll_disp_put_area(vrambuf, 0, 0, 255, 126);
    
        INFO("Draw %08x\n",c);
}

void main() {

    // SYS STACK      0x023FA000
    // IRQ STACK      0x023FFFF0

    printf("System Booting...\n");

    void IRQ_ISR();
    void SWI_ISR();
    ll_set_irq_stack(IRQ_STACK_ADDR);
    ll_set_irq_vector(((uint32_t)IRQ_ISR) + 4);
    ll_set_svc_stack(SWI_STACK_ADDR);
    ll_set_svc_vector(((uint32_t)SWI_ISR) + 4);
    ll_enable_irq(false);
    // ll_set_keyboard(true);

    xTaskCreate(vTask1, "Task1", configMINIMAL_STACK_SIZE, NULL, configMAX_PRIORITIES - 1, NULL);
    xTaskCreate(vTask2, "Task2", configMINIMAL_STACK_SIZE, NULL, configMAX_PRIORITIES - 1, NULL);
    // xTaskCreate(vTask3, "mstick", configMINIMAL_STACK_SIZE, NULL, configMAX_PRIORITIES - 1, NULL);

    xTaskCreate(main_thread, "System", configMINIMAL_STACK_SIZE, NULL, configMAX_PRIORITIES - 3, NULL);

    vTaskStartScheduler();

    for (;;) {
        *((uint32_t *)0x45678901) = 114514 + 1919810;
    }
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    PANNIC("StackOverflowHook:%s\n", pcTaskName);
}

void vAssertCalled(char *file, int line) {
    PANNIC("ASSERT FAILED AT %s:%d\n", file, line);
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
