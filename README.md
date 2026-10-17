# STM32-l476rg-VL53L0X

An I2C driver for the STMicroelectronics VL53L0X time-of-flight distance sensor,
written for the STM32 NUCLEO-L476RG using STM32CubeIDE and the STM32 HAL library.

## Features
- Distance measurement in millimetres
- Communication over I2C
- Built with STM32CubeIDE and HAL

## Hardware
- STM32 NUCLEO-L476RG
- VL53L0X time-of-flight sensor module

## Wiring (I2C)

| VL53L0X pin | Nucleo pin | Note                  |
|-------------|------------|-----------------------|
| VIN / VCC   | 3V3        | Check module rating   |
| GND         | GND        |                       |
| SDA         | PB9 (D14)  | I2C1_SDA              |
| SCL         | PB8 (D15)  | I2C1_SCL              |
| XSHUT       | [GPIO]     | Optional, sensor reset|

Default I2C address: `0x29` (7-bit). The driver shifts it left by one bit
for the STM32 HAL.

## Project structure
The STM32CubeIDE project is located in the `[VL53L0X]/` folder.
The CubeMX configuration is in `[VL53L0X]/[VL53L0X].ioc`.

## Usage

```c
#include "[vl53l0x.h]"

if ([VL53L0X_Init]() != 0) {
    // Sensor not found, check wiring and I2C address
}

while (1) {
    uint16_t distance_mm = [VL53L0X_ReadDistance]();
    HAL_Delay(100);
}
```

## License
MIT. If the project includes ST's VL53L0X API files, they are covered by
ST's own license terms.
