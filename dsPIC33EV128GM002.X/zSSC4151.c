/*
 * File:   ZSSC4151.c
 * Author: easyf
 *
 * Created on August 30, 2025, 4:38 PM
 */
#include <stdbool.h>
#include <stdint.h> 
#include "..\h\zSSC4151.h"

extern void dbg_putc (char c);
extern void dbg_put_hex_byte (uint8_t val);
extern void dbg_put_hex_word (uint16_t val);
extern void dbg_put_float (float temp);
extern void dbg_put_string (char *s);

// I2C
extern void I2C1_Init (void);
extern bool I2C1_Start (void);
extern bool I2C1_Stop (void);
extern bool I2C1_Restart (void);
extern bool I2C1_Write (uint8_t data);
extern uint8_t I2C1_Read (bool send_ack);
extern void I2C1_WaitForIdle (void);
extern void I2C1_Ack (void);
extern void I2C1_Nack (void);
extern void I2C_Scan (void);
extern int I2C1_SendByte (unsigned char data);

extern void delay_10ms (uint16_t num);
extern void delay_us (unsigned int us);

// ?????? ZSSC4151_Probe_CM ???
bool ZSSC4151_Probe_CM (void)
{
    uint8_t rx_buffer[4];

    dbg_put_string ("  Probe Step 1: Start\r\n");
    if (!I2C1_Start ())
    {
        dbg_put_string ("  -> FAILED\r\n");
        return false;
    }

    dbg_put_string ("  Probe Step 2: Write Address\r\n");
    if (!I2C1_Write (ZSSC4151_WRITE_ADDR))
    {
        dbg_put_string ("  -> FAILED (NACK or Timeout)\r\n");
        I2C1_Stop ();
        return false;
    }

    dbg_put_string ("  Probe Step 3: Write Command\r\n");
    if (!I2C1_Write (CMD_READ_OUTPUT_MEM))
    {
        dbg_put_string ("  -> FAILED (NACK or Timeout)\r\n");
        I2C1_Stop ();
        return false;
    }

  /*
  dbg_put_string("  Probe Step 4: Write Parameter\r\n");
  if (!I2C1_Write(0x17)) {
      dbg_put_string("  -> FAILED (NACK or Timeout)\r\n");
      I2C1_Stop();
      return false;
  }
   */

    dbg_put_string ("  Probe Step 5: Restart\r\n");
    if (!I2C1_Restart ())
    {
        dbg_put_string ("  -> FAILED\r\n");
        I2C1_Stop ();
        return false;
    }

    dbg_put_string ("  Probe Step 6: Write Read Address\r\n");
    if (!I2C1_Write (ZSSC4151_READ_ADDR))
    {
        dbg_put_string ("  -> FAILED (NACK or Timeout)\r\n");
        I2C1_Stop ();
        return false;
    }

    dbg_put_string ("  Probe Step 7: Reading bytes...\r\n");
    rx_buffer[0] = I2C1_Read (true);
    rx_buffer[1] = I2C1_Read (true);
    rx_buffer[2] = I2C1_Read (true);
    rx_buffer[3] = I2C1_Read (false);
    
    I2C1_Stop ();

    dbg_put_string ("  Probe RX Raw: ");
    for (int i = 0; i < 4; i++)
    {
        dbg_put_hex_byte (rx_buffer[i]);
        dbg_putc (' ');
    }
    dbg_put_string ("\r\n");

    for (int i = 0; i < 4; i++)
    {
        if (rx_buffer[i] == CMD_READ_OUTPUT_MEM)
        {
            dbg_put_string ("  -> Probe OK!\r\n");
            return true;
        }
    }

    dbg_put_string ("  -> FAILED (Echo mismatch)\r\n");
    return false;
}

//
void Crc16_update (uint16_t *crc, uint8_t data)
{
    *crc = *crc ^ ((uint16_t) data << 8);

    for (int i = 0; i < 8; i++)
    {
        if (*crc & 0x8000) *crc = (*crc << 1) ^ 0x1021;
        else *crc = (*crc << 1);
    }
}

