#include "metadata.h"

/* Created medata sector and save factory informations */
METADATA_SecDef_t cfg __attribute__((section(".metadata"))) =
{
     .header = 0x55AA,
     .Host_versionAppA = 1,
     .Host_versionAppB = 1,
     .active_bank = 0xA
};
