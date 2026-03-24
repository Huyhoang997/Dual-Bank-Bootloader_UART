#include "boot.h"


static void JUMP_To_AppA(void)
{
  volatile uint32_t app_A_vector_table_addr = *(volatile uint32_t*)APP_A_START_ADDR;
  volatile uint32_t app_A_reset_handler_addr = *(volatile uint32_t*)APP_A_RESET_HANDLER_ADDR;
  __disable_irq();
  HAL_DeInit();

  __set_MSP(app_A_vector_table_addr);
  SCB->VTOR = APP_A_START_ADDR;
  void (*FuncPtr)() = (void*)app_A_reset_handler_addr;
  __enable_irq();
  FuncPtr();
}

static void JUMP_To_AppB(void)
{
  volatile uint32_t app_B_vector_table_addr = *(volatile uint32_t*)APP_B_START_ADDR;
  volatile uint32_t app_B_reset_handler_addr = *(volatile uint32_t*)APP_B_RESET_HANDLER_ADDR;
  __disable_irq();
  HAL_DeInit();
  __set_MSP(app_B_vector_table_addr);
  SCB->VTOR = APP_B_START_ADDR;
  void (*FuncPtr)() = (void*)app_B_reset_handler_addr;
  __enable_irq();
  FuncPtr();
}


/* Set active bank in metadata sector */
static BOOTLOADER_Status_Typedef BOOTLOADER_SetActiveBank(METADATA_SetActive active_bank)
{
	/* Check if invalid banks */
    if(active_bank < ACTIVE_APP_A || active_bank > ACTIVE_APP_B)
    {
        return BOOTLOADER_INVALID_ACTIVE_APP;
    }

    uint32_t metadata_backup[9];
    FLASH_EraseInitTypeDef cfg;
    uint32_t SectorErr = 0;
    HAL_StatusTypeDef status;
    cfg.TypeErase = FLASH_TYPEERASE_SECTORS;
    cfg.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    cfg.Sector = 2;
    cfg.NbSectors = 1;

    /* Save context and data before erase full sector */
    METADATA_SecDef_t *host_metadata = (METADATA_SecDef_t*)METADATA_START_ADDR;
    metadata_backup[0] = host_metadata->header;
    metadata_backup[1] = host_metadata->Host_sizeAppA;
    metadata_backup[2] = host_metadata->Host_crcappA;
    metadata_backup[3] = host_metadata->Host_versionAppA;
    metadata_backup[4] = host_metadata->Host_sizeAppB;
    metadata_backup[5] = host_metadata->Host_crcappB;
    metadata_backup[6] = host_metadata->Host_versionAppB;
    metadata_backup[7] = active_bank;
    metadata_backup[8] = host_metadata->pending_flag;

    HAL_FLASH_Unlock();

    status = HAL_FLASHEx_Erase(&cfg, &SectorErr);
    if(status != HAL_OK)
    {
        return BOOTLOADER_ERASE_ERR;
    }

    for(uint8_t i = 0; i < 9; i++)
    {
    	/* Restore back up data for metadata sector */
        status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, (uint32_t)(METADATA_START_ADDR + 4*i), metadata_backup[i]);
        if(status != HAL_OK)
        {
            return BOOTLOADER_PROGRAM_ERR;
        }
    }
    HAL_FLASH_Lock();

    return BOOTLOADER_OK;
}


/* Set pending bank function */
static BOOTLOADER_Status_Typedef BOOTLOADER_PendingFlag(uint8_t flag)
{
	/* Check if invalid pending flag*/
    if(flag > 1)
    {
        return BOOTLOADER_INVALID_PENDING_FLAG;
    }

    uint32_t metadata_backup[9];
    FLASH_EraseInitTypeDef cfg;
    uint32_t SectorErr = 0;
    HAL_StatusTypeDef status;
    cfg.TypeErase = FLASH_TYPEERASE_SECTORS;
    cfg.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    cfg.Sector = 2;
    cfg.NbSectors = 1;

    /* Save context and data before erase full sector */
    METADATA_SecDef_t *host_metadata = (METADATA_SecDef_t*)METADATA_START_ADDR;
    metadata_backup[0] = host_metadata->header;
    metadata_backup[1] = host_metadata->Host_sizeAppA;
    metadata_backup[2] = host_metadata->Host_crcappA;
    metadata_backup[3] = host_metadata->Host_versionAppA;
    metadata_backup[4] = host_metadata->Host_sizeAppB;
    metadata_backup[5] = host_metadata->Host_crcappB;
    metadata_backup[6] = host_metadata->Host_versionAppB;
    metadata_backup[7] = host_metadata->active_bank;
    metadata_backup[8] = flag;

    HAL_FLASH_Unlock();

    status = HAL_FLASHEx_Erase(&cfg, &SectorErr);
    if(status != HAL_OK)
    {
        return BOOTLOADER_ERASE_ERR;
    }

    for(uint8_t i = 0; i < 9; i++)
    {
    	/* Restore back up data for metadata sector */
        status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, (uint32_t)(METADATA_START_ADDR + 4*i), metadata_backup[i]);
        if(status != HAL_OK)
        {
            return BOOTLOADER_PROGRAM_ERR;
        }
    }
    HAL_FLASH_Lock();

    return BOOTLOADER_OK;
}


