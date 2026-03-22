#ifndef METADATA_H_
#define METADATA_H_

#include "stdint.h"

typedef struct __attribute__((aligned(4)))
{
    uint32_t  header;
    
    uint32_t size__appA;
    uint32_t crc_hostappA;
    uint32_t  versionA;

    uint32_t size__appB;
    uint32_t crc_hostappB;
    uint32_t versionB;

    uint32_t active_bank;
    uint32_t pending_bank;
} METADATA_Sector_t;

typedef enum
{
    ACTIVE_A = 0xA,
    ACTIVE_B = 0xB
} METADATA_SetActiveApp_Typedef;

 
#endif