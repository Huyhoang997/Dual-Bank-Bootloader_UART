# STM32 Dual-Bank Bootloader

A custom bootloader for STM32 microcontrollers implementing a dual-bank (A/B) firmware update strategy over UART with CRC32 verification and rollback support.

---

## Features

- Dual-bank firmware storage (App A & App B)
- UART-based firmware transfer (128-byte packets)
- CRC32 integrity verification
- Atomic bank switching with metadata persistence
- One-click rollback to previous firmware version
- Pending flag mechanism for safe update confirmation
- Hardware GPIO trigger to enter bootloader mode

---

## Flash Memory Layout

| Region | Sector | Description |
|---|---|---|
| Bootloader | 0 – 1 | Bootloader code |
| Metadata | 2 | Firmware metadata (header, size, CRC, version, active bank, pending flag) |
| App A | 3 – 5 | Application A firmware |
| App B | 6 | Application B firmware |

BOOTLOADER
<img width="1595" height="197" alt="image" src="https://github.com/user-attachments/assets/0c3b30fe-1719-4522-8f9d-cc6d0561df7e" />

APPLICATION A
<img width="1591" height="167" alt="image" src="https://github.com/user-attachments/assets/b62db8c4-e655-4626-81bf-933a0b0dbc19" />

APPLICATION B

<img width="1586" height="220" alt="image" src="https://github.com/user-attachments/assets/0de21421-77a6-4381-aa88-ebd1d56bfcb1" />


---

## Metadata Sector Structure
```c
typedef struct {
    uint32_t header;            // Magic header (0x55AA)
    uint32_t Host_sizeAppA;     // Size of App A
    uint32_t Host_crcappA;      // CRC32 of App A
    uint32_t Host_versionAppA;  // Version of App A
    uint32_t Host_sizeAppB;     // Size of App B
    uint32_t Host_crcappB;      // CRC32 of App B
    uint32_t Host_versionAppB;  // Version of App B
    uint32_t active_bank;       // Currently active bank (App A / App B)
    uint32_t pending_flag;      // 1 = new firmware pending, 0 = stable
} METADATA_SecDef_t;
```

---

## Behavior

### Boot Flow

On every power-on or reset, the bootloader runs the following logic:

1. Check GPIO PA0 — if LOW, enter Bootloader Mode and wait for a UART command.
2. Read active_bank and pending_flag from the Metadata sector.
3. If pending_flag == 1, switch to the opposite bank, clear the flag, and jump.
4. If pending_flag == 0, jump directly to the currently active bank.
```
Power ON / Reset
      │
      ▼
GPIO PA0 LOW? ──YES──► Enter Bootloader Mode (UART command wait)
      │
      NO
      ▼
Read active_bank from Metadata
      │
      ├── ACTIVE_APP_A + pending_flag=1 ──► Switch to App B ──► Jump to App B
      ├── ACTIVE_APP_A + pending_flag=0 ──► Jump to App A
      ├── ACTIVE_APP_B + pending_flag=1 ──► Switch to App A ──► Jump to App A
      └── ACTIVE_APP_B + pending_flag=0 ──► Jump to App B


```

<img width="618" height="538" alt="image" src="https://github.com/user-attachments/assets/9b1e621f-e7f3-414c-b560-d32078091440" />


### Firmware Update Flow

1. Pull GPIO PA0 LOW before reset to enter bootloader mode.
2. Send command byte BOOTLOADER_CMD_WRITE via UART.
3. Send a 16-byte Info Frame containing header, size, CRC32, and version.
4. Wait for START signal from the device.
5. Send firmware in 128-byte packets, wait for ACK after each packet.
6. Receive DONE when all packets are transferred.
7. Send a reset — the bootloader verifies CRC, sets pending_flag, switches bank, and jumps to the new firmware.
```
Host                         STM32 Bootloader
  │                               │
  │──── CMD_WRITE ───────────────►│
  │──── Info Frame (16 bytes) ───►│
  │                               │ (erase target sector)
  │◄─── "START\n" ───────────────│
  │──── Packet 1 (128 bytes) ────►│
  │◄─── "ACK\n" ─────────────────│
  │──── Packet 2 (128 bytes) ────►│
  │◄─── "ACK\n" ─────────────────│
  │         ... (repeat)          │
  │◄─── "DONE\n" ────────────────│
  │                               │ (CRC verify → set pending_flag=1)
```

