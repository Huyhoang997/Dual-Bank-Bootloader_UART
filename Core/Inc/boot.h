#ifndef BOOT_H_
#define BOOT_H_

#include "main.h"
#include "metadata.h"
#include "print_log.h"

typedef enum
{
    BOOTLOADER_OK,
    BOOTLOADER_INVALID_ACTIVE_APP,
    BOOTLOADER_INVALID_PENDING_FLAG,
    BOOTLOADER_PROGRAM_ERR,
    BOOTLOADER_ERASE_ERR,
    BOOTLOADER_VERIFY_CRC_FAIL,
    BOOTLOADER_INVALID_HEADER
} BOOTLOADER_Status_Typedef;

#define APP_A_START_ADDR                0x800C000UL
#define APP_A_RESET_HANDLER_ADDR        (APP_A_START_ADDR + 4)


#define APP_B_START_ADDR                0x8040000UL
#define APP_B_RESET_HANDLER_ADDR        (APP_B_START_ADDR + 4)

#define BOOTLOADER_CMD_WRITE            0x01U
#define BOOTLOADER_CMD_ERASE            0x02U
#define HEADER_INFO                     0x55AA

void BOOTLOADER_Init(void);

#endif