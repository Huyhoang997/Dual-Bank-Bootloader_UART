#include "boot.h"

void JUMP_To_App_A(void)
{
    volatile uint32_t app_vector_table_addr = *(volatile uint32_t*)APP_A_START_ADDR;
    volatile uint32_t app_reset_handler_addr = *(volatile uint32_t*)APP_A_RESET_HANDLER_ADDR;
    __disable_irq();
    HAL_DeInit();
    __set_MSP(app_vector_table_addr);
    SCB->VTOR = APP_A_START_ADDR;
    void (*FunctionPtr)(void) = (void *)app_reset_handler_addr;
    __enable_irq();
    FunctionPtr();
}


void JUMP_To_App_B(void)
{
    volatile uint32_t app_vector_table_addr = *(volatile uint32_t*)APP_B_START_ADDR;
    volatile uint32_t app_reset_handler_addr = *(volatile uint32_t*)APP_B_RESET_HANDLER_ADDR;
    __disable_irq();
    HAL_DeInit();
    __set_MSP(app_vector_table_addr);
    SCB->VTOR = APP_B_START_ADDR;
    void (*FunctionPtr)(void) = (void *)app_reset_handler_addr;
    __enable_irq();
    FunctionPtr();
}


void SET_ActiveBank
BOOTLOADER_Status_Typedef BOOTLOADER_EraseCurrentFirmware(void)
{
    METADATA_Sector_t app_status = *(METADATA_Sector_t*)METADATA_START_ADDR;
    HAL_StatusTypeDef erase_status;
    uint32_t SectorErr;
    FLASH_EraseInitTypeDef cfg;
    cfg.TypeErase = FLASH_TYPEERASE_SECTORS;
    cfg.VoltageRange = FLASH_VOLTAGE_RANGE_3;

    HAL_FLASH_Unlock();
    if(app_status.active_bank == ACTIVE_A)
    {
        /* Erase firmware A */
        cfg.Sector = 3;
        cfg.NbSectors = 3;
        erase_status = HAL_FLASHEx_Erase(&cfg, &SectorErr);
        if(erase_status != HAL_OK)
        {
            return BOOOTLOADER_ERASE_FAIL;
        }    
    }
    else
    {
        /* Erase firmware B */
        cfg.Sector = 6;
        cfg.NbSectors = 1;
        erase_status = HAL_FLASHEx_Erase(&cfg, &SectorErr);
        if(erase_status != HAL_OK)
        {
            return BOOOTLOADER_ERASE_FAIL;
        }
    }
    HAL_FLASH_Lock();
    return BOOTLOADER_OK;
}

