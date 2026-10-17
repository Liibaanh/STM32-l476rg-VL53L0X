#ifndef INC_VL53L0X_H_
#define INC_VL53L0X_H_

#include "stm32l4xx_hal.h"

// I2C Address configuration (shifted left by 1 for STM32 HAL compatibility)
#define VL53L0X_ADDR_7BIT   0x29
#define ADDR                (VL53L0X_ADDR_7BIT << 1)

// Operation timeout configuration in milliseconds
#define VL53L0X_TIMEOUT_MS  500

// Register Map

#define REG_SYSRANGE_START                 0x00
#define REG_SEQUENCE_CONFIG                0x01
#define REG_SYSTEM_INTERRUPT_CONFIG_GPIO   0x0A
#define REG_SYSTEM_INTERRUPT_CLEAR         0x0B
#define REG_RESULT_INTERRUPT_STATUS        0x13
#define REG_RESULT_RANGE_STATUS            0x14
#define REG_MSRC_CONFIG_CONTROL            0x60
#define REG_FINAL_RANGE_MIN_COUNT_RATE     0x44
#define REG_GPIO_HV_MUX_ACTIVE_HIGH        0x84
#define REG_VHV_CONFIG_PAD_SCL_SDA_EXTSUP  0x89
#define REG_GLOBAL_CONFIG_SPAD_ENABLES_REF 0xB0
#define REG_GLOBAL_CONFIG_REF_EN_START_SEL 0xB6
#define REG_DYNAMIC_SPAD_NUM_REQUESTED_REF 0x4E
#define REG_DYNAMIC_SPAD_REF_EN_START_OFF  0x4F
#define REG_IDENTIFICATION_MODEL_ID        0xC0

/**
 * @brief Initialize the VL53L0X Time-of-Flight sensor.
 */
void VL53L0X_Init(void);

/**
 * @brief Trigger a single distance measurement and read back the result.
 * @return Distance in millimeters (mm), or 0xFFFF if a timeout or error occurs.
 */
uint16_t VL53L0X_Dist(void);

#endif /* INC_VL53L0X_H_ */
