#include "metadata.h"

METADATA_Sector_t cfg __attribute__((section(".metadata"))) = 
{
    .header = 0x55AA,
    .active_bank = 0xB,
    .pending_bank = 0,
    .crc_hostappA = 0x8c987ee1,
    .crc_hostappB = 0,
    .size__appA = 5776,
    .size__appB = 0,
};