//
uint16_t Calculate_ZSSC_CRC (const uint16_t *nvm_data)
{
    uint16_t crc = 0xFFFF;

    for (int i = 1; i < 0x79; i++)
    {
        uint16_t word = nvm_data[i];
        Crc16_update (&crc, (uint8_t) (word >> 8));
        Crc16_update (&crc, (uint8_t) (word & 0xFF));
    }

    return crc;
}

/**
 * @brief ??? ????? ???? 32??? ?????? ?о???? Helper ???
 */
int ZSSC4151_Read_Command_Response32 (uint8_t command, uint32_t *response_data)
{
    uint8_t b[5]; // command_echo + 32-bit data

    if (!I2C1_Start ()) return -1;
  
    if (!I2C1_Write (ZSSC4151_WRITE_ADDR))
    {
        I2C1_Stop ();
        return -1;
    }
  
    if (!I2C1_Write (command))
    {
        I2C1_Stop ();
        return -1;
    }
    
    if (!I2C1_Restart ())
    {
        I2C1_Stop ();
        return -1;
    }
    
    if (!I2C1_Write (ZSSC4151_READ_ADDR))
    {
        I2C1_Stop ();
        return -1;
    }

    for (int i = 0; i < 4; i++) b[i] = I2C1_Read (true);
    b[4] = I2C1_Read (false);

    if (!I2C1_Stop ()) return -1;

    // ? ?????(b[0])?? ????? ???????? ???????.
    *response_data = ((uint32_t) b[1] << 24) | ((uint32_t) b[2] << 16) | ((uint32_t) b[3] << 8) | b[4];
    return 0;
}

/**
 * @brief ZSSC4151?? ???? ????(IC Status) ????????? ????????.
 * @details WrClrIcStatus (0x61) ????? ?????? ??? ???? ?÷???? ??????.
 * @return 0: ????, -1: ????
 */
int ZSSC4151_Clear_Status (void)
{
    const uint8_t CMD_CLEAR_STATUS = 0x61;
    const uint8_t CLEAR_ALL_MSB = 0xFF; // ??? ?÷???? ????? ???? ?????
    const uint8_t CLEAR_ALL_LSB = 0xFF;

    if (!I2C1_Start ()) return -1;
    
    if (!I2C1_Write (ZSSC4151_WRITE_ADDR))
    {
        I2C1_Stop ();
        return -1;
    }
    
    if (!I2C1_Write (CMD_CLEAR_STATUS))
    {
        I2C1_Stop ();
        return -1;
    }

    // 32??? ????? ???? (??? ????? 1?? ??????? ??? ?????)
    if (!I2C1_Write (CLEAR_ALL_MSB))
    {
        I2C1_Stop ();
        return -1;
    }
    
    if (!I2C1_Write (CLEAR_ALL_LSB))
    {
        I2C1_Stop ();
        return -1;
    }
    
    if (!I2C1_Write (CLEAR_ALL_MSB))
    {
        I2C1_Stop ();
        return -1;
    }
  
    if (!I2C1_Write (CLEAR_ALL_LSB))
    {
        I2C1_Stop ();
        return -1;
    }

    if (!I2C1_Stop ()) return -1;

    delay_us (500); // ????? ??? ?ð? ???
    return 0;
}

/**
 * @brief ZSSC4151?? ???? ???? ?? ???? ????????? ?о? UART?? ???????.
 * @details Command Mode???? ?????? ????.
 */
void ZSSC4151_Check_Status (void)
{
    uint32_t status_val;

    dbg_put_string ("\r\n--- Checking ZSSC4151 Status ---\r\n");

    // Failure Status ?б? (???ø?????? ???? ????: ???? ???/??? ??)
    if (ZSSC4151_Read_Command_Response32 (0x62, &status_val) == 0)
    {
        dbg_put_string ("  FailureStatus (0x62): 0x");
        // 32??? ????? ???? ????/???? ???? ?и? ???
        dbg_put_hex_word ((uint16_t) (status_val >> 16));
        dbg_put_hex_word ((uint16_t) (status_val & 0xFFFF));

        if (status_val != 0) dbg_put_string (" <-- Application Error Detected!");
        dbg_put_string ("\r\n");
    }
    else dbg_put_string ("  Failed to read FailureStatus (0x62)\r\n");

    // IC Status ?б? (??????/????? ???? ????)
    if (ZSSC4151_Read_Command_Response32 (0x60, &status_val) == 0)
    {
        dbg_put_string ("  IC Status (0x60)      : 0x");
        dbg_put_hex_word ((uint16_t) (status_val >> 16));
        dbg_put_hex_word ((uint16_t) (status_val & 0xFFFF));
        if (status_val != 0) dbg_put_string (" <-- Firmware/HW Error Detected!");
        dbg_put_string ("\r\n");
    }
    else dbg_put_string ("  Failed to read IC Status (0x60)\r\n");
    
    dbg_put_string ("---------------------------------\r\n\r\n");
}

