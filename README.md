# Asynchronous VL53L0X ToF Sensor Driver for STM32F103

**Read me in English** | [Читати мене Українською](README_uk.md)

An optimized, lightweight **non-blocking C driver** for the **VL53L0X** Time-of-Flight (ToF) laser distance sensor, designed specifically for the STM32F103C8T6 (Blue Pill) microcontroller using the **STM32CubeHAL** library.

This project is a heavily modified and architecturally enhanced fork of the original implementation by *Squieler/VL53L0X---STM32-HAL*. It re-engineers blocking data polling into an asynchronous state machine suitable for real-time robotic systems.

---

## 🎯 Key Enhancements Over the Base Driver

1. **Zero-Blocking State Machine:** Removed all hard blocking `while()` timeout loops inside the ranging sequence. Ranging is split into a standalone "trigger" function and a "check/read" function, freeing up CPU time for parallel tasks.
2. **Byte-Endianness Fix:** Resolved a critical raw 16-bit register reading bug (`0x1E` / `0x1F`) by implementing explicit byte-swapping, enabling reliable, high-speed distance parsing.
3. **High-Speed & Long-Range Tuning:** Reconfigured internal VCSEL macro-period pulse widths (`PreRange = 18`, `FinalRange = 14`) and dropped the return signal limit (`SignalRateLimit = 100`). This extends the indoor ranging capability close to **2.0 meters** while cutting the measurement timing budget down to just **30 ms**.
4. **Hardware Timeout Protection:** Integrated a software timestamp watchdog (`100 ms`) to isolate the firmware against system stalls if the I2C physical lines break during operation.

---

## 🛠️ Toolchain & Hardware Constraints

- **Target MCU:** STM32F103C8T6 (ARM Cortex-M3 @ 72 MHz)
- **Bus Profile:** **I2C1 Fast Mode (400 kHz)** is mandatory. Standard 100 kHz speed introduces excessive bus communication delays that degrade state machine responsiveness.
- **IDE & Compiler:** STM32CubeIDE (GCC toolchain)

---

## 🔌 Hardware Connection (Pinout)

| Sensor Pin | STM32F103 Pin | Description |
| :---: | :---: | :--- |
| **VCC** | **3.3V** | Main Power Supply |
| **GND** | **GND** | Common Ground |
| **SCL** | **PB6** | I2C1 Clock (Fast Mode 400 kHz) |
| **SDA** | **PB7** | I2C1 Data (Fast Mode 400 kHz) |

---