/* Erase current firmware and roll back to older firmware version */
static BOOTLOADER_Status_Typedef BOOTLOADER_EraseCurrentFirmware(void)
{
    FLASH_EraseInitTypeDef cfg;
    uint32_t SectorErr = 0;
    HAL_StatusTypeDef erase_status;
    cfg.TypeErase = FLASH_TYPEERASE_SECTORS;
    cfg.VoltageRange = FLASH_VOLTAGE_RANGE_3;

    METADATA_SecDef_t *metadata = (METADATA_SecDef_t*)METADATA_START_ADDR;
    HAL_FLASH_Unlock();
    
    if(metadata->active_bank == ACTIVE_APP_A)
    {
        cfg.Sector = 3;
        cfg.NbSectors = 3;
        BOOTLOADER_SetActiveBank(ACTIVE_APP_B);
    }
    else if(metadata->active_bank == ACTIVE_APP_B)
    {
        cfg.Sector = 6;
        cfg.NbSectors = 1;
        BOOTLOADER_SetActiveBank(ACTIVE_APP_A);

    }
    /* Delay to allow flash to stabilize before next erase */
    HAL_Delay(10);
    erase_status = HAL_FLASHEx_Erase(&cfg, &SectorErr);
    if(erase_status != HAL_OK)
    {
        return BOOTLOADER_ERASE_ERR;
    }


    return BOOTLOADER_OK;
}


