# SRRReceiver

STM32G070KBT6 (Cortex-M0+, 32KB flash, LQFP32) firmware for a dual-channel
2.4 GHz race timing receiver. Receives SportIdent punch packets via two CC2500
radios, buffers them, and serves them to a host (Allwinner H3 SBC) over I2C slave.
Also supports transmit mode — punches received over I2C are sent out via CC2500.

## Build

This is an STM32CubeIDE project. The IDE auto-generates Makefiles under
`Debug/` and `Release/`. Use CubeIDE's bundled GCC (GNU Tools for STM32 11.3),
not the system `arm-none-eabi-gcc` — the system one lacks `-fcyclomatic-complexity`.

### Command line

```bash
TOOLS="/opt/st/stm32cubeide_1.14.0/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.11.3.rel1.linux64_1.1.100.202309141235/tools/bin"
PATH="$TOOLS:$PATH"

# Debug build (-O0 -g3 -DDEBUG)
make -j$(nproc) -C Debug

# Release build (-Os, no debug, -DNDEBUG)
make -j$(nproc) -C Release
```

### VS Code

Ctrl+Shift+B runs the Debug build. Tasks are configured in `.vscode/tasks.json`.
IntelliSense needs `.vscode/c_cpp_properties.json` for STM32 HAL header paths.

Build configurations:
| Config | Dir | Flags |
|--------|-----|-------|
| Debug | `Debug/` | `-O0 -g3 -DDEBUG` |
| Release | `Release/` | `-Os` (no debug) |

## Hardware

| Peripheral | Pins                           | Purpose                            |
| ---------- | ------------------------------ | ---------------------------------- |
| SPI1       | PA15(CS), PA3(MISO), PA4(MOSI) | CC2500 Red channel (ch 146)        |
| SPI2       | PA5(CS)                        | CC2500 Blue channel (ch 186)       |
| I2C1       | PA10(SDA)                      | Slave at addr 0x20, host interface |
| USART1     | PA9(TX)                        | 115200 baud debug/error log        |
| EXTI4-15   | PA12 (falling), PA6 (falling)  | CC2500 GDO0 sync-word interrupts   |

### CC2500 GDO0 pin modes

The GDO0 pin on each CC2500 is dynamically reconfigured:

- **Falling interrupt** (GDOx_CFG_ASSERT_SYNC_WORD): RX mode — asserts when sync word detected
- **Rising interrupt** (GDOx_CFG_PA_PD): TX mode — edge at TX completion enters RX
- **GPIO input**: Disabled/transitional

Functions: `Configure_GDO_INT_1/2_AsFallingInterrupt()`, `_AsRisingInterrupt()`, `_AsGPIO()`

## Channels

- Base frequency (channel 0): 2,424,999,878 Hz about 2.4250 GHz
- Channel spacing: (26 MHz / 2^18) x (256 + 59) x 2^3 = 249.9 kHz

---

| Channel | Calculation | Frequency |
| Red (146) | 2424.999878 + (146 x 0.249939) | 2,461.49 MHz |
| Blue (186) | 2424.999878 + (186 x 0.249939) | 2,471.49 MHz |

Both are in the upper end of the 2.4 GHz ISM band.

## Architecture

```
[HOST (Allwinner H3 SBC)]
      |
  I2C (slave addr 0x20)
      |
  [STM32G070]────SPI1────[CC2500 #1, 2.461 GHz, ch 146]  Red channel
      |                  GDO0 → PA12 (EXTI12, falling)
      |
      └──────SPI2────[CC2500 #2, 2.471 GHz, ch 186]  Blue channel
                     GDO0 → PA6 (EXTI6, falling)
```

### Key constants (main.h)

```c
#define REDCHANNEL  146
#define BLUECHANNEL 186
```

### Receive flow (radio → I2C)

1. CC2500 GDO0 goes low (sync word match) → EXTI falling ISR
2. ISR disables further EXTI interrupts
3. `ReadMessage()` reads packet via SPI, validates CRC
4. If ACK (length==14, dest==our serialno): pop outgoing TX queue, set `txLastAckedChannel`
5. If punch: enqueue to `incomingPunchQueue`, send ACK reply via `SendAckReply_*Channel()`
6. Rising ISR (`ResumeRX_*Channel`) reconfigures back to RX mode

