#include "VL53L0X.h"
#include <stdbool.h>

// External I2C handle
extern I2C_HandleTypeDef hi2c1;

// Shift the 7-bit I2C address left by 1 to fit the STM32 HAL format (which includes the R/W bit space)
#define ADDR (VL53L0X_ADDR_7BIT << 1)

// Stores internal stop variable configuration required by the VL53L0X state machine
static uint8_t stop_var;


/**
 * @brief Write a single byte to an 8-bit register on the VL53L0X.
 * @param reg Register address.
 * @param val Value to write.
 */
static void wr(uint8_t reg, uint8_t val)
{
    HAL_I2C_Mem_Write(&hi2c1, ADDR, reg, I2C_MEMADD_SIZE_8BIT, &val, 1, 100);
}

/**
 * @brief Read a single byte from an 8-bit register on the VL53L0X.
 * @param reg Register address.
 * @return The value read from the register.
 */
static uint8_t rd(uint8_t reg)
{
    uint8_t val = 0;
    HAL_I2C_Mem_Read(&hi2c1, ADDR, reg, I2C_MEMADD_SIZE_8BIT, &val, 1, 100);
    return val;
}

/**
 * @brief Write a list of register-value pairs to the sensor.
 * @param list Pointer to an array containing alternating {register, value} bytes.
 * @param pairs Number of pairs to write.
 */
static void wr_list(const uint8_t *list, uint16_t pairs)
{
    for (uint16_t i = 0; i < pairs; i++)
        wr(list[2 * i], list[2 * i + 1]);
}

/**
 * @brief Wait until a register bitmask matches a target condition (zero or non-zero).
 * @param reg Register address to monitor.
 * @param mask Bitmask to apply.
 * @param until_zero If true, wait until masked value is 0; if false, wait until non-zero.
 * @return true if condition met, false if timeout occurs.
 */
static bool wait_reg(uint8_t reg, uint8_t mask, bool until_zero)
{
    uint32_t t0 = HAL_GetTick();
    for (;;) {
        uint8_t v = rd(reg);
        if (until_zero ? ((v & mask) == 0) : ((v & mask) != 0)) return true;
        if (HAL_GetTick() - t0 > VL53L0X_TIMEOUT_MS) return false;
    }
}


/**
 * @brief Retrieve SPAD (Single Photon Avalanche Diode) configuration info from the sensor.
 * @param count Pointer to store the SPAD count.
 * @param is_aperture Pointer to store whether an aperture SPAD layout is used.
 * @return true on success, false otherwise.
 */
static bool get_spad_info(uint8_t *count, bool *is_aperture)
{
    static const uint8_t pre[]  = { 0x80,0x01, 0xFF,0x01, 0x00,0x00, 0xFF,0x06 };
    static const uint8_t mid[]  = { 0xFF,0x07, 0x81,0x01, 0x80,0x01, 0x94,0x6B, 0x83,0x00 };
    static const uint8_t post[] = { 0xFF,0x01, 0x00,0x01, 0xFF,0x00, 0x80,0x00 };

    wr_list(pre, 4);
    wr(0x83, rd(0x83) | 0x04);
    wr_list(mid, 5);

    if (!wait_reg(0x83, 0xFF, false)) return false;
    wr(0x83, 0x01);

    uint8_t tmp = rd(0x92);
    *count       = tmp & 0x7F;
    *is_aperture = (tmp >> 7) & 0x01;

    wr(0x81, 0x00);
    wr(0xFF, 0x06);
    wr(0x83, rd(0x83) & ~0x04);
    wr_list(post, 4);
    return true;
}

/**
 * @brief Perform reference calibration steps (VHV or phase).
 * @param vhv_init_byte Calibration command parameter.
 * @return true on success, false on timeout.
 */
static bool ref_calibration(uint8_t vhv_init_byte)
{
    wr(REG_SYSRANGE_START, 0x01 | vhv_init_byte);
    if (!wait_reg(REG_RESULT_INTERRUPT_STATUS, 0x07, false)) return false;
    wr(REG_SYSTEM_INTERRUPT_CLEAR, 0x01);
    wr(REG_SYSRANGE_START, 0x00);
    return true;
}

// Undocumented tuning values provided directly by STMicroelectronics for optimal sensor performance
static const uint8_t tuning[] = {
    0xFF,0x01, 0x00,0x00, 0xFF,0x00, 0x09,0x00, 0x10,0x00, 0x11,0x00,
    0x24,0x01, 0x25,0xFF, 0x75,0x00, 0xFF,0x01, 0x4E,0x2C, 0x48,0x00,
    0x30,0x20, 0xFF,0x00, 0x30,0x09, 0x54,0x00, 0x31,0x04, 0x32,0x03,
    0x40,0x83, 0x46,0x25, 0x60,0x00, 0x27,0x00, 0x50,0x06, 0x51,0x00,
    0x52,0x96, 0x56,0x08, 0x57,0x30, 0x61,0x00, 0x62,0x00, 0x64,0x00,
    0x65,0x00, 0x66,0xA0, 0xFF,0x01, 0x22,0x32, 0x47,0x14, 0x49,0xFF,
    0x4A,0x00, 0xFF,0x00, 0x7A,0x0A, 0x7B,0x00, 0x78,0x21, 0xFF,0x01,
    0x23,0x34, 0x42,0x00, 0x44,0xFF, 0x45,0x26, 0x46,0x05, 0x40,0x40,
    0x0E,0x06, 0x20,0x1A, 0x43,0x40, 0xFF,0x00, 0x34,0x03, 0x35,0x44,
    0xFF,0x01, 0x31,0x04, 0x4B,0x09, 0x4C,0x05, 0x4D,0x04, 0xFF,0x00,
    0x44,0x00, 0x45,0x20, 0x47,0x08, 0x48,0x28, 0x67,0x00, 0x70,0x04,
    0x71,0x01, 0x72,0xFE, 0x76,0x00, 0x77,0x00, 0xFF,0x01, 0x0D,0x01,
    0xFF,0x00, 0x80,0x01, 0x01,0xF8, 0xFF,0x01, 0x8E,0x01, 0x00,0x01,
    0xFF,0x00, 0x80,0x00
};