/* Receive information frame from host (HEADER | SIZE | CRC | VERSION)*/
static BOOTLOADER_Status_Typedef BOOTLOADER_ReceiveInfoFrame(void)
{
    uint32_t host_header, host_size, host_crc, host_version;
    uint32_t metadata_backup[6];
    METADATA_SecDef_t *metadata = (METADATA_SecDef_t *)METADATA_START_ADDR;
    uint8_t info_frame[16];

    FLASH_EraseInitTypeDef cfg;
    uint32_t SectorErr = 0;
    HAL_StatusTypeDef erase_status;
    cfg.TypeErase = FLASH_TYPEERASE_SECTORS;
    cfg.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    cfg.Sector = 2;
    cfg.NbSectors = 1;

    HAL_UART_Receive(&huart1, info_frame, 16, HAL_MAX_DELAY);

    host_header = ((uint32_t)info_frame[0] << 24) | ((uint32_t)info_frame[1] << 16) | ((uint32_t)info_frame[2] << 8) | ((uint32_t)info_frame[3]); 
    host_size = ((uint32_t)info_frame[4] << 24) | ((uint32_t)info_frame[5] << 16) | ((uint32_t)info_frame[6] << 8) | ((uint32_t)info_frame[7]); 
    host_crc = ((uint32_t)info_frame[8] << 24) | ((uint32_t)info_frame[9] << 16) | ((uint32_t)info_frame[10] << 8) | ((uint32_t)info_frame[11]); 
    host_version = ((uint32_t)info_frame[12] << 24) | ((uint32_t)info_frame[13] << 16) | ((uint32_t)info_frame[14] << 8) | ((uint32_t)info_frame[15]);

    if(host_header != 0x55AA)
    {
        return BOOTLOADER_INVALID_HEADER;
    }

    HAL_FLASH_Unlock();

    /* Write new firmware information to metadata sector */
    switch (metadata->active_bank)
    {
        case ACTIVE_APP_A:
            /* Save context and data before erase full sector */
            metadata_backup[0] = metadata->header;
            metadata_backup[1] = metadata->Host_sizeAppA;
            metadata_backup[2] = metadata->Host_crcappA;
            metadata_backup[3] = metadata->Host_versionAppA;
            metadata_backup[4] = metadata->active_bank;
            metadata_backup[5] = metadata->pending_flag;

            erase_status = HAL_FLASHEx_Erase(&cfg, &SectorErr);
            if(erase_status != HAL_OK)
            {
                return BOOTLOADER_ERASE_ERR;
            }

            HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, (uint32_t)(METADATA_START_ADDR), metadata_backup[0]);
            HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, (uint32_t)(METADATA_START_ADDR + 4), metadata_backup[1]);
            HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, (uint32_t)(METADATA_START_ADDR + 8), metadata_backup[2]);
            HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, (uint32_t)(METADATA_START_ADDR + 12), metadata_backup[3]);
            HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, (uint32_t)(METADATA_START_ADDR + 16), host_size);
            HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, (uint32_t)(METADATA_START_ADDR + 20), host_crc);
            HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, (uint32_t)(METADATA_START_ADDR + 24), host_version);
            HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, (uint32_t)(METADATA_START_ADDR + 28), metadata_backup[4]);
            HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, (uint32_t)(METADATA_START_ADDR + 32), metadata_backup[5]);
        break;


        case ACTIVE_APP_B:
            /* Save context and data before erase full sector */
            metadata_backup[0] = metadata->header;
            metadata_backup[1] = metadata->Host_sizeAppB;
            metadata_backup[2] = metadata->Host_crcappB;
            metadata_backup[3] = metadata->Host_versionAppB;
            metadata_backup[4] = metadata->active_bank;
            metadata_backup[5] = metadata->pending_flag;

            erase_status = HAL_FLASHEx_Erase(&cfg, &SectorErr);
            if(erase_status != HAL_OK)
            {
                return BOOTLOADER_ERASE_ERR;
            }
            HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, (uint32_t)(METADATA_START_ADDR), metadata_backup[0]);
            HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, (uint32_t)(METADATA_START_ADDR + 4), host_size);
            HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, (uint32_t)(METADATA_START_ADDR + 8), host_crc);
            HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, (uint32_t)(METADATA_START_ADDR + 12), host_version);
            HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, (uint32_t)(METADATA_START_ADDR + 16), metadata_backup[1]);
            HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, (uint32_t)(METADATA_START_ADDR + 20), metadata_backup[2]);
            HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, (uint32_t)(METADATA_START_ADDR + 24), metadata_backup[3]);
            HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, (uint32_t)(METADATA_START_ADDR + 28), metadata_backup[4]);
            HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, (uint32_t)(METADATA_START_ADDR + 32), metadata_backup[5]);
        break;
    }

    HAL_FLASH_Lock();
    
    return BOOTLOADER_OK;
}


/* Function to verify new firmware by CRC32 and set pending flag */
static BOOTLOADER_Status_Typedef BOOTLOADER_VerifyFirmware(void)
{
    METADATA_SecDef_t *metadata = (METADATA_SecDef_t *)METADATA_START_ADDR;
    BOOTLOADER_Status_Typedef status;
    uint32_t length_word = 0;
    switch (metadata->active_bank)
    {
    case ACTIVE_APP_A:
        /* Check if size is odd then +1 at last */
    	length_word = metadata->Host_sizeAppB / 4;
        if(metadata->Host_sizeAppB % 4)
        {
            length_word++;
        }
        uint32_t crc_newAppB = HAL_CRC_Calculate(&hcrc, (uint32_t*)APP_B_START_ADDR, length_word);
        uint32_t crc_hostB = metadata->Host_crcappB;
        if(crc_hostB == crc_newAppB)
        {
            status = BOOTLOADER_PendingFlag(1);
            if(status != BOOTLOADER_OK)
            {
                return status;
            }
            mPrintf("New firmware available!\nPress reset to update\n");  
        }
        else
        {
        	return BOOTLOADER_VERIFY_CRC_FAIL;
        }
    break;
    

    case ACTIVE_APP_B:
        /* Check if size is odd then +1 at last */
        length_word = metadata->Host_sizeAppA / 4;
        if(metadata->Host_sizeAppA % 4)
        {
            length_word++;
        }
        uint32_t crc_newAppA = HAL_CRC_Calculate(&hcrc, (uint32_t*)APP_A_START_ADDR, length_word);
        uint32_t crc_hostA = metadata->Host_crcappA;
        if(crc_hostA == crc_newAppA)
        {
            status = BOOTLOADER_PendingFlag(1);
            if(status != BOOTLOADER_OK)
            {
                return status;
            }
            mPrintf("New firmware available!\nPress reset to update\n"); 
        }
        else
        {
        	return BOOTLOADER_VERIFY_CRC_FAIL;
        }
    break;

    default:
        mPrintf("Can't verify new firmware!\n");
        break;
    }

    return BOOTLOADER_OK;
}


