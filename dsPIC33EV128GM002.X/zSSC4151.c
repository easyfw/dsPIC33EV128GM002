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
 * @brief ??? ????? ???? 32??? ?????? ?��???? Helper ???
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
 * @details WrClrIcStatus (0x61) ????? ?????? ??? ???? ?��???? ??????.
 * @return 0: ????, -1: ????
 */
int ZSSC4151_Clear_Status (void)
{
    const uint8_t CMD_CLEAR_STATUS = 0x61;
    const uint8_t CLEAR_ALL_MSB = 0xFF; // ??? ?��???? ????? ???? ?????
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

    delay_us (500); // ????? ??? ?��? ???
    return 0;
}

/**
 * @brief ZSSC4151?? ???? ???? ?? ???? ????????? ?��? UART?? ???????.
 * @details Command Mode???? ?????? ????.
 */
void ZSSC4151_Check_Status (void)
{
    uint32_t status_val;

    dbg_put_string ("\r\n--- Checking ZSSC4151 Status ---\r\n");

    // Failure Status ?��? (???��?????? ???? ????: ???? ???/??? ??)
    if (ZSSC4151_Read_Command_Response32 (0x62, &status_val) == 0)
    {
        dbg_put_string ("  FailureStatus (0x62): 0x");
        // 32??? ????? ???? ????/???? ???? ?��? ???
        dbg_put_hex_word ((uint16_t) (status_val >> 16));
        dbg_put_hex_word ((uint16_t) (status_val & 0xFFFF));

        if (status_val != 0) dbg_put_string (" <-- Application Error Detected!");
        dbg_put_string ("\r\n");
    }
    else dbg_put_string ("  Failed to read FailureStatus (0x62)\r\n");

    // IC Status ?��? (??????/????? ???? ????)
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

    // Normal Mode?? ?????? ? ??????? ???? ?????? ???? ?��? ???
    delay_10ms (5);

    return 0;
}

/*
 *  @brief NVM ???? ???? ?????? ???? ????(Lock)?? ????????. (??????)
 * @warning ?? ????? ?????? ???? ???? ��??? ???? ??????? ????.
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

  delay_10ms (1); // ????? ????? ???? ��?? ???
  return 0;
}

/**
 * @brief ZSSC4151 NVM???? 1 ???? ?��????. (?????? ??? ???? ???)
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

    // ???? ??? ???? ?��? ????
    command_echo = I2C1_Read (true); // 1. ????? ????(Echo) ?????(0x24)?? ?��? ???????. (ACK ????)
    msb = I2C1_Read (true); // 2. ???? ???????? MSB?? ?��????. (ACK ????)
    lsb = I2C1_Read (false); // 3. ???? ???????? LSB?? ?��????. (NACK ???????? ?��? ????)

    if (!I2C1_Stop ()) return -1;

    *read_data = ((uint16_t) msb << 8) | lsb;

    // ????? ??? ?��? ?????? ???? ???? ????
    delay_us (100);

    return 0;
}

/**
 * @brief ZSSC4151 NVM?? 1 ???? ?????. (??????)
 * @details ?????(0x25) ?????? ???(16???)?? ??????(16???)?? ??? 32??? ?��??? ????????.
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

  // ???????? ??? ????: 32??? ?��? = 16??? ??? + 16??? ??????
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
  delay_10ms (2);

  return 0;
}

/**
 * @brief ZSSC4151���� Ư�� ���� �۾��� �����϶�� ����մϴ�.
 * @details CMD ��忡���� �����ؾ� �մϴ�. �� �Լ� ���� �Ŀ��� CMD ���� �����˴ϴ�.
 * @param taskNum ������ ���� �۾� ��ȣ (0~31). �긴�� ������ ���� 0 �Ǵ� 1 �Դϴ�.
 * @param repeats ���� �ݺ� Ƚ�� (0=1��, 1=2��, 2=4��, ... n=2^n �� �ݺ�).
 * @param avgFactor ��� ���� ��� (0~7). 2^n ���� ������ ȿ���Դϴ�.
 * @return ���� �� 0, ���� �� -1
 */