/**
 * @brief Initialize the VL53L0X sensor (performs setup sequence, SPAD configuration, and calibration).
 */
void VL53L0X_Init(void)
{
    /* 1. Verify sensor model ID response */
    if (rd(REG_IDENTIFICATION_MODEL_ID) != 0xEE) return;

    /* 2. Configure 2.8V I/O mode and standard I2C operating mode */
    wr(REG_VHV_CONFIG_PAD_SCL_SDA_EXTSUP,
       rd(REG_VHV_CONFIG_PAD_SCL_SDA_EXTSUP) | 0x01);
    wr(0x88, 0x00);

    /* 3. Read and store internal stop variable */
    wr(0x80, 0x01); wr(0xFF, 0x01); wr(0x00, 0x00);
    stop_var = rd(0x91);
    wr(0x00, 0x01); wr(0xFF, 0x00); wr(0x80, 0x00);

    /* 4. Disable select limit checks and set signal rate limit to 0.25 MCPS */
    wr(REG_MSRC_CONFIG_CONTROL, rd(REG_MSRC_CONFIG_CONTROL) | 0x12);
    wr(REG_FINAL_RANGE_MIN_COUNT_RATE, 0x00);       // High byte
    wr(REG_FINAL_RANGE_MIN_COUNT_RATE + 1, 0x20);   // Low byte: 0.25 * 128 = 32 (0x20)
    wr(REG_SEQUENCE_CONFIG, 0xFF);

    /* 5. Perform SPAD (Single Photon Avalanche Diode) reference setup */
    uint8_t spad_count;
    bool aperture;
    if (!get_spad_info(&spad_count, &aperture)) return;

    uint8_t spad_map[6];
    HAL_I2C_Mem_Read(&hi2c1, ADDR, REG_GLOBAL_CONFIG_SPAD_ENABLES_REF,
                     I2C_MEMADD_SIZE_8BIT, spad_map, 6, 100);

    wr(0xFF, 0x01);
    wr(REG_DYNAMIC_SPAD_REF_EN_START_OFF, 0x00);
    wr(REG_DYNAMIC_SPAD_NUM_REQUESTED_REF, 0x2C);
    wr(0xFF, 0x00);
    wr(REG_GLOBAL_CONFIG_REF_EN_START_SEL, 0xB4);

    uint8_t first = aperture ? 12 : 0;
    uint8_t enabled = 0;
    for (uint8_t i = 0; i < 48; i++) {
        if (i < first || enabled == spad_count)
            spad_map[i / 8] &= ~(1 << (i % 8));
        else if ((spad_map[i / 8] >> (i % 8)) & 0x01)
            enabled++;
    }
    HAL_I2C_Mem_Write(&hi2c1, ADDR, REG_GLOBAL_CONFIG_SPAD_ENABLES_REF,
                      I2C_MEMADD_SIZE_8BIT, spad_map, 6, 100);

    /* 6. Apply mandatory tuning settings */
    wr_list(tuning, sizeof(tuning) / 2);

    /* 7. Configure GPIO interrupt for "new measurement ready" event */
    wr(REG_SYSTEM_INTERRUPT_CONFIG_GPIO, 0x04);
    wr(REG_GPIO_HV_MUX_ACTIVE_HIGH, rd(REG_GPIO_HV_MUX_ACTIVE_HIGH) & ~0x10);
    wr(REG_SYSTEM_INTERRUPT_CLEAR, 0x01);

    /* 8. Execute internal reference calibrations */
    wr(REG_SEQUENCE_CONFIG, 0xE8);

    wr(REG_SEQUENCE_CONFIG, 0x01);
    if (!ref_calibration(0x40)) return;     // VHV calibration

    wr(REG_SEQUENCE_CONFIG, 0x02);
    if (!ref_calibration(0x00)) return;     // Phase calibration

    wr(REG_SEQUENCE_CONFIG, 0xE8);
}


/**
 * @brief Trigger a distance measurement and retrieve the result.
 * @return Measured distance in millimeters, or 0xFFFF if a timeout/error occurs.
 */
uint16_t VL53L0X_Dist(void)
{
    uint8_t r[2];

    /* Restore stop variable and initiate ranging sequence */
    wr(0x80, 0x01); wr(0xFF, 0x01); wr(0x00, 0x00);
    wr(0x91, stop_var);
    wr(0x00, 0x01); wr(0xFF, 0x00); wr(0x80, 0x00);

    wr(REG_SYSRANGE_START, 0x01);

    // Wait for measurement start and completion interrupts with timeouts
    if (!wait_reg(REG_SYSRANGE_START, 0x01, true))          return 0xFFFF;
    if (!wait_reg(REG_RESULT_INTERRUPT_STATUS, 0x07, false)) return 0xFFFF;

    // Read 2-byte distance result from the output registers
    HAL_I2C_Mem_Read(&hi2c1, ADDR, REG_RESULT_RANGE_STATUS + 10,
                     I2C_MEMADD_SIZE_8BIT, r, 2, 100);
    wr(REG_SYSTEM_INTERRUPT_CLEAR, 0x01);

    // Combine high and low bytes into final distance value (mm)
    return (r[0] << 8) | r[1];
}