/* Function to receive new firmware data and write to new available sector */
static BOOTLOADER_Status_Typedef BOOTLOADER_ReceiveFirmware(void)
{
    uint8_t data[128] = {0};
    uint8_t Nboftransfer = 0;
    METADATA_SecDef_t *metadata = (METADATA_SecDef_t *)METADATA_START_ADDR;

    FLASH_EraseInitTypeDef cfg;
    uint32_t SectorErr = 0;
    HAL_StatusTypeDef status;
    BOOTLOADER_Status_Typedef metadata_status;
    cfg.TypeErase = FLASH_TYPEERASE_SECTORS;
    cfg.VoltageRange = FLASH_VOLTAGE_RANGE_3;

    
    switch (metadata->active_bank)
    {
    case ACTIVE_APP_A:
        cfg.Sector = 6;
        cfg.NbSectors = 1;
        Nboftransfer = metadata->Host_sizeAppB / 128;
        if(metadata->Host_sizeAppB % 128)
        {
            Nboftransfer++;
        }

        HAL_FLASH_Unlock();

        status = HAL_FLASHEx_Erase(&cfg, &SectorErr);
        if(status != HAL_OK)
        {
            return BOOTLOADER_ERASE_ERR;
        }
        /* Send START signal */
        mPrintf("START\n");
        for(uint32_t i = 0; i < Nboftransfer; i++)
        {
        	/* Received 128 byte/1 packet */
            HAL_UART_Receive(&huart1, data, 128, HAL_MAX_DELAY);
            for(uint8_t j = 0; j < 128; j++)
            {
            	/* FLASH write one by one byte to available sector */
            	status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE, (uint32_t)(APP_B_START_ADDR + j + (i*128)), data[j]);
            }
            if(status != HAL_OK)
            {
                return BOOTLOADER_PROGRAM_ERR;
            }
            /* After received 1 packet = 128byte then send ACK signal */
            mPrintf("ACK\n");
        }
        HAL_FLASH_Lock();
        /* After received full firmware then send DONE signal */
        mPrintf("DONE\n");
    break;
    

    case ACTIVE_APP_B:
        cfg.Sector = 3;
        cfg.NbSectors = 3;
        Nboftransfer = metadata->Host_sizeAppA / 128;
        if(metadata->Host_sizeAppA % 128)
        {
            Nboftransfer++;
        }

        HAL_FLASH_Unlock();

        status = HAL_FLASHEx_Erase(&cfg, &SectorErr);
        if(status != HAL_OK)
        {
            return BOOTLOADER_ERASE_ERR;
        }
        /* Send START signal */
        mPrintf("START\n");
        for(uint32_t i = 0; i < Nboftransfer; i++)
        {
        	/* Received 128 byte/1 packet */
            HAL_UART_Receive(&huart1, data, 128, HAL_MAX_DELAY);
            for(uint8_t j = 0; j < 128; j++)
            {
            	/* FLASH write one by one byte to available sector */
            	status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE, (uint32_t)(APP_A_START_ADDR + j + (i*128)), data[j]);
            	if(status != HAL_OK)
            	{
            		return BOOTLOADER_PROGRAM_ERR;
            	}

            }
            /* After received 1 packet = 128byte then send ACK signal */
            mPrintf("ACK\n");
        }
        /* After received full firmware then send DONE signal */
        HAL_FLASH_Lock();
        mPrintf("DONE\n");
    break;

    default:
    	/* Check if fail to receive firmware */
        mPrintf("Update firmware error!\n");
        break;
    }

    metadata_status = BOOTLOADER_VerifyFirmware();
    if(metadata_status != BOOTLOADER_OK)
    {
        return metadata_status;
    }

    return BOOTLOADER_OK;
}


