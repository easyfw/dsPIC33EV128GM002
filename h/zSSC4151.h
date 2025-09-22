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
#define CMD_START_NOM                   0x01 // ??? ???? ??? ???? (StrtNom)
#define CMD_READ_OUTPUT_MEM     0x2C // ?? ???: Read Output Memory (??????)
#define CMD_RD_OUT_MEM_BURST    0x2E // Read Output Memory in Burst Mode

#define CMD_STRT_MEAS_TASK        0x09 // Start Measurement Task
#define CMD_CP_NVM_TO_SHDW      0x18
#define CMD_STRT_MEAS_CYC         0x0B    // 
#define CMD_RUN_COND_CYC          0x0E    //
#define CMD_RD_IC_STATUS             0x60    // 
#define CMD_RD_FAILURE_STATUS   0x62    // 

// --- ZSSC4151 RAM 주소 정의 ---
#define ZSSC_RAM_ADDR_BRIDGE_RESULT 0x41

// --- 교정 상수 (Calibration Constants) ---
// SENSOR_15BIT_MIN: 센서 출력이 0%일 때의 15비트 Raw 값 (부호 있는 정수로 처리)
// SENSOR_15BIT_MAX: 센서 출력이 4%일 때의 15비트 Raw 값 (부호 있는 정수로 처리)
#define SENSOR_15BIT_MIN  2000   // 예시: 0%일 때의 15비트 값 (실제 교정 필요!)
#define SENSOR_15BIT_MAX  30000  // 4%에 해당하는 센서의 최대 출력값 (고객사 요청 임시값)

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

bool ZSSC4151_Probe_CM (void);

int ZSSC4151_EnterCommandMode (void);
int ZSSC4151_Start_Normal_Mode (void);

int ZSSC4151_Unlock_NVM (void);
int ZSSC4151_WriteNvmWord (uint8_t nvm_addr, uint16_t w);
int ZSSC4151_ReadNvmWord (uint8_t nvm_addr, uint16_t *read_data);
int ZSSC4151_Read_Command_Response32 (uint8_t command, uint32_t *response_data);

int ZSSC4151_CopyNvmToShadow(void);
int ZSSC4151_StartMeaTask(uint8_t taskNum, uint8_t repeats, uint8_t avgFactor);
int ZSSC4151_ReadRamBurst(uint8_t startAddr, uint8_t wordCount, uint8_t* readBuffer);
int Get_ZSSC4151_BridgeRaw(int16_t *raw_value);
int ZSSC4151_Read_Ram_In_Normal_Mode(uint8_t address, uint8_t word_count, uint8_t* buffer);
int ZSSC4151_ReadRam_Corrected(uint8_t address, uint8_t word_count, uint8_t* buffer);
int ZSSC4151_StartMeasCycle(void);
int ZSSC4151_RunConditioningCycle(void);
void ZSSC4151_Check_Status_Registers(void);

/**
 * @brief 현재 센서의 상태를 반환하는 함수
 * @return SensorStatus_t 열거형 값
 */
void ZSSC4151_Check_Status (void);
int ZSSC4151_Clear_Status (void);

#endif // End of ZSSC4151_H_ 




