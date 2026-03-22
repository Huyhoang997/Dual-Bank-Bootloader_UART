#ifndef METADATA_H_
#define METADATA_H_

#include "stdint.h"

typedef struct 
{
    uint32_t header;

    uint32_t Host_sizeAppA;
    uint32_t Host_crcappA;
    uint32_t Host_versionAppA;

    uint32_t Host_sizeAppB;
    uint32_t Host_crcappB;
    uint32_t Host_versionAppB;

    uint32_t active_bank;
    uint32_t pending_flag;
} METADATA_SecDef_t;

typedef enum
{
    ACTIVE_APP_A = 0xA,
    ACTIVE_APP_B = 0xB
} METADATA_SetActive;

#define METADATA_START_ADDR             0x8008000UL
#endif