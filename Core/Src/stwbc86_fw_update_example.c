/**
  ******************************************************************************
  * @file        stwbc86_fw_update_example.c
  * @brief       The file provide an example to perform FW update on STWBC86 using
  *             stwbc86-pid.
  *
  ******************************************************************************
  * @attention
  *
  * This file is part of stwbc86-pid.
  *
  * Copyright (c) 2023, STMicroelectronics - All Rights Reserved
  * Author(s): ACD (Analog Custom Devices) Software Team for STMicroelectronics.
  *
  * License terms: BSD 3-clause "New" or "Revised" License.
  *
  * Redistribution and use in source and binary forms, with or without
  * modification, are permitted provided that the following conditions are met:
  *
  * 1. Redistributions of source code must retain the above copyright notice, this
  * list of conditions and the following disclaimer.
  *
  * 2. Redistributions in binary form must reproduce the above copyright notice,
  * this list of conditions and the following disclaimer in the documentation
  * and/or other materials provided with the distribution.
  *
  * 3. Neither the name of the copyright holder nor the names of its contributors
  * may be used to endorse or promote products derived from this software
  * without specific prior written permission.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
  * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************
  */

/**
  * This example is developed using following STMicroelectronics evaluation boards:
  *
  * - NUCLEO-H503RB
  * - NUCLEO-H563ZI
  *
  *
  * If you need to run this example on a different hardware platform a modification
  * of the structure and functions: `stmdev_platform`, `platform_write`,
  * `platform_write_read`, `platform_delay`, `platform_free_mem` and 'platform_log'
  * is required
  * and remove `HAL_I2C_MasterTxCpltCallback` and `HAL_I2C_MasterRxCpltCallback`.
  *
  *******************************************************************************
  *
  * IOC configurations need to use example below:
  *
  * Peripherals settings:
  * 1. USART3 - For console printing purpose.
  * 2. I2C1
  *     - In NVIC settings, enable I2C1 Event and Error Interrupt.
  *
  * Generated files settings:
  * 1. Open .ioc files, navigate to `Project Manager` and click `Code Generator` tab.
  * 2. In `Generate Files` column, check the box of `Generate peripheral
  *    initialization as pair of ".c/.h" files per peripheral`.
  * 3. Save .ioc and generate code.
  *
 */

/* Includes ------------------------------------------------------------------*/
#include "i2c.h"
#include "usart.h"

#include <stdlib.h>
#include "stwbc86.h"

/* Private define ------------------------------------------------------------*/
#define USE_STATIC_ALLOC_RW 0

#if USE_STATIC_ALLOC_RW
#define MEMORY_BLOCKS_NUM    3
struct memory_block {
    uint8_t buf[8192];
    uint8_t is_allocated;
};
static struct memory_block stwlc_alloc_buf[MEMORY_BLOCKS_NUM] = { { 0 } };
#endif

struct stmdev_platform {
    I2C_HandleTypeDef *hi2c;
    UART_HandleTypeDef *huart;
};

/* Private function prototypes -----------------------------------------------*/
int32_t platform_write(void *phandle, uint8_t *wbuf, int32_t wlen);
int32_t platform_write_read(void *phandle, uint8_t *wbuf, int32_t wlen, uint8_t *rbuf, int32_t rlen);
void platform_delay(uint32_t millisec);
void *platform_alloc_mem(size_t size);
void platform_free_mem(void *ptr);

/* Private variables ---------------------------------------------------------*/
static uint8_t i2cSequentialRxDone = 0;
static uint8_t i2cSequentialTxDone = 0;

/**
  * @brief  Master Tx Transfer completed callback.
  * @param  hi2c Pointer to a I2C_HandleTypeDef structure that contains
  *              the configuration information for the specified I2C.
  * @retval None
  */
/* Private user code ---------------------------------------------------------*/
void HAL_I2C_MasterTxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    i2cSequentialTxDone = 1;
}

/**
  * @brief  Master Rx Transfer completed callback.
  * @param  hi2c Pointer to a I2C_HandleTypeDef structure that contains
  *              the configuration information for the specified I2C.
  * @retval None
  */

void HAL_I2C_MasterRxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    i2cSequentialRxDone = 1;
}

/*
 * @brief  platform I2C write (platform dependent)
 * @param  phandle      Pointer to platform handle.
 * @param  wbuf         Pointer to bytes to be written into STWBC86
 * @param  wlen         Size of bytes to be written
 *
 * @retval error status (MANDATORY: return 0 -> no Error)
 */

int32_t platform_write(void *phandle, uint8_t *wbuf, int32_t wlen)
{
    struct stmdev_platform *platform = (struct stmdev_platform *)phandle;
    HAL_StatusTypeDef status = HAL_OK;
    uint32_t startTick;

    i2cSequentialTxDone = 0;
    startTick = HAL_GetTick();

    status = HAL_I2C_Master_Seq_Transmit_IT(platform->hi2c, STWBC86_I2C_ADDR << 1, wbuf, wlen, I2C_FIRST_AND_LAST_FRAME);
    if(status != HAL_OK)
        return status;

    while(i2cSequentialTxDone == 0)
    {
        if((HAL_GetTick() - startTick) > 1000)
        {
            /* I2C NACK WORKAROUND */
            /* Process Unlocked */
            __HAL_LOCK(platform->hi2c);

            /* Clear STOP Flag */
            __HAL_I2C_CLEAR_FLAG(platform->hi2c, I2C_FLAG_STOPF);

            /* Clear Configuration Register 2 */
            I2C_RESET_CR2(platform->hi2c);

            platform->hi2c->State = HAL_I2C_STATE_READY;
            platform->hi2c->Mode  = HAL_I2C_MODE_NONE;

            /* Process Unlocked */
            __HAL_UNLOCK(platform->hi2c);

            return 0;
        }
    }

    return 0;
}