int ZSSC4151_StartMeasTask(uint8_t taskNum, uint8_t repeats, uint8_t avgFactor)
{
    uint16_t argument;

    // 1. Ŀ�ǵ忡 �ʿ��� 16��Ʈ ����(argument)�� ������ ���� �����մϴ�.
    // Bits [4:0]  : taskNum (�۾� ��ȣ)
    // Bits [11:8] : repeats (�ݺ� Ƚ��)
    // Bits [14:12]: avgFactor (��� ���)
    argument =  ((uint16_t)(avgFactor & 0x07) << 12) | \
                ((uint16_t)(repeats   & 0x0F) << 8)  | \
                ((uint16_t)(taskNum   & 0x1F));

    // 2. I2C ��� ����
    if (!I2C1_Start()) return -1;

    // 3. �����̺� �ּ�(����) ����
    if (!I2C1_Write(ZSSC4151_WRITE_ADDR)) 
    {
        I2C1_Stop();
        return -1;
    }

    // 4. StrtMeasTask Ŀ�ǵ� ����
    if (!I2C1_Write(CMD_STRT_MEAS_TASK)) 
    {
        I2C1_Stop();
        return -1;
    }

    // 5. 16��Ʈ ���ڸ� 2����Ʈ�� ������ ���� (MSB ����)
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

    // 6. I2C ��� ����
    if (!I2C1_Stop()) return -1;

    // ����: �� Ŀ�ǵ�� ��� �Ϸ���� �ʽ��ϴ�.
    // ȣ�� �� ZSSC4151�� ������ �Ϸ��� ������ �ణ�� �ð� ������ �ʿ��մϴ�.
    return 0;
}

/**
 * @brief ZSSC4151�� RAM Ư�� ��ġ���� ���ϴ� ��ŭ�� �����͸� �о�ɴϴ�. (Burst Mode)
 * @details CMD ��� �Ǵ� Normal ��忡�� ��� ��� �����մϴ�.
 * @param startAddr �б� ������ RAM�� ���� �ּ� (0~127)
 * @param wordCount �о�� ������ ���� (16��Ʈ ���� ����)
 * @param readBuffer �����͸� ������ ������ ������ (�ּ� wordCount * 2 ����Ʈ ũ��)
 * @return ���� �� 0, ���� �� -1
 */
int ZSSC4151_ReadRamBurst(uint8_t startAddr, uint8_t wordCount, uint8_t* readBuffer)
{
    if (wordCount == 0) return 0; // ���� �����Ͱ� ������ �ٷ� ���� ����

    // 1. ���� ��ġ�� �����ϴ� Ŀ�ǵ� ���� (���� Ʈ�����)
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
    // ���� ����: ���� �ּҿ� ���� ���� ����
    // ���������δ� 16��Ʈ ���� �ϳ�����, ���� �ڵ� ��Ÿ�ϰ� ȣȯ���� ����
    // ������ ����Ʈ�� �����ϴ� ���� �������� �� �ֽ��ϴ�.
    // ZSSC4151_Read_Output_Data �Լ������� �ּҿ� ������ ������ �����ϴ�.
    // �ش� ���������� word count�� 0�̸� 1���带 �ǹ��ϹǷ�, 1�� ���� �����մϴ�.
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

    // 2. Restart �� �б� Ʈ����� ����
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
    
    // 3. ZSSC4151�κ��� ������ ����
    // ù ����Ʈ�� Ŀ�ǵ� ����(echo)�̹Ƿ� �о �����ϴ�.
    (void)I2C1_Read(true); 

    // ���� ������ �б�
    uint16_t bytesToRead = wordCount * 2;
    for (uint16_t i = 0; i < bytesToRead; i++) 
    {
        // ������ ����Ʈ�� ���� ���� NACK�� ������.
        bool send_ack = (i < (bytesToRead - 1));
        readBuffer[i] = I2C1_Read(send_ack);
    }

    // 4. I2C ��� ����
    if (!I2C1_Stop()) return -1;
    
    return 0; // ����
}

//
int Get_ZSSC4151_BridgeRaw(int16_t *raw_value)
{
    uint8_t raw_buffer[2]; // 2����Ʈ�� �����ϹǷ� ���� ũ�⸦ 2�� ����
    *raw_value = 0; // ���� �߻� �� 0�� ��ȯ�ϵ��� �ʱ�ȭ

    // 1. Command Mode ����
    if (ZSSC4151_EnterCommandMode() != 0) return -1;

    // 2. NVM �������� Shadow Register�� ����
    if (ZSSC4151_CopyNvmToShadow() != 0) 
    {
        ZSSC4151_Start_Normal_Mode(); // ���� �� Normal ���� ���� �õ�
        return -1;
    }

    // 3. StrtMeasTask(0,0,0) Ŀ�ǵ� ����
    if (ZSSC4151_StartMeasTask(0, 0, 0) != 0) 
    {
        ZSSC4151_Start_Normal_Mode();
        return -1;
    }

    // 4. ���� �Ϸ� ��� (20ms)
    delay_10ms(2);

    // 5. RAM���� ����� 1����(2����Ʈ) �б�
    // ZSSC4151_ReadRamBurst�� �� ��° ���ڸ� 1�� ����
    if (ZSSC4151_ReadRamBurst(0x15, 1, raw_buffer) != 0) 
    {
        ZSSC4151_Start_Normal_Mode();
        return -1;
    }
    
    // 6. Normal Mode�� ����
    if (ZSSC4151_Start_Normal_Mode() != 0) return -1;
    
    // 7. �о�� 2����Ʈ�� 16��Ʈ ������ ���� (Big-endian)
    *raw_value = ((uint16_t)raw_buffer[0] << 8) | raw_buffer[1];
            
    return 0; // ��� ���� ����
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

    // Ŀ�ǵ� ���� �� ����ȭ�� ���� �ణ�� ���� �ð��� �ݴϴ�.
    delay_us(500); 
    
    return 0;
}