//==============================================================================
// ZSSC4151 NVM ?? ?? ??
//==============================================================================
//
int ZSSC4151_EnterCommandMode (void)
{
    if (!I2C1_Start ()) return -1;
    
    if (!I2C1_Write (ZSSC4151_WRITE_ADDR))
    {
        I2C1_Stop ();
        return -1;
    }
    
    if (!I2C1_Write (CMD_START_CM))
    {
        I2C1_Stop ();
        return -1;
    }
  
    if (!I2C1_Write (CM_AUTH_KEY_MSB))
    {
        I2C1_Stop ();
        return -1;
    }
    
    if (!I2C1_Write (CM_AUTH_KEY_LSB))
    {
        I2C1_Stop ();
        return -1;
    }
    
    if (!I2C1_Stop ()) return -1;
    
    delay_10ms (1);
    return 0;
}

/**
 * @brief ZSSC4151?? Normal Operation Mode?? ???????.
 * @return 0: ????, -1: ????
 */
int ZSSC4151_Start_Normal_Mode (void)
{
    if (!I2C1_Start ()) return -1;
    
    if (!I2C1_Write (ZSSC4151_WRITE_ADDR))
    {
        I2C1_Stop ();
        return -1;
    }
    
    if (!I2C1_Write (CMD_START_NOM))
    {
        I2C1_Stop ();
        return -1;
    }
    
    if (!I2C1_Stop ()) return -1;

    // Normal Mode?? ?????? ? ??????? ???? ?????? ???? ?ð? ???
    delay_10ms (5);

    return 0;
}

/*
 *  @brief NVM ???? ???? ?????? ???? ????(Lock)?? ????????. (??????)
 * @warning ?? ????? ?????? ???? ???? Ŀ??? ???? ??????? ????.
 * @return 0: ????, -1: ????
 */
int ZSSC4151_Unlock_NVM (void)
{
  // ???????? ??? ????: ????? 0x15, ???? ? 0x3C5B
  const uint8_t CMD_WR_NVM_AUTH = 0x15;
  const uint16_t AUTH_KEY = 0x3C5B;

  if (!I2C1_Start ()) return -1;
  if (!I2C1_Write (ZSSC4151_WRITE_ADDR))
    {
      I2C1_Stop ();
      return -1;
    }
  if (!I2C1_Write (CMD_WR_NVM_AUTH))
    {
      I2C1_Stop ();
      return -1;
    }

  // 16??? ???? ? ????
  if (!I2C1_Write ((uint8_t) (AUTH_KEY >> 8)))
    {
      I2C1_Stop ();
      return -1;
    } // MSB
  if (!I2C1_Write ((uint8_t) (AUTH_KEY & 0xFF)))
    {
      I2C1_Stop ();
      return -1;
    } // LSB

  if (!I2C1_Stop ()) return -1;

  delay_10ms (1); // ????? ????? ???? ª?? ???
  return 0;
}

/**
 * @brief ZSSC4151 NVM???? 1 ???? ?н????. (?????? ??? ???? ???)
 * @details ?????(0x20)?? ???? ?????? ??????? ????????.
 */