### Transmit flow (I2C → radio)

1. Host writes to I2C register `PUNCHREGADDR (0x40)`:
   - Byte 0: payload length
   - Bytes 1..N: payload (max 15)
2. Assembled into `TxPunch`, enqueued to `outgoingTxPunchQueue`
3. Main loop calls `ProcessOutgoingPunches()`
4. `BuildRadioPacketFromTxPunch()` assembles: `[len][dest"siok"][src serial][PORT=0x3F][DevInfo=0x03][msgSeq][punchSeq][payload]`
5. `SendPunch_*Channel()` writes TX FIFO, waits CCA, strobes STX
6. ACK detection in `ReadMessage` pops the queue on success
7. Retries: up to 6 attempts with exponential backoff (64/128/256/512/1024 ms)
8. Channel selection: first try on last-ACKed channel, retries alternate

## Key source files

| File                               | Purpose                                                        |
| ---------------------------------- | -------------------------------------------------------------- |
| `Core/Src/main.c`                  | System init, main loop, CC2500 init/config, radio packet RX/TX |
| `Core/Src/CC2500.c` / `.h`         | CC2500 SPI driver (register R/W, FIFO ops, state commands)     |
| `Core/Src/I2CSlave.c` / `.h`       | I2C slave protocol, register map, feature flags                |
| `Core/Src/PunchQueue.c` / `.h`     | Circular queues for incoming and outgoing punches              |
| `Core/Src/ErrorLog.c` / `.h`       | Error logging with UART output                                 |
| `Core/Src/IRQLineHandler.c` / `.h` | GPIO IRQ line to host (punch/error notification)               |
| `Core/Src/stm32g0xx_it.c`          | Interrupt handlers (SysTick, EXTI, I2C, SPI)                   |

## I2C register map (slave addr 0x20)

| Address | Name             | R/W | Description                                        |
| ------- | ---------------- | --- | -------------------------------------------------- |
| 0x00    | FIRMWAREVERSION  | R   | Firmware version (1)                               |
| 0x01    | HARDWAREFEATURES | R   | Available hardware features bitmap                 |
| 0x02    | SERIALNO         | R/W | 4-byte dongle serial number                        |
| 0x03    | ERRORCOUNT       | R   | Error count (uint16)                               |
| 0x04    | STATUS           | R   | bit7=errors, bit0=punches available                |
| 0x05    | SETDATAINDEX     | R/W | Index into block data registers                    |
| 0x06    | FEATURES_ENABLE  | R/W | Enabled/disable bits for red/blue/UART/listen/send |
| 0x20    | PUNCHLENGTH      | R   | Length of next punch to read                       |
| 0x27    | ERRORLENGTH      | R   | Length of next error message                       |
| 0x40    | PUNCHDATA        | R/W | Read incoming punch / write outgoing punch         |
| 0x47    | ERRORMSG         | R   | Error message text                                 |

Feature bits: bit0=RED, bit1=BLUE, bit2=UART errors, bit3=RED listen-only,
bit4=BLUE listen-only, bit5=Send mode

## Testing

`PythonI2CTest.py` at repo root — Python script for testing I2C communication
with the receiver from host.

## Coding conventions

- Pre-existing code uses tabs for indentation, match it
- ISR-called functions must be fast (no `HAL_Delay` in ISR)
- SPI bus access is not reentrant: bracket with `HAL_NVIC_DisableIRQ(EXTI4_15_IRQn)` / `_EnableIRQ`
- Both CC2500 channels share `EXTI4_15_IRQn` — disabling it blocks both
- `volatile` for variables accessed from ISR + main loop
- `struct Punch` = incoming (radio → I2C), `struct TxPunch` = outgoing (I2C → radio)
- Important to send Ack to punches with minimal delay because sender expects quick reply - and goes to sleep if not recieved quickly
- Main loop uses `__WFI()` to sleep the CPU between iterations — SysTick (1ms), I2C, and EXTI interrupts wake it.
  Saves ~20 mA. Do not replace with `HAL_Delay()` — that would reintroduce active spin-waiting.
