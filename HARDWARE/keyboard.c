#include "keyboard.h"
#include "FreeRTOS.h"
#include "task.h"
#include "sys.h"

/*
 * 4×4 矩阵键盘
 *
 * 行线作为推挽输出，空闲时保持高电平：
 *   第1行：PD6    第2行：PD7    第3行：PC6    第4行：PC8
 *
 * 列线作为上拉输入，检测到低电平表示按键按下：
 *   第1列：PC11   第2列：PE5    第3列：PA6    第4列：PC7
 *
 * 键值排列：
 *         PC11  PE5   PA6   PC7
 *   PD6     1     2     3     A
 *   PD7     4     5     6     B
 *   PC6     7     8     9     C
 *   PC8     *     0     #     D
 *
 * 无按键时返回字符 'N'。
 */

static GPIO_InitTypeDef GPIO_InitStructure;

/*
 * 初始化矩阵键盘使用的 GPIO。
 */
void key_board_init(void)
{
    /* 使能按键引脚所在 GPIO 端口的时钟。 */
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE, ENABLE);

    /* 配置四条行线为推挽输出。 */
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_High_Speed;
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_NOPULL;

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7;
    GPIO_Init(GPIOD, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_8;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    /* 空闲时四条行线全部保持高电平。 */
    GPIO_SetBits(GPIOD, GPIO_Pin_6 | GPIO_Pin_7);
    GPIO_SetBits(GPIOC, GPIO_Pin_6 | GPIO_Pin_8);

    /* 配置四条列线为上拉输入。 */
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5;
    GPIO_Init(GPIOE, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_7;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11;
    GPIO_Init(GPIOC, &GPIO_InitStructure);
}

/*
 * 逐行扫描键盘。
 * 每次只拉低一条行线，等待电平稳定后读取四条列线。
 */
char get_key_board(void)
{
    char result;

    /* 扫描第1行：PD6 拉低。 */
    PDout(6) = 0;
    PDout(7) = 1;
    PCout(6) = 1;
    PCout(8) = 1;
    vTaskDelay(pdMS_TO_TICKS(2));

    result = 'N';
    if      (PCin(11) == 0) result = '1';
    else if (PEin(5)  == 0) result = '2';
    else if (PAin(6)  == 0) result = '3';
    else if (PCin(7)  == 0) result = 'A';

    PDout(6) = 1;
    if (result != 'N') return result;

    /* 扫描第2行：PD7 拉低。 */
    PDout(6) = 1;
    PDout(7) = 0;
    PCout(6) = 1;
    PCout(8) = 1;
    vTaskDelay(pdMS_TO_TICKS(2));

    result = 'N';
    if      (PCin(11) == 0) result = '4';
    else if (PEin(5)  == 0) result = '5';
    else if (PAin(6)  == 0) result = '6';
    else if (PCin(7)  == 0) result = 'B';

    PDout(7) = 1;
    if (result != 'N') return result;

    /* 扫描第3行：PC6 拉低。 */
    PDout(6) = 1;
    PDout(7) = 1;
    PCout(6) = 0;
    PCout(8) = 1;
    vTaskDelay(pdMS_TO_TICKS(2));

    result = 'N';
    if      (PCin(11) == 0) result = '7';
    else if (PEin(5)  == 0) result = '8';
    else if (PAin(6)  == 0) result = '9';
    else if (PCin(7)  == 0) result = 'C';

    PCout(6) = 1;
    if (result != 'N') return result;

    /* 扫描第4行：PC8 拉低。 */
    PDout(6) = 1;
    PDout(7) = 1;
    PCout(6) = 1;
    PCout(8) = 0;
    vTaskDelay(pdMS_TO_TICKS(2));

    result = 'N';
    if      (PCin(11) == 0) result = '*';
    else if (PEin(5)  == 0) result = '0';
    else if (PAin(6)  == 0) result = '#';
    else if (PCin(7)  == 0) result = 'D';

    PCout(8) = 1;

    return result;
}