void BOOTLOADER_ReceiveInfoFrame(void)
{
    METADATA_Sector_t *metadata = (METADATA_Sector_t*)METADATA_START_ADDR;
    uint8_t info_frame[16] = {0};
    uint32_t host_header, host_size, host_crc, host_version;
    uint32_t size_appA_backup, size_appB_backup, crc_appA_backup, crc_appB_backup, version_appA_backup, version_appB_backup;
    HAL_StatusTypeDef erase_status;
    uint32_t SectorErr;
    FLASH_EraseInitTypeDef cfg;

    HAL_UART_Receive(&huart1, info_frame, 16, HAL_MAX_DELAY);
    host_header = ((uint32_t)info_frame[0] << 24) | ((uint32_t)info_frame[1] << 16) | ((uint32_t)info_frame[2] << 8) | ((uint32_t)info_frame[3]);
    host_size = ((uint32_t)info_frame[4] << 24) | ((uint32_t)info_frame[5] << 16) | ((uint32_t)info_frame[6] << 8) | ((uint32_t)info_frame[7]);
    host_crc = ((uint32_t)info_frame[8] << 24) | ((uint32_t)info_frame[9] << 16) | ((uint32_t)info_frame[10] << 8) | ((uint32_t)info_frame[11]);
    host_version = ((uint32_t)info_frame[12] << 24) | ((uint32_t)info_frame[13] << 16) | ((uint32_t)info_frame[14] << 8) | ((uint32_t)info_frame[15]);

    size_appA_backup = (uint32_t)metadata->size__appA;
    size_appB_backup = (uint32_t)metadata->size__appB;
    crc_appA_backup = (uint32_t)metadata->crc_hostappA;
    crc_appB_backup = (uint32_t)metadata->crc_hostappB;
    version_appA_backup = ((uint32_t)metadata->versionA);
    version_appB_backup = (uint32_t)metadata->versionB;

    cfg.TypeErase = FLASH_TYPEERASE_SECTORS;
    cfg.VoltageRange = FLASH_VOLTAGE_RANGE_4;
    cfg.Sector = 2;
    cfg.NbSectors = 1;

    HAL_FLASH_Unlock();


    switch (metadata->active_bank)
    {
        case ACTIVE_A:
            HAL_FLASHEx_Erase(&cfg, SectorErr);
            metadata->size__appA = size_appA_backup;
        break;
    
        case ACTIVE_B:
            HAL_FLASHEx_Erase(&cfg, SectorErr);
        break;
    }

}
void BOOTLOADER_ReceiveFirmWare(void)
{
    uint8_t data[128] = {0};
    METADATA_Sector_t *metadata = (METADATA_Sector_t*)METADATA_START_ADDR;
    uint8_t Nboftransfer = 0;
    HAL_StatusTypeDef erase_status;
    uint32_t SectorErr;
    FLASH_EraseInitTypeDef cfg;
    cfg.TypeErase = FLASH_TYPEERASE_SECTORS;
    cfg.VoltageRange = FLASH_VOLTAGE_RANGE_3;


    HAL_FLASH_Unlock();

    switch (metadata->active_bank)
    {
        case ACTIVE_A:
            /* Erase firmware B */
            cfg.Sector = 6;
            cfg.NbSectors = 1;
            erase_status = HAL_FLASHEx_Erase(&cfg, &SectorErr);
            if(erase_status != HAL_OK)
            {
                return BOOOTLOADER_ERASE_FAIL;
            }

            mPrintf("START\n");

            Nboftransfer = metadata->size__appA / 4;
            if(metadata->size__appA % 4)
            {
                Nboftransfer++;
            }

            for(uint8_t i = 0; i < Nboftransfer; i++)
            {
                HAL_UART_Receive(&huart1, data, 128, 1000);
                for(uint8_t j; j < 128; j++)
                {
                    HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE, (uint32_t*)(APP_B_START_ADDR + j + (128*i)), data[j]);
                }
            }
        break;

        case ACTIVE_B:
            /* Erase firmware A */
            cfg.Sector = 3;
            cfg.NbSectors = 3;
            erase_status = HAL_FLASHEx_Erase(&cfg, &SectorErr);
            if(erase_status != HAL_OK)
            {
                return BOOOTLOADER_ERASE_FAIL;
            }    
            
            mPrintf("START\n");

            Nboftransfer = metadata->size__appB / 4;
            if(metadata->size__appB % 4)
            {
                Nboftransfer++;
            }

            for(uint8_t i = 0; i < Nboftransfer; i++)
            {
                HAL_UART_Receive(&huart1, data, 128, 1000);
                for(uint8_t j; j < 128; j++)
                {
                    HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE, (uint32_t*)(APP_A_START_ADDR + j + (128*i)), data[j]);
                }
            }
        break;
    }
    mPrintf("DONE\n");
    HAL_FLASH_Lock();
}



void BOOTLOADER_WaitForCmd(void)
{
    uint8_t cmd;
    BOOTLOADER_Status_Typedef status;
    HAL_UART_Receive(&huart1, &cmd, 1, HAL_MAX_DELAY);

    switch(cmd)
    {
        case BOOTLOADER_WRITE_CMD:
        mPrintf("Waiting for new firmware!\n");
        BOOTLOADER_ReceiveInfoFrame();
        break;

        case BOOTLOADER_ERASE_CMD:
        mPrintf("Erasing current Firmware...\n");
        status = BOOTLOADER_EraseCurrentFirmware();
        if(status == BOOTLOADER_OK)
        {
        mPrintf("Erase Finished!\n");
        }
        else
        {
            mPrintf("bootloader fail to erase sectors!\n");
        }
        break;

        default:
        mPrintf("Unknow command!\n");
        break;
    };

}


void JUMP_To_Boot(void)
{
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_SET);
    mPrintf("Enter Bootloader mode!\n");
    SET_PendingFlag(1);
    BOOTLOADER_WaitForCmd();
}


void BOOTLOADER_InitMode(void)
{
    METADATA_Sector_t app_status = *(METADATA_Sector_t*)METADATA_START_ADDR;
    uint32_t crc_appA = HAL_CRC_Calculate(&hcrc, (uint32_t*)APP_A_START_ADDR, (app_status.size__appA / 4));
    uint32_t crc_appB = HAL_CRC_Calculate(&hcrc, (uint32_t*)APP_B_START_ADDR, (app_status.size__appB / 4));
    mPrintf("crc_A: %.02x", crc_appA);
    if(!(HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0)))
    {
        JUMP_To_Boot();
    }
    else
    {
        switch(app_status.active_bank)
        {
            case ACTIVE_A:
                if(app_status.crc_hostappA == crc_appA && app_status.pending_bank == 0 && app_status.size__appA > 0)
                {
                    
                    JUMP_To_App_A();
                }
                else
                {
                    
                    JUMP_To_App_B();
                }             
            break;

            case ACTIVE_B:
                if(app_status.crc_hostappB == crc_appB && app_status.pending_bank == 0 && app_status.size__appB > 0)
                {
                    
                    JUMP_To_App_B();
                }
                else 
                {
                    
                    JUMP_To_App_A();
                }           
            break;

        };

    }
}