/* Receive command function */
static void BOOTLOADER_WaitCmd()
{
    uint8_t cmd = 0;
    BOOTLOADER_Status_Typedef cmd_status;
    HAL_UART_Receive(&huart1, &cmd, 1, HAL_MAX_DELAY);

    switch (cmd)
    {
    /* Detect user's new firmware update request */
    case BOOTLOADER_CMD_WRITE:
        mPrintf("Waiting for new firmware...\n");
        cmd_status = BOOTLOADER_ReceiveInfoFrame();
        if(cmd_status != BOOTLOADER_OK)
        {
            mPrintf("cmd error id: %.02x\n", cmd_status);
            break;
        }

        cmd_status = BOOTLOADER_ReceiveFirmware();
        if(cmd_status != BOOTLOADER_OK)
        {
            mPrintf("Failed to update firmware!\n");
            break;
        }
    break;

    /* Detect user's erase current firmware and roll back to older firmware version request */
    case BOOTLOADER_CMD_ERASE:
        mPrintf("Erasing current firmware!\n");
        cmd_status = BOOTLOADER_EraseCurrentFirmware();
        if(cmd_status == BOOTLOADER_OK)
        {
            mPrintf("Erase finished!\n");
            mPrintf("Roll back to older version!\n");
        }
        else
        {
            mPrintf("Erase failed!\n");

        }
    break;
    
    /* Check if user's invalid command */
    default:
        mPrintf("Unknow command!\n");
    break;
    }
}


static void JUMP_To_Boot(void)
{
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_SET);
    mPrintf("Enter Bootloader mode!\n");
    BOOTLOADER_WaitCmd();
}



void BOOTLOADER_Init(void)
{

    METADATA_SecDef_t *metadata_cfg = (METADATA_SecDef_t*)METADATA_START_ADDR;
    uint32_t length = 0;
    /* Check if enter bootloader mode */
    if(!(HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0)))
    {
        JUMP_To_Boot();
    }
    else
    {
    	/* Check if jump to application banks */
        switch (metadata_cfg->active_bank)
        {
        case ACTIVE_APP_A:
            length = metadata_cfg->Host_sizeAppB / 4;
            if(metadata_cfg->Host_sizeAppB % 4)
            {
                length++;
            }
            uint32_t crc_newAppB = HAL_CRC_Calculate(&hcrc, (uint32_t*)APP_B_START_ADDR, length);

            mPrintf("CRC_Host: %d\n", metadata_cfg->Host_crcappB);
            mPrintf("Real_appB_CRC: %d\n", crc_newAppB);

            if(metadata_cfg->pending_flag == 1 && metadata_cfg->Host_crcappB == crc_newAppB)
            {

                BOOTLOADER_SetActiveBank(ACTIVE_APP_B);
                HAL_Delay(10);
                BOOTLOADER_PendingFlag(0);
                HAL_Delay(10);
                mPrintf("New firmware deteced!\nJump to Application B!\n");
                JUMP_To_AppB();
            }
            else
            {
            	mPrintf("CRC mismatch!\n");
                mPrintf("Current firmware application: %.X\n", metadata_cfg->active_bank);
                JUMP_To_AppA();
            }
        break;


        case ACTIVE_APP_B:
            length = metadata_cfg->Host_sizeAppA / 4;
            if(metadata_cfg->Host_sizeAppA % 4)
            {
                length++;
            }
            uint32_t crc_newAppA = HAL_CRC_Calculate(&hcrc, (uint32_t*)APP_A_START_ADDR, length);

            mPrintf("CRC_Host: %d\n", metadata_cfg->Host_crcappA);
            mPrintf("Real_appA_CRC: %d\n", crc_newAppA);
            if(metadata_cfg->pending_flag == 1 && metadata_cfg->Host_crcappA == crc_newAppA)
            {

                BOOTLOADER_SetActiveBank(ACTIVE_APP_A);
                HAL_Delay(10);
                BOOTLOADER_PendingFlag(0);
                HAL_Delay(10);
                mPrintf("New firmware deteced!\nJump to Application A!\n");
                JUMP_To_AppA();
            }
            else
            {
            	mPrintf("CRC mismatch!\n");
                mPrintf("Current firmware application: %.x\n", metadata_cfg->active_bank);
                JUMP_To_AppB();
            }
        break;

        default:
            mPrintf("No firmware available!\n");
        break;
        }
    }
}