/*
 * @brief  platform I2C write (platform dependent)
 * @param  phandle      Pointer to platform handle.
 * @param  wbuf         Pointer to bytes to be written into STWBC86
 * @param  wlen         Size of bytes to be written
 * @param  rbuf         Pointer to hold the bytes read from STWBC86
 * @param  rlen         Size of bytes to be read
 *
 * @retval error status (MANDATORY: return 0 -> no Error)
 */
int32_t platform_write_read(void *phandle, uint8_t *wbuf, int32_t wlen, uint8_t *rbuf, int32_t rlen)
{
    struct stmdev_platform *platform = (struct stmdev_platform *)phandle;
    HAL_StatusTypeDef status = HAL_OK;
    uint32_t startTick;

    i2cSequentialTxDone = 0;
    i2cSequentialRxDone = 0;
    startTick = HAL_GetTick();

    status = HAL_I2C_Master_Seq_Transmit_IT(platform->hi2c, STWBC86_I2C_ADDR << 1, wbuf, wlen, I2C_FIRST_FRAME);
    if(status != HAL_OK)
        return status;

    while(i2cSequentialTxDone == 0)
    {
        if((HAL_GetTick() - startTick) > 1000)
        {
            return HAL_TIMEOUT;
        }
    }

    status = HAL_I2C_Master_Seq_Receive_IT(platform->hi2c, STWBC86_I2C_ADDR << 1, rbuf, rlen, I2C_LAST_FRAME);
    if(status != HAL_OK)
        return status;

    while(i2cSequentialRxDone == 0)
    {
        if((HAL_GetTick() - startTick) > 1000)
            return HAL_TIMEOUT;
    }

    return 0;
}

/*
 * @brief  platform specific delay (platform dependent)
 * @param  millisec     delay in milliseconds
 *
 */
void platform_delay(uint32_t millisec)
{
    HAL_Delay(millisec);
}

/*
 * @brief  platform allocate memory (platform dependent)
 * @param  size         Memory size to be allocated
 *
 * @retval Pointer to allocated memory
 *
 */
void *platform_alloc_mem(size_t size)
{
#if USE_STATIC_ALLOC_RW
    int i;

    for (i = 0; i < MEMORY_BLOCKS_NUM; i++)
    {
        if (!stwlc_alloc_buf[i].is_allocated)
        {
            stwlc_alloc_buf[i].is_allocated = 1;
            return (void *)stwlc_alloc_buf[i].buf;
        }
    }

    return (void*) NULL;
#else
    return malloc(size);
#endif
}

/*
 * @brief  platform free memory (platform dependent)
 * @param  ptr          Pointer to memory to be free
 *
 */
void platform_free_mem(void *ptr)
{
#if USE_STATIC_ALLOC_RW
    int i;

    for (i = 0; i < MEMORY_BLOCKS_NUM; i++)
    {
        if (stwlc_alloc_buf[i].buf == (uint8_t *)ptr)
            stwlc_alloc_buf[i].is_allocated = 0;
    }
#else
    free(ptr);
#endif
}

/*
 * @brief  platform print output (platform dependent)
 * @param  phandle        Pointer to platform handle.
 * @param  level          log level
 * @param  msg            message to be printed
 * @param  len            size of message
 *
 * @retval None
 *
 */
void platform_log(void *phandle, int32_t level, const char *msg, int32_t len)
{
    struct stmdev_platform *platform = (struct stmdev_platform *)phandle;

    HAL_UART_Transmit(platform->huart, (uint8_t *)msg, len, 1000);
}

/* Main Example --------------------------------------------------------------*/
void stwbc86_fw_update_example(void)
{
    /** stwbc86 is the used part number **/
    struct stmdev_platform platform = { 0 };
    struct stwbc86_dev stwbc86 = { 0 };
    struct stwbc86_chip_info info = { 0 };

    platform.hi2c = &hi2c1;
    platform.huart = &huart2;

    stwbc86.bus_write = platform_write;
    stwbc86.bus_write_read = platform_write_read;
    stwbc86.mdelay = platform_delay;
    stwbc86.alloc_mem = platform_alloc_mem;
    stwbc86.free_mem = platform_free_mem;
    stwbc86.phandle = &platform;
    stwbc86.log = platform_log;
    stwbc86.log_info = 1;

    if (stwbc86_get_chip_info(&stwbc86, &info) != STWBC86_OK)
        return;

    if (stwbc86_fw_update(&stwbc86, STWBC86_FW_PATCH_CFG, 0) != STWBC86_OK)
        return;
}