int ZSSC4151_ReadNvmWord (uint8_t nvm_addr, uint16_t *read_data)
{
    const uint8_t CMD_READ_NVM = 0x24;
    uint8_t command_echo, msb, lsb;

    if (!I2C1_Start ()) return -1;
    if (!I2C1_Write (ZSSC4151_WRITE_ADDR))
    {
        I2C1_Stop ();
        return -1;
    }
    if (!I2C1_Write (CMD_READ_NVM))
    {
        I2C1_Stop ();
        return -1;
    }
    if (!I2C1_Write (0x00))
    {
        I2C1_Stop ();
        return -1;
    }
    if (!I2C1_Write (nvm_addr))
    {
        I2C1_Stop ();
        return -1;
    }

    if (!I2C1_Restart ())
    {
        I2C1_Stop ();
        return -1;
    }

    // --- 3???: ?????? ???? (?? 3?????) ---
    if (!I2C1_Write (ZSSC4151_READ_ADDR))
    {
        I2C1_Stop ();
        return -1;
    }

    // ???? ??? ???? ?κ? ????
    command_echo = I2C1_Read (true); // 1. ????? ????(Echo) ?????(0x24)?? ?а? ???????. (ACK ????)
    msb = I2C1_Read (true); // 2. ???? ???????? MSB?? ?н????. (ACK ????)
    lsb = I2C1_Read (false); // 3. ???? ???????? LSB?? ?н????. (NACK ???????? ?б? ????)

    if (!I2C1_Stop ()) return -1;

    *read_data = ((uint16_t) msb << 8) | lsb;

    // ????? ??? ?ð? ?????? ???? ???? ????
    delay_us (100);

    return 0;
}

/**
 * @brief ZSSC4151 NVM?? 1 ???? ?????. (??????)
 * @details ?????(0x25) ?????? ???(16???)?? ??????(16???)?? ??? 32??? ?μ??? ????????.
 */
int ZSSC4151_WriteNvmWord (uint8_t nvm_addr, uint16_t write_data)
{
  const uint8_t CMD_WRITE_NVM = 0x25;

  if (!I2C1_Start ()) return -1;
  if (!I2C1_Write (ZSSC4151_WRITE_ADDR))
    {
      I2C1_Stop ();
      return -1;
    }
  if (!I2C1_Write (CMD_WRITE_NVM))
    {
      I2C1_Stop ();
      return -1;
    }

  // ???????? ??? ????: 32??? ?μ? = 16??? ??? + 16??? ??????
  // ??? ???? (???? ??????? 0)
  if (!I2C1_Write (0x00))
    {
      I2C1_Stop ();
      return -1;
    }
  if (!I2C1_Write (nvm_addr))
    {
      I2C1_Stop ();
      return -1;
    }

  // ?????? ????
  if (!I2C1_Write ((uint8_t) (write_data >> 8)))
    {
      I2C1_Stop ();
      return -1;
    }
  if (!I2C1_Write ((uint8_t) (write_data & 0xFF)))
    {
      I2C1_Stop ();
      return -1;
    }

  if (!I2C1_Stop ()) return -1;

  // ???? ???? ???? ?? t_NVM_prog ????? ??? ????? ?? ??????.
  // ?????????? t_NVM_prog = 10ms (max)
  delay_10ms (1);

  return 0;
}

/**
 * @brief ZSSC4151에게 특정 측정 작업을 시작하라고 명령합니다.
 * @details CMD 모드에서만 실행해야 합니다. 이 함수 실행 후에도 CMD 모드는 유지됩니다.
 * @param taskNum 실행할 측정 작업 번호 (0~31). 브릿지 측정은 보통 0 또는 1 입니다.
 * @param repeats 측정 반복 횟수 (0=1번, 1=2번, 2=4번, ... n=2^n 번 반복).
 * @param avgFactor 평균 필터 계수 (0~7). 2^n 으로 나누는 효과입니다.
 * @return 성공 시 0, 실패 시 -1
 */