### Rollback Flow

1. Pull GPIO PA0 LOW before reset to enter bootloader mode.
2. Send command byte BOOTLOADER_CMD_ERASE via UART.
3. The bootloader erases the currently active bank's sectors.
4. Switches active_bank to the other bank.
5. On next boot, automatically jumps to the older firmware.


<img width="614" height="538" alt="image" src="https://github.com/user-attachments/assets/1c64a624-ce95-48a5-8580-c411cbead8fb" />


### LED Mapping

| LED | Color | Condition |
|-----|-------|-----------|
| PA1 | Green | ON when in Bootloader Mode |
| PA1 | Off | OFF when running application |

---

## UART Protocol

### Commands

| Command Byte | Action |
|---|---|
| BOOTLOADER_CMD_WRITE | Start firmware update |
| BOOTLOADER_CMD_ERASE | Erase current firmware & rollback |

### Info Frame Format (16 bytes)

| Bytes | Field | Description |
|---|---|---|
| [0:3] | Header | Must equal 0x55AA |
| [4:7] | Size | Firmware size in bytes |
| [8:11] | CRC32 | Expected CRC32 of firmware |
| [12:15] | Version | Firmware version |

---

## API Reference

| Function | Description |
|---|---|
| BOOTLOADER_Init() | Entry point — checks boot mode and jumps to the correct app |
| BOOTLOADER_WaitCmd() | Waits for a UART command byte |
| BOOTLOADER_ReceiveInfoFrame() | Receives and stores firmware metadata from host |
| BOOTLOADER_ReceiveFirmware() | Receives firmware packets and writes to flash |
| BOOTLOADER_VerifyFirmware() | CRC32 verification of received firmware |
| BOOTLOADER_SetActiveBank() | Persists active bank selection to metadata sector |
| BOOTLOADER_PendingFlag() | Sets or clears the pending update flag |
| BOOTLOADER_EraseCurrentFirmware() | Erases active firmware for rollback |
| JUMP_To_AppA() | Jumps execution to App A |
| JUMP_To_AppB() | Jumps execution to App B |

---

## Return Codes

| Code | Meaning |
|---|---|
| BOOTLOADER_OK | Operation successful |
| BOOTLOADER_ERASE_ERR | Flash erase failed |
| BOOTLOADER_PROGRAM_ERR | Flash write failed |
| BOOTLOADER_INVALID_HEADER | Info frame header mismatch |
| BOOTLOADER_INVALID_ACTIVE_APP | Invalid bank selection |
| BOOTLOADER_INVALID_PENDING_FLAG | Invalid pending flag value |
| BOOTLOADER_VERIFY_CRC_FAIL | CRC32 mismatch after transfer |

---

## Hardware Requirements

| Peripheral | Usage |
|---|---|
| UART1 | Firmware transfer & debug output |
| GPIO PA0 | Bootloader mode trigger (active LOW) |
| GPIO PA1 | Bootloader mode indicator LED |
| CRC Unit | Hardware CRC32 calculation |
| Internal Flash | Firmware & metadata storage |

---

## Known Limitations

- BOOTLOADER_VerifyFirmware() always uses Host_sizeAppB for length calculation regardless of active_bank — bug when active_bank == ACTIVE_APP_B
- No timeout handling on UART receive (uses HAL_MAX_DELAY)
- No retry mechanism on failed packet receive
- Flash write is byte-by-byte which is slower than word-aligned writes

---

## License

MIT License — feel free to use and modify for your own STM32 projects.
