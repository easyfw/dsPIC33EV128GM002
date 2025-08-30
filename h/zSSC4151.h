#ifndef ZSSC4151_H_ // Start of ZSSC4151_H_ 
#define ZSSC4151_H_

//==============================================================================
// Includes
//==============================================================================

//==============================================================================
// 상수 정의 (Defines)
//==============================================================================

#define ZSSC4151_I2C_ADDR         0x28
#define ZSSC4151_WRITE_ADDR (ZSSC4151_I2C_ADDR << 1)
#define ZSSC4151_READ_ADDR  ((ZSSC4151_I2C_ADDR << 1) | 1)

// --- ZSSC4151 Commands (???????? ??? ??????) ---
#define CMD_START_CM            0x72 // (????) Command Mode + ? F5 A2
#define CM_AUTH_KEY_MSB         0xF5
#define CM_AUTH_KEY_LSB         0xA2

#define CMD_READ_NVM_WORD       0x24 // ?? ????: Read NVM Word (App Desc. Table 5.3 ??? ????)
#define CMD_START_NOM             0x01 // ??? ???? ??? ???? (StrtNom)
#define CMD_READ_OUTPUT_MEM     0x2C // ?? ???: Read Output Memory (??????)

//==============================================================================
// 구조체 및 열거형 정의 (Typedefs)
//==============================================================================

// ZSSC4151 operation mode
typedef enum
{
  ZSSC_MODE_COMMAND,        // Command Mode 
  ZSSC_MODE_NORMAL             // Normal Mode 
} ZSSC4151_Mode_t;

//==============================================================================
// 외부 함수 선언 (Function Prototypes)
//==============================================================================

/**
 * @brief zSSC4151 센서를 초기화하는 함수
 */
//void ZSSC4151_Initialize(void);

/**
 * @brief 센서로부터 압력 값을 읽어오는 함수
 * @return 16비트 압력 데이터
 */

void Crc16_update (uint16_t *crc, uint8_t data);
uint16_t Calculate_ZSSC_CRC (const uint16_t *nvm_data);

void Dump_NVM_Map (void);
bool ZSSC4151_Probe_CM (void);

int ZSSC4151_EnterCommandMode (void);
int ZSSC4151_Start_Normal_Mode (void);

bool ZSSC4151_ReadNvm (uint8_t nvm_addr, uint16_t *value);
int ZSSC4151_Write_NVM (unsigned char reg_addr, unsigned int data);
int ZSSC4151_Unlock_NVM (void);
int ZSSC4151_WriteNvmWord (uint8_t nvm_addr, uint16_t w);
int ZSSC4151_ReadNvmWord (uint8_t nvm_addr, uint16_t *read_data);
int ZSSC4151_Read_Output_Data (uint8_t ram_addr, uint16_t *read_data);
int ZSSC4151_Read_Command_Response32 (uint8_t command, uint32_t *response_data);

/**
 * @brief 현재 센서의 상태를 반환하는 함수
 * @return SensorStatus_t 열거형 값
 */
void ZSSC4151_Check_Status (void);
int ZSSC4151_Clear_Status (void);

#endif // End of ZSSC4151_H_ 




