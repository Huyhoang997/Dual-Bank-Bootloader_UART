#ifndef BOOT_H_
#define BOOT_H_

#include "main.h"
#include "metadata.h"
#include "printf_log.h"

typedef enum
{
    BOOTLOADER_OK,
    BOOTLOADER_ERR,
    BOOTLOADER_SET_ACTIVE_FAIL,
    BOOOTLOADER_ERASE_FAIL
} BOOTLOADER_Status_Typedef;


/* Define macro for application A's vector table base address */
#define APP_A_START_ADDR                0x800C000UL
/* Define macro for application A's reset handler base address */
#define APP_A_RESET_HANDLER_ADDR        (APP_A_START_ADDR + 4)

/* Define macro for application B's vector table base address */
#define APP_B_START_ADDR                 0x08040000UL
/* Define macro for application B's reset handler base address */
#define APP_B_RESET_HANDLER_ADDR        (APP_B_START_ADDR + 4)

/* Define macro for metadata sector base address */
#define METADATA_START_ADDR             0x8008000UL

#define BOOTLOADER_WRITE_CMD            0x01U
#define BOOTLOADER_ERASE_CMD            0x02U
#define BOOTLOADER_UNKNOW_CMD           0x03U


void BOOTLOADER_InitMode(void);

#endif