/**
 * @brief Normal Mode���� I2C 'RdOutMemBurst (0x2E)' ������� RAM ���� �б�
 * @param address �а��� �ϴ� RAM�� ���� �ּ�
 * @param word_count �о�� ����(16��Ʈ)�� ����
 * @param buffer �����͸� ������ ������ ������
 * @return ���� �� 0, ���� �� ���� ���� �ڵ�
 */
int ZSSC4151_Read_Ram_In_Normal_Mode(uint8_t address, uint8_t word_count, uint8_t* buffer)
{
    // 1. I2C Start
    if (!I2C1_Start()) return -1;

    // 2. �����̺� �ּ� ���� (���� ���)
    if (!I2C1_Write(ZSSC4151_WRITE_ADDR))
    {
        I2C1_Stop();
        return -2;
    }

    // 3. 'RdOutMemBurst' ���(0x2E) ����
    if (!I2C1_Write(0x2E))
    {
        I2C1_Stop();
        return -3;
    }

    // 4. �μ� ����: ���� �ּ�(���� ����Ʈ)�� ���� ����(���� ����Ʈ)
    if (!I2C1_Write(address)) // ���� ����Ʈ: RAM ���� �ּ�
    {
        I2C1_Stop();
        return -4;
    }
    if (!I2C1_Write(word_count - 1)) // ���� ����Ʈ: ���� ���� ���� - 1
    {
        I2C1_Stop();
        return -5;
    }
    
    // 5. I2C Restart
    if (!I2C1_Restart()) return -6;

    // 6. �����̺� �ּ� ���� (�б� ���)
    if (!I2C1_Write(ZSSC4151_READ_ADDR))
    {
        I2C1_Stop();
        return -7;
    }

    // 7. ������ ����
    for (uint8_t i = 0; i < (word_count * 2); i += 2)
    {
        buffer[i] = I2C1_Read(true); // MSB ���� �� ACK
        buffer[i + 1] = I2C1_Read(i == (word_count * 2) - 2 ? false : true); // ������ ����Ʈ�� NACK, �� �ܿ��� ACK
    }

    // 8. I2C Stop
    if (!I2C1_Stop()) return -8;

    return 0; // ����
}

//
int ZSSC4151_ReadRam_Corrected(uint8_t address, uint8_t word_count, volatile uint8_t* buffer)
{
    // --- ���� Ʈ�����: ���� ��ġ ���� ---
    if (!I2C1_Start()) return -1;

    // 2. �����̺� �ּ� ���� (���� ���)
    if (!I2C1_Write(ZSSC4151_WRITE_ADDR))
    {
        I2C1_Stop();
        return -2;
    }

    // 3. 'RdOutMemBurst' ���(0x2E) ����
    if (!I2C1_Write(CMD_RD_OUT_MEM_BURST))
    {
        I2C1_Stop();
        return -3;
    }

    // 4. [������] �����ͽ�Ʈ�� ���� 16��Ʈ �μ��� �����ϰ� �����մϴ�.
    // ���� 8��Ʈ: ���� �ּ�, ���� 8��Ʈ: ���� ���� �� - 1
    int16_t argument = ((int16_t)address << 8) | (word_count - 1);

    // 4-1. �μ� MSB ����
    if (!I2C1_Write((uint8_t)(argument >> 8)))
    {
        I2C1_Stop();
        return -4;
    }
    // 4-2. �μ� LSB ����
    if (!I2C1_Write((uint8_t)(argument & 0xFF)))
    {
        I2C1_Stop();
        return -5;
    }

    // --- �б� Ʈ�����: ������ ���� ---
    if (!I2C1_Restart()) return -6;

    // 6. �����̺� �ּ� ���� (�б� ���)
    if (!I2C1_Write(ZSSC4151_READ_ADDR))
    {
        I2C1_Stop();
        return -7;
    }

    // 7. [�߰���] ù ����Ʈ�� Command Echo(0x2E)�̹Ƿ� �а� �����ϴ�.
    (void)I2C1_Read(true);

    // 8. ���� �����͸� �����մϴ�.
    int16_t bytesToRead = word_count * 2;
    for (int16_t i = 0; i < bytesToRead; i++)
    {
        bool is_last_byte = (i == (bytesToRead - 1));
        buffer[i] = I2C1_Read(!is_last_byte); // ������ ����Ʈ �������� ACK, ������ ����Ʈ�� NACK
    }

    // 9. I2C Stop
    if (!I2C1_Stop()) return -8;

    return 0; // ����
}

