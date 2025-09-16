#ifndef ZSSC4151_H_ // Start of ZSSC4151_H_ 
#define ZSSC4151_H_

//==============================================================================
// Includes
//==============================================================================

//==============================================================================
// ��� ���� (Defines)
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

// --- ZSSC4151 RAM �ּ� ���� ---
#define ZSSC_RAM_ADDR_BRIDGE_RESULT 0x41

// --- ���� ��� (Calibration Constants) ---
// SENSOR_15BIT_MIN: ���� ����� 0%�� ���� 15��Ʈ Raw �� (��ȣ �ִ� ������ ó��)
// SENSOR_15BIT_MAX: ���� ����� 4%�� ���� 15��Ʈ Raw �� (��ȣ �ִ� ������ ó��)
#define SENSOR_15BIT_MIN  2000   // ����: 0%�� ���� 15��Ʈ �� (���� ���� �ʿ�!)
#define SENSOR_15BIT_MAX  30000  // 4%�� �ش��ϴ� ������ �ִ� ��°� (���� ��û �ӽð�)

//==============================================================================
// ����ü �� ������ ���� (Typedefs)
//==============================================================================

// ZSSC4151 operation mode
typedef enum
{
  ZSSC_MODE_COMMAND,        // Command Mode 
  ZSSC_MODE_NORMAL             // Normal Mode 
} ZSSC4151_Mode_t;

//==============================================================================
// �ܺ� �Լ� ���� (Function Prototypes)
//==============================================================================

/**
 * @brief zSSC4151 ������ �ʱ�ȭ�ϴ� �Լ�
 */
//void ZSSC4151_Initialize(void);

/**
 * @brief �����κ��� �з� ���� �о���� �Լ�
 * @return 16��Ʈ �з� ������
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
 * @brief ���� ������ ���¸� ��ȯ�ϴ� �Լ�
 * @return SensorStatus_t ������ ��
 */
void ZSSC4151_Check_Status (void);
int ZSSC4151_Clear_Status (void);

#endif // End of ZSSC4151_H_ 