int ZSSC4151_StartMeasTask(uint8_t taskNum, uint8_t repeats, uint8_t avgFactor)
{
    uint16_t argument;

    // 1. 커맨드에 필요한 16비트 인자(argument)를 문서에 따라 생성합니다.
    // Bits [4:0]  : taskNum (작업 번호)
    // Bits [11:8] : repeats (반복 횟수)
    // Bits [14:12]: avgFactor (평균 계수)
    argument =  ((uint16_t)(avgFactor & 0x07) << 12) | \
                ((uint16_t)(repeats   & 0x0F) << 8)  | \
                ((uint16_t)(taskNum   & 0x1F));

    // 2. I2C 통신 시작
    if (!I2C1_Start()) return -1;

    // 3. 슬레이브 주소(쓰기) 전송
    if (!I2C1_Write(ZSSC4151_WRITE_ADDR)) 
    {
        I2C1_Stop();
        return -1;
    }

    // 4. StrtMeasTask 커맨드 전송
    if (!I2C1_Write(CMD_STRT_MEAS_TASK)) 
    {
        I2C1_Stop();
        return -1;
    }

    // 5. 16비트 인자를 2바이트로 나누어 전송 (MSB 먼저)
    if (!I2C1_Write((uint8_t)(argument >> 8))) 
    { // Argument MSB
        I2C1_Stop();
        return -1;
    }
    
    if (!I2C1_Write((uint8_t)(argument & 0xFF))) 
    { // Argument LSB
        I2C1_Stop();
        return -1;
    }

    // 6. I2C 통신 종료
    if (!I2C1_Stop()) return -1;

    // 참고: 이 커맨드는 즉시 완료되지 않습니다.
    // 호출 후 ZSSC4151이 측정을 완료할 때까지 약간의 시간 지연이 필요합니다.
    return 0;
}

/**
 * @brief ZSSC4151의 RAM 특정 위치에서 원하는 만큼의 데이터를 읽어옵니다. (Burst Mode)
 * @details CMD 모드 또는 Normal 모드에서 모두 사용 가능합니다.
 * @param startAddr 읽기 시작할 RAM의 내부 주소 (0~127)
 * @param wordCount 읽어올 데이터 개수 (16비트 워드 단위)
 * @param readBuffer 데이터를 저장할 버퍼의 포인터 (최소 wordCount * 2 바이트 크기)
 * @return 성공 시 0, 실패 시 -1
 */
int ZSSC4151_ReadRamBurst(uint8_t startAddr, uint8_t wordCount, uint8_t* readBuffer)
{
    if (wordCount == 0) return 0; // 읽을 데이터가 없으면 바로 성공 리턴

    // 1. 읽을 위치를 지정하는 커맨드 전송 (쓰기 트랜잭션)
    if (!I2C1_Start()) return -1;
    
    if (!I2C1_Write(ZSSC4151_WRITE_ADDR)) 
    {
        I2C1_Stop();
        return -1;
    }
    
    if (!I2C1_Write(CMD_RD_OUT_MEM_BURST)) 
    {
        I2C1_Stop();
        return -1;
    }
    // 인자 전송: 시작 주소와 읽을 워드 개수
    // 문서상으로는 16비트 인자 하나지만, 기존 코드 스타일과 호환성을 위해
    // 별도의 바이트로 전송하는 것이 안정적일 수 있습니다.
    // ZSSC4151_Read_Output_Data 함수에서는 주소와 개수를 별도로 보냅니다.
    // 해당 문서에서는 word count가 0이면 1워드를 의미하므로, 1을 빼서 전송합니다.
    if (!I2C1_Write(startAddr)) 
    {
        I2C1_Stop();
        return -1;
    }
    
    if (!I2C1_Write(wordCount -1)) 
    { // wordCount 1 -> arg 0, wordCount 2 -> arg 1 ...
        I2C1_Stop();
        return -1;
    }

    // 2. Restart 후 읽기 트랜잭션 시작
    if (!I2C1_Restart()) 
    {
        I2C1_Stop();
        return -1;
    }
    
    if (!I2C1_Write(ZSSC4151_READ_ADDR)) 
    {
        I2C1_Stop();
        return -1;
    }
    
    // 3. ZSSC4151로부터 데이터 수신
    // 첫 바이트는 커맨드 에코(echo)이므로 읽어서 버립니다.
    (void)I2C1_Read(true); 

    // 실제 데이터 읽기
    uint16_t bytesToRead = wordCount * 2;
    for (uint16_t i = 0; i < bytesToRead; i++) 
    {
        // 마지막 바이트를 읽을 때는 NACK을 보낸다.
        bool send_ack = (i < (bytesToRead - 1));
        readBuffer[i] = I2C1_Read(send_ack);
    }

    // 4. I2C 통신 종료
    if (!I2C1_Stop()) return -1;
    
    return 0; // 성공
}

