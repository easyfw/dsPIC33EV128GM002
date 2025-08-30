int ZSSC4151_EnterCommandMode (void);
int ZSSC4151_Start_Normal_Mode (void);

bool ZSSC4151_ReadNvm (uint8_t nvm_addr, uint16_t *value);
int ZSSC4151_Write_NVM (unsigned char reg_addr, unsigned int data);
int ZSSC4151_Unlock_NVM (void);
int ZSSC4151_WriteNvmWord (uint8_t nvm_addr, uint16_t w);
int ZSSC4151_ReadNvmWord (uint8_t nvm_addr, uint16_t *read_data);
int ZSSC4151_Read_Output_Data (uint8_t ram_addr, uint16_t *read_data);
int ZSSC4151_Read_Command_Response32 (uint8_t command, uint32_t *response_data);
void ZSSC4151_Check_Status (void);
int ZSSC4151_Clear_Status (void);