/**
 * @brief ZSSC4151���� ��ü ���� ����Ŭ�� �����϶�� ����մϴ�. (StrtMeasCyc, 0x0B)
 * @details Command Mode������ �����ؾ� �մϴ�. �� �Լ� ���� �Ŀ��� Command Mode�� �����˴ϴ�.
 * @return ���� �� 0, ���� �� -1
 */
int ZSSC4151_StartMeasCycle(void)
{
    // 1. I2C ��� ����
    if (!I2C1_Start()) return -1;

    // 2. �����̺� �ּ�(����) ����
    if (!I2C1_Write(ZSSC4151_WRITE_ADDR))
    {
        I2C1_Stop();
        return -2;
    }

    // 3. StrtMeasCyc Ŀ�ǵ�(0x0B) ����
    if (!I2C1_Write(CMD_STRT_MEAS_CYC))    
    {
        I2C1_Stop();
        return -3;
    }

    // 4. �����ͽ�Ʈ�� ������ StrtMeasCyc�� 16��Ʈ �μ��� �޽��ϴ�.
    // �Ϲ����� ��� ��ʿ����� �� �μ��� 0���� �����մϴ�.
    // Argument: strtMeasCycArg, 16 Bits
    if (!I2C1_Write(0x00)) // Argument MSB
    {
        I2C1_Stop();
        return -4;
    }
    if (!I2C1_Write(0x00)) // Argument LSB
    {
        I2C1_Stop();
        return -5;
    }

    // 5. I2C ��� ����
    if (!I2C1_Stop()) return -6;

    return 0;
}

/**
 * @brief RAM�� �ִ� ���� �����͸� �������� ���� ����Ŭ�� �����մϴ�. (RunCondCyc, 0x0E)
 * @details �� ��� �� RAM 0x41 � ���� ������� ��ϵ˴ϴ�.
 * @return ���� �� 0, ���� �� -1
 */
int ZSSC4151_RunConditioningCycle(void)
{
    if (!I2C1_Start()) return -1;
    if (!I2C1_Write(ZSSC4151_WRITE_ADDR)) { I2C1_Stop(); return -2; }
    if (!I2C1_Write(CMD_RUN_COND_CYC)) { I2C1_Stop(); return -3; }
    // RunCondCyc ���� 16��Ʈ �μ��� ������, ���� 0���� �����մϴ�.
    if (!I2C1_Write(0x00)) { I2C1_Stop(); return -4; } // Argument MSB
    if (!I2C1_Write(0x00)) { I2C1_Stop(); return -5; } // Argument LSB
    if (!I2C1_Stop()) return -6;
    return 0;
}

/**
 * @brief ZSSC4151�� ���� ��������(IC Status, Failure Status)�� �о� ����մϴ�.
 */
void ZSSC4151_Check_Status_Registers(void)
{
    uint32_t status_val;

    // IC Status �б� (0x60)
    if (ZSSC4151_Read_Command_Response32(CMD_RD_IC_STATUS, &status_val) == 0)
    {
        dbg_put_string(" -> IC Status (0x60): 0x");
        dbg_put_hex_word((uint16_t)(status_val >> 16));
        dbg_put_hex_word((uint16_t)(status_val & 0xFFFF));
        dbg_put_string("\r\n");
    }
    else
    {
        dbg_put_string(" -> Failed to read IC Status (0x60)\r\n");
    }

    // Failure Status �б� (0x62)
    if (ZSSC4151_Read_Command_Response32(CMD_RD_FAILURE_STATUS, &status_val) == 0)
    {
        dbg_put_string(" -> Failure Status (0x62): 0x");
        dbg_put_hex_word((uint16_t)(status_val >> 16));
        dbg_put_hex_word((uint16_t)(status_val & 0xFFFF));
        dbg_put_string("\r\n");
    }
    else
    {
        dbg_put_string(" -> Failed to read Failure Status (0x62)\r\n");
    }
}