/**
 * @brief ZSSC4151 브릿지 Raw 값을 읽어오는 전체 시퀀스를 테스트합니다.
 */
/*
int Get_ZSSC4151_BridgeRaw(int32_t *raw_value)
{
    uint8_t raw_buffer[4];
    *raw_value = 0; // 에러 발생 시 0을 반환하도록 초기화

    // 1. Command Mode 진입
    if (ZSSC4151_EnterCommandMode() != 0) return -1;

    // 2. NVM 설정값을 Shadow Register로 복사
    if (ZSSC4151_CopyNvmToShadow() != 0) 
    {
        ZSSC4151_Start_Normal_Mode(); // 실패 시 Normal 모드로 복귀 시도
        return -1;
    }

    // 3. StrtMeasTask(0,0,0) 커맨드 전송
    if (ZSSC4151_StartMeasTask(0, 0, 0) != 0) 
    {
        ZSSC4151_Start_Normal_Mode();
        return -1;
    }

    // 4. 측정 완료 대기 (20ms)
    delay_10ms(2);

    // 5. RAM에서 결과값 읽기
    if (ZSSC4151_ReadRamBurst(0x15, 2, raw_buffer) != 0) 
    {
        ZSSC4151_Start_Normal_Mode();
        return -1;
    }
    
    // 6. Normal Mode로 복귀
    if (ZSSC4151_Start_Normal_Mode() != 0) return -1;
    
    // 7. 읽어온 값을 32비트 변수로 조합 (Big-endian)
    *raw_value = ((uint32_t)raw_buffer[0] << 24) | 
                 ((uint32_t)raw_buffer[1] << 16) | 
                 ((uint32_t)raw_buffer[2] << 8)  | 
                 raw_buffer[3];
                 
    return 0; // 모든 과정 성공
}
*/

int Get_ZSSC4151_BridgeRaw(int16_t *raw_value)
{
    uint8_t raw_buffer[2]; // 2바이트만 수신하므로 버퍼 크기를 2로 변경
    *raw_value = 0; // 에러 발생 시 0을 반환하도록 초기화

    // 1. Command Mode 진입
    if (ZSSC4151_EnterCommandMode() != 0) return -1;

    // 2. NVM 설정값을 Shadow Register로 복사
    if (ZSSC4151_CopyNvmToShadow() != 0) 
    {
        ZSSC4151_Start_Normal_Mode(); // 실패 시 Normal 모드로 복귀 시도
        return -1;
    }

    // 3. StrtMeasTask(0,0,0) 커맨드 전송
    if (ZSSC4151_StartMeasTask(0, 0, 0) != 0) 
    {
        ZSSC4151_Start_Normal_Mode();
        return -1;
    }

    // 4. 측정 완료 대기 (20ms)
    delay_10ms(2);

    // 5. RAM에서 결과값 1워드(2바이트) 읽기
    // ZSSC4151_ReadRamBurst의 두 번째 인자를 1로 변경
    if (ZSSC4151_ReadRamBurst(0x15, 1, raw_buffer) != 0) 
    {
        ZSSC4151_Start_Normal_Mode();
        return -1;
    }
    
    // 6. Normal Mode로 복귀
    if (ZSSC4151_Start_Normal_Mode() != 0) return -1;
    
    // 7. 읽어온 2바이트를 16비트 변수로 조합 (Big-endian)
    *raw_value = ((uint16_t)raw_buffer[0] << 8) | raw_buffer[1];
            
    return 0; // 모든 과정 성공
}

//
int ZSSC4151_CopyNvmToShadow(void)
{
    if (!I2C1_Start()) return -1;
    
    if (!I2C1_Write(ZSSC4151_WRITE_ADDR)) 
    {
        I2C1_Stop();
        return -1;
    }
    
    if (!I2C1_Write(CMD_CP_NVM_TO_SHDW)) 
    {
        I2C1_Stop();
        return -1;
    }
    
    if (!I2C1_Stop()) return -1;

    // 커맨드 실행 후 안정화를 위해 약간의 지연 시간을 줍니다.
    delay_us(500); 
    
    return 0;
}
