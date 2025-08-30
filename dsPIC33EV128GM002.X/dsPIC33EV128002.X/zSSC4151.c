/*
 * File:   ZSSC4151.c
 * Author: easyf
 *
 * Created on August 30, 2025, 4:38 PM
 */
#include <stdbool.h>
#include <stdint.h> 
#include "zSSC4151.h"

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
 * @brief ??? ????? ???? 32??? ?????? ?¬à???? Helper ???
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
 * @details WrClrIcStatus (0x61) ????? ?????? ??? ???? ?¡À???? ??????.
 * @return 0: ????, -1: ????
 */
int ZSSC4151_Clear_Status (void)
{
  const uint8_t CMD_CLEAR_STATUS = 0x61;
  const uint8_t CLEAR_ALL_MSB = 0xFF; // ??? ?¡À???? ????? ???? ?????
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

  delay_us (500); // ????? ??? ?©£? ???
  return 0;
}

/**
 * @brief ZSSC4151?? ???? ???? ?? ???? ????????? ?¬à? UART?? ???????.
 * @details Command Mode???? ?????? ????.
 */
void ZSSC4151_Check_Status (void)
{
  uint32_t status_val;

  dbg_put_string ("\r\n--- Checking ZSSC4151 Status ---\r\n");

  // Failure Status ?¬Ò? (???©ª?????? ???? ????: ???? ???/??? ??)
  if (ZSSC4151_Read_Command_Response32 (0x62, &status_val) == 0)
    {
      dbg_put_string ("  FailureStatus (0x62): 0x");
      // 32??? ????? ???? ????/???? ???? ?¬Ú? ???
      dbg_put_hex_word ((uint16_t) (status_val >> 16));
      dbg_put_hex_word ((uint16_t) (status_val & 0xFFFF));

      if (status_val != 0)
        {
          dbg_put_string (" <-- Application Error Detected!");
        }
      dbg_put_string ("\r\n");
    }
  else
    {
      dbg_put_string ("  Failed to read FailureStatus (0x62)\r\n");
    }

  // IC Status ?¬Ò? (??????/????? ???? ????)
  if (ZSSC4151_Read_Command_Response32 (0x60, &status_val) == 0)
    {
      dbg_put_string ("  IC Status (0x60)      : 0x");
      dbg_put_hex_word ((uint16_t) (status_val >> 16));
      dbg_put_hex_word ((uint16_t) (status_val & 0xFFFF));
      if (status_val != 0)
        {
          dbg_put_string (" <-- Firmware/HW Error Detected!");
        }
      dbg_put_string ("\r\n");
    }
  else
    {
      dbg_put_string ("  Failed to read IC Status (0x60)\r\n");
    }
  dbg_put_string ("---------------------------------\r\n\r\n");
}

/**
 * @brief ZSSC4151 NVRAM?? 16??? ??????? ???? ??? (?????? ????? ????)
 * @param reg_addr ??????? ?? NVRAM ???????? ???
 * @param data ?? 16??? ??????
 * @return 0: ????, -1: ????
 */
int ZSSC4151_Write_NVM (unsigned char reg_addr, unsigned int data)
{
  // NVM ???? ?????(0x25)?? 5??? ???? ??????? ¨¨??? ????? ????
  unsigned char command_byte = 0x25 | (reg_addr & 0x1F);
  int error = 0;

  if (I2C1_Start () != 0) return -1;
  if (I2C1_SendByte (ZSSC4151_WRITE_ADDR) != 0) error = -1;

  // ?????? ?¥ê?: ??? ??? ????? ????? ????? ????
  if (!error && I2C1_SendByte (command_byte) != 0) error = -1;

  if (!error && I2C1_SendByte ((unsigned char) (data >> 8)) != 0) error = -1;
  if (!error && I2C1_SendByte ((unsigned char) (data & 0xFF)) != 0) error = -1;

  I2C1_Stop ();

  // NVRAM ?????? ?©£??? ??? ?? ??????? ????? ??????? ????.
  delay_10ms (2); // ?????????? t_NVM_prog ?©£??? ??????? ??????? 20ms ???

  return error;
}

/**
 * @brief ZSSC4151?? NVM ???? ?????? ?????? ?¬à? ?©ª???? ???????.
 */
void Dump_NVM_Map (void)
{
    uint16_t nvm_value;
    uint8_t start_addr = 0x00;
    uint8_t end_addr = 0x4B;

    dbg_put_string ("\r\n--- NVM Map Dump Start ---\r\n");

    for (uint8_t addr = start_addr; addr <= end_addr; addr++)
    {
      // --- ???? ?????? ?¥ê?: ???? ?¬Ò? ????? ZSSC4151_ReadNvmWord?? ???????. ???? ---
        if (ZSSC4151_ReadNvmWord (addr, &nvm_value) == 0)
        {
          // ???? ??, ???? ???? ???
            dbg_put_string ("NVM[0x");
            dbg_put_hex_byte (addr);
            dbg_put_string ("] = 0x");
            dbg_put_hex_word (nvm_value);
            dbg_put_string ("\r\n");
        }
        else
        {
            // ???? ??, ???? ????? ???
            dbg_put_string ("NVM[0x");
            dbg_put_hex_byte (addr);
            dbg_put_string ("] = Read FAILED\r\n");
        }
        delay_10ms (1); // ?? NVM ?¬Ò? ????? ¨£?? ??????
    }

    dbg_put_string ("--- NVM Map Dump End ---\r\n");
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

  // Normal Mode?? ?????? ? ??????? ???? ?????? ???? ?©£? ???
  delay_10ms (5);

  return 0;
}



//
bool ZSSC4151_Read_Normal_Mode_Output (uint16_t* read_word)
{
    uint8_t rx_buffer[2];

    // 1. I2C ??? ????
    if (!I2C1_Start ())
    {
        return false;
    }

    // 2. ?????? ?¬Ò? ??? ????
    if (!I2C1_Write (ZSSC4151_READ_ADDR))
    {
        I2C1_Stop ();
        return false;
    }

    // 3. ?????¥ê??? 2????? ?????? ?¬Ò?
    rx_buffer[0] = I2C1_Read (true); // MSB ?¬Ñ?, ACK ????
    rx_buffer[1] = I2C1_Read (false); // LSB ?¬Ñ?, NACK ????

    // 4. I2C ??? ????
    I2C1_Stop ();

    // 5. ?¬à?? ????????? 16??? ????? ????
    *read_word = ((uint16_t) rx_buffer[0] << 8) | rx_buffer[1];

    return true;
}

/**
 * @brief Normal Mode???? ZSSC4151?? ??? RAM ???? ?¬ß????. (burst mode ???)
 * @param ram_addr ?¬Ñ? ???? RAM?? ???? ???
 * @param read_data ?¬à?? 16??? ??????? ????? ??????
 * @return 0: ????, -1: ????
 */
int ZSSC4151_Read_Output_Data (uint8_t ram_addr, uint16_t *read_data)
{
  // App Desc. ???? Table 5.3 ????
  const uint8_t CMD_READ_OUTPUT_MEM_BURST = 0x2E;
  uint8_t arg_msb = ram_addr; // Start Address
  uint8_t arg_lsb = 0x00; // Word Count (0x00 means 1 word)
  uint8_t cmd_echo, msb, lsb;

  if (!I2C1_Start ()) return -1;
  if (!I2C1_Write (ZSSC4151_WRITE_ADDR))
    {
      I2C1_Stop ();
      return -1;
    }
  if (!I2C1_Write (CMD_READ_OUTPUT_MEM_BURST))
    {
      I2C1_Stop ();
      return -1;
    }
  if (!I2C1_Write (arg_msb))
    {
      I2C1_Stop ();
      return -1;
    }
  if (!I2C1_Write (arg_lsb))
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

  // RAM ?¬Ò? ????? ????? ???? ?? ??????? ??????? 3????? ???
  cmd_echo = I2C1_Read (true);
  msb = I2C1_Read (true);
  lsb = I2C1_Read (false);

  if (!I2C1_Stop ()) return -1;

  *read_data = ((uint16_t) msb << 8) | lsb;
  return 0;
}

//

bool ZSSC4151_ReadNvm (uint8_t addr, uint16_t *val)
{
  uint8_t rx_buffer[2];
  uint8_t command_with_addr;

  // ???? ??? ????: ?????(0x20)?? ???(???? 5???)?? ???? ????
  command_with_addr = 0x24 | (addr & 0x1F);

  // --- 1. ???? ????? ????? ?????? ???? ?????? ---
  if (!I2C1_Start ()) return false;
  if (!I2C1_Write (ZSSC4151_WRITE_ADDR))
    {
      I2C1_Stop ();
      return false;
    }
  if (!I2C1_Write (command_with_addr))
    {
      I2C1_Stop ();
      return false;
    }
  // if (!I2C1_Write(addr))             { I2C1_Stop(); return false; } // ?????? ??? ????? ?????? ???? ??? ????

  // ???? Probe?? ??????? Repeated Start ??????? ???? ????
  if (!I2C1_Restart ())
    {
      I2C1_Stop ();
      return false;
    }

  // --- 2. ??????? ???????? ?¬Ò? ?????? ---
  if (!I2C1_Write (ZSSC4151_READ_ADDR))
    {
      I2C1_Stop ();
      return false;
    }

  rx_buffer[0] = I2C1_Read (true);
  rx_buffer[1] = I2C1_Read (false);

  I2C1_Stop ();

  /*    
      // ?¥á? ???
      dbg_put_string("  NVM Read Raw (2 bytes): ");
      dbg_put_hex_byte(rx_buffer[0]);
      dbg_putc(' ');
      dbg_put_hex_byte(rx_buffer[1]);
      dbg_put_string("\r\n");
   */
  *val = ((uint16_t) rx_buffer[0] << 8) | rx_buffer[1];
  return true;
}

/*
 *  @brief NVM ???? ???? ?????? ???? ????(Lock)?? ????????. (??????)
 * @warning ?? ????? ?????? ???? ???? ¨¨??? ???? ??????? ????.
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

  delay_10ms (1); // ????? ????? ???? ¨£?? ???
  return 0;
}

/**
 * @brief ZSSC4151 NVM???? 1 ???? ?¬ß????. (?????? ??? ???? ???)
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

    // ???? ??? ???? ?¥ê? ????
    command_echo = I2C1_Read (true); // 1. ????? ????(Echo) ?????(0x24)?? ?¬Ñ? ???????. (ACK ????)
    msb = I2C1_Read (true); // 2. ???? ???????? MSB?? ?¬ß????. (ACK ????)
    lsb = I2C1_Read (false); // 3. ???? ???????? LSB?? ?¬ß????. (NACK ???????? ?¬Ò? ????)

    if (!I2C1_Stop ()) return -1;

    *read_data = ((uint16_t) msb << 8) | lsb;

    // ????? ??? ?©£? ?????? ???? ???? ????
    delay_us (100);

    return 0;
}

/**
 * @brief ZSSC4151 NVM?? 1 ???? ?????. (??????)
 * @details ?????(0x25) ?????? ???(16???)?? ??????(16???)?? ??? 32??? ?¥ì??? ????????.
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

  // ???????? ??? ????: 32??? ?¥ì? = 16??? ??? + 16??? ??????
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

