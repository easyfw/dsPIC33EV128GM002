/*
 * File:   main.c
 * Author: easyf
 *
 * Created on 2025? 8? 25? (?), ?? 1:05
 */
#include <p33EV128GM002.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h> 

//
// General Option
//
// ZSSC4151 ?? ?? ??

typedef enum
{
  ZSSC_MODE_COMMAND, // ?? ?? (?? ?? ?? ??, Power-On ?? ??)
  ZSSC_MODE_NORMAL // ?? ?? ?? (?? ??, ?? ?? ??)
} ZSSC4151_Mode_t;


// >> Set for Configuration bits <<
// #pragma config FNOSC = PRI, POSCMD = XT, FCKSM = CSDCMD, OSCIOFNC = OFF
// Dead Man Timer is Disabled
#pragma config FNOSC = PRIPLL, POSCMD = XT, FCKSM = CSECMD, OSCIOFNC = OFF
#pragma config FWDTEN = OFF, ICS = PGD1, BOREN0 = OFF, ALTI2C1 = OFF, DMTEN = DISABLE

#define FCAN                                    40000000UL      // Fcyc = 1/2Fpll
#define FCY                                       40000000UL      // ??? ?? ??
#define I2C1_CLOCK                          100000L            // I2C1 ?? ?? (100kHz)

#define SHT4X_ADDR                          0x45
#define SHT4X_WRITE_ADDR                (SHT4X_ADDR << 1)
#define SHT4X_READ_ADDR                 ((SHT4X_ADDR << 1) | 1)
#define SHT4X_CMD_MEASURE_HPM   0xFD // ??? ?? ??

/********************************************************************************/
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

// UART
// FOSC = (8MHz(XT) / PLLPRE=2) * (PLLFBD=30) / (PLLPOST=2) = 60MHz
// FCY = FOSC / 2 = 30 MIPS
#define BAUD38400   ((FCAN/38400)/16) - 1   // this is what the demo UART serial baud rate is

#define NUM_DIGITS 5               // floating point digits to print
#define STRING_BUFFER_SIZE 64      // arbitrary length message buffer

//
volatile unsigned long t1_tick, t2_tick;
volatile unsigned char t3_done;

volatile char sent_rx; // receive message flags
volatile int datal;
volatile int datah;

uint8_t buffer[6];
float T, H;

volatile int tickTime = 50; // Tick time in us
volatile float peripheralClk = 39.77; // in Mhz

//
// Function declaration
//
void oscConfig (void);
void clearIntrflags (void);

void delay_10ms (uint16_t num);
void delay_us (unsigned int us);

// send a character to the serial port

void UART1_Init (void);

void dbg_putc (char c);
void dbg_put_hex_byte (uint8_t val);
void dbg_put_hex_word (uint16_t val);
void dbg_put_float (float temp);
void dbg_put_string (char *s);

void I2C1_Init (void);
bool I2C1_Start (void);
bool I2C1_Stop (void);
bool I2C1_Restart (void);
bool I2C1_Write (uint8_t data);
uint8_t I2C1_Read (bool send_ack);
void I2C1_WaitForIdle (void);

void I2C1_Ack (void);
void I2C1_Nack (void);
void I2C_Scan (void);
int I2C1_SendByte (unsigned char data);

bool SHT4x_Read_TH (float *T, float *H);

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

void Crc16_update (uint16_t *crc, uint8_t data);
uint16_t Calculate_ZSSC_CRC (const uint16_t *nvm_data);

void Dump_NVM_Map (void);
bool ZSSC4151_Probe_CM (void);

void Check_Reset_Cause (void);

void Init_Hw ();
void REFCLKO_Init (void);

void SENT1_TX_Init (void);
void ADC_Init (void);

uint8_t mode;
uint32_t ascii_lo, ascii_hi, hex_dig, count = 1;
uint16_t *readData;

const uint16_t harsh_emc_nvm_config[128] = {
  // 0x00 - 0x0F
  0xC008, 0x0886, 0xA022, 0x61A2, 0xA010, 0x0800, 0x0000, 0xC1F0,
  0x80FA, 0x5408, 0x0000, 0x0000, 0x0610, 0x3740, 0x8610, 0x0400,
  // 0x10 - 0x1F
  0x4DE1, 0x1D80, 0x5F9A, 0x1503, 0x1526, 0xFFFF, 0x0003, 0x00B0,
  0x0000, 0x0000, 0x4000, 0x0000, 0x0002, 0x0008, 0x0000, 0x0000,
  // 0x20 - 0x2F
  0x7FFF, 0x0002, 0x0005, 0x0000, 0x0000, 0x0000, 0x0000, 0x4000,
  0xB847, 0x0096, 0x0097, 0xFF00, 0x1000, 0x0001, 0x5898, 0x6C48,
  // 0x30 - 0x3F
  0x0000, 0x0001, 0x8000, 0xFC00, 0xF000, 0x1000, 0x8000, 0x7FFF,
  0x0400, 0x7BFF, 0x3000, 0x6900, 0x5103, 0x17AD, 0x0000, 0x0000,
  // 0x40 - 0x4F
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x589C, 0x1805,
  // 0x50 - 0x5F
  0x589A, 0x1804, 0xE28B, 0x3E1D, 0xE286, 0x3C1D, 0x58A4, 0x1802,
  0x98A0, 0x1817, 0xD898, 0x1803, 0x4400, 0x3A02, 0x7678, 0x1800,
  // 0x60 - 0x6F
  0xB410, 0x8600, 0x3800, 0x0244, 0x98A2, 0x1815, 0xD89E, 0x1819,
  0xCA07, 0x3206, 0xBA92, 0x4621, 0x2A00, 0x4017, 0x1291, 0x2E90,
  // 0x70 - 0x7F
  0x2280, 0x2E16, 0x121F, 0x2C1E, 0x0400, 0x2A16, 0x0000, 0x2C01,
  0x5286, 0x5000, 0x0000, 0x0000, 0x0000, 0x1428, 0x1DBC, 0x0000
};

//
int main (void)
{
    RCONbits.SWDTEN = 0; // ??? ?? ????????? WDT ???? ????

    // --- 1. ???? ???? ????? I2C?? ??????? ZSSC4151 ???? ??? ???? ??? ---
    oscConfig ();

    // --- 2. ?????? ?????? ???? ---
    Init_Hw ();
    UART1_Init ();
    I2C1_Init ();

  REFCLKO_Init ();
  ADC_Init ();
  SENT1_TX_Init ();

  bool command_mode_ok = (ZSSC4151_EnterCommandMode () == 0);

  // --- 2. ???? ???? ?¡À??? ???? ---
  dbg_put_string ("Clearing IC Status flags...\r\n");
  if (ZSSC4151_Clear_Status () != 0)
    {
      dbg_put_string ("Warning: Failed to clear status flags.\r\n");
    }

  // --- 3. ???? ??? ???? ????? ???? ?¬Ò? ??? ---
  if (!command_mode_ok)
    {
      dbg_put_string ("Cmd ??? ???? ????!\r\n");
      while (1); // ???? ?? ????
    }
  dbg_put_string ("Cmd ??? ???? ????!\r\n");

  uint16_t nvm_buffer[128];
  for (int i = 0; i < 128; i++)
    {
      nvm_buffer[i] = harsh_emc_nvm_config[i];
    }

  // --- 4. ??? ?¬Ò? ???? ???? ???? ?¬à??? ????? ??? ---
  dbg_put_string ("Reading factory-programmed values...\r\n");
  bool read_ok = true;
  if (ZSSC4151_ReadNvmWord (0x3D, &nvm_buffer[0x3D]) != 0) read_ok = false;

  for (int addr = 0x79; addr <= 0x7F; addr++)
    {
      if (ZSSC4151_ReadNvmWord (addr, &nvm_buffer[addr]) != 0)
        {
          read_ok = false;
          break;
        }
    }

  if (!read_ok)
    {
      dbg_put_string ("Failed to read factory values!\r\n");
      while (1);
    }
  dbg_put_string ("Factory values read OK.\r\n");

  // --- 5. ????? ?? ???? ?? CRC ???? ---
  nvm_buffer[0x2E] = 0x4444;
  nvm_buffer[0x2F] = 0x6666;

  uint16_t new_crc = Calculate_ZSSC_CRC (nvm_buffer);
  nvm_buffer[0x00] = new_crc;

  dbg_put_string ("OK: New CRC calculated: 0x");
  dbg_put_hex_word (new_crc);
  dbg_put_string ("\r\n");

  // --- 6. NVM ???? ??? ???? ---
  if (ZSSC4151_Unlock_NVM () != 0)
    {
      dbg_put_string ("NVRAM ???? ??? ???? ????!\r\n");
      while (1);
    }
  dbg_put_string ("NVRAM ???? ??? ???? ????!\r\n");

  // --- 7. NVM ?????? ???? ---
  dbg_put_string ("Writing data to NVM (0x00 to 0x78)...\r\n");
  bool write_ok = true;
  for (int addr = 0; addr < 0x79; addr++)
    {
      __asm__ volatile ("clrwdt");
      if (ZSSC4151_WriteNvmWord (addr, nvm_buffer[addr]) != 0)
        {
          dbg_put_hex_byte (addr);
          dbg_put_string ("\r\n");
          write_ok = false;
          break;
        }
    }

  if (!write_ok)
    {
      dbg_put_string ("Fatal: NVRAM write process aborted!\r\n");
      while (1);
    }
  dbg_put_string ("OK: NVRAM write commands sent.\r\n");

  // --- 8. NVM ???? ??? ???? ????? ??? ?©£? ---
  // ??? ???? ????? ?????????? ??????? ????
  dbg_put_string ("Waiting for NVM programming to complete...\r\n");
  delay_10ms (5); // 50ms ???

  // --- 9. NVM ?????? ???? ---
  dbg_put_string ("Verifying written data...\r\n");
  bool verify_ok = true;
  for (int addr = 0; addr < 0x79; addr++)
    {
      __asm__ volatile ("clrwdt");
      uint16_t read_val;
      if (ZSSC4151_ReadNvmWord (addr, &read_val) != 0)
        {
          dbg_put_string ("\r\nERROR: Read failed during verification at 0x");
          dbg_put_hex_byte (addr);
          dbg_put_string ("\r\n");
          verify_ok = false;
          break;
        }
      else
        {
          dbg_put_string ("\r\nPASS: Read OK!!! => 0x");
          dbg_put_hex_byte (addr);
          dbg_put_string (" | Wrote: 0x");
          dbg_put_hex_word (nvm_buffer[addr]);
          dbg_put_string (" | Read: 0x");
          dbg_put_hex_word (read_val);
        }

      if (read_val != nvm_buffer[addr])
        {
          dbg_put_string ("\r\nMISMATCH at 0x");
          dbg_put_hex_byte (addr);
          dbg_put_string (" | Wrote: 0x");
          dbg_put_hex_word (nvm_buffer[addr]);
          dbg_put_string (" | Read: 0x");
          dbg_put_hex_word (read_val);
          dbg_put_string ("\r\n");
          verify_ok = false;
        }
    }

  if (verify_ok) dbg_put_string ("\r\n???? ????!\r\n");
  else dbg_put_string ("\r\n???? ????!\r\n");

  dbg_put_string ("\r\n--- ZSSC4151 Normal Operation Mode ---\r\n");

  // ???? Normal Mode ???? ??, ???? ??? ????
  ZSSC4151_Check_Status ();

  // ???? ??? ???? 1: Normal Mode ???? ????? ???? ????
  if (ZSSC4151_Start_Normal_Mode () != 0)
    {
      dbg_put_string ("Fatal: Failed to start Normal Mode!\r\n");
      while (1);
    }
  dbg_put_string ("OK: Normal Mode Started. Acquiring data...\r\n\r\n");

  dbg_put_string ("\r\n    ---- ???? ??????, ZSSC4151 raw ?????? ??, SENT ?????? ??? ----\r\n\r\n");

  /*
  count = 1;

  while (1)
  {
      __asm__ volatile ("clrwdt");
      uint16_t revision_number;

      // --- ???? NVM?? ?????? ???? ?¬à? I2C ????? ???????? ???? ---
      // ?? ?????? ???? ????????? Command Mode?? ??? ????????.
        
      ZSSC4151_EnterCommandMode(); // ??? Command Mode??
      delay_10ms(10);
        
      if (ZSSC4151_ReadNvmWord(0x3D, &revision_number) == 0)
      {
          dbg_put_hex_word(count++);
          dbg_put_string(") NVM[0x3D] SVN Revision: 0x");
          dbg_put_hex_word(revision_number);
            
          // Harsh EMC ?????? ?????? 0x17AD ????.
          if (revision_number == 0x17AD) {
              dbg_put_string(" (OK, I2C communication is stable!)");
          } else {
              dbg_put_string(" (Warning: Incorrect value read!)");
          }
          dbg_put_string("\r\n");
      }
      else
      {
          dbg_put_string("Failed to read NVM[0x3D]. I2C communication may be unstable.\r\n");
      }

      // ???? ?? ??? Normal Mode?? ???? (???? ???©ª??????????? ?????)
      ZSSC4151_Start_Normal_Mode();

      delay_10ms(100);
  }    
   */

  while (1)
    {
      __asm__ volatile ("clrwdt");

      // ... (???? ???? ?????? ?¬Ò? ?????? ?????? ????) ...
      uint16_t adc_value, sense_value;
      uint8_t status_nibble = 0x1;
      float sht_temp, sht_humi;

      AD1CON1bits.SAMP = 1;
      delay_us (10);
      AD1CON1bits.SAMP = 0;

      while (!AD1CON1bits.DONE);
      adc_value = ADC1BUF0;

      uint8_t data_nibble1 = (adc_value >> 8) & 0x0F;
      uint8_t data_nibble2 = (adc_value >> 4) & 0x0F;
      uint8_t data_nibble3 = (adc_value >> 0) & 0x0F;

      SENT1DATH = ((uint16_t) status_nibble << 12) |
              ((uint16_t) data_nibble1 << 8) |
              ((uint16_t) data_nibble2 << 4) |
              ((uint16_t) data_nibble3 << 0);

      SENT1DATL = 0x0000;

      ZSSC4151_Read_Output_Data (0x01, &sense_value);

      SHT4x_Read_TH (&sht_temp, &sht_humi);
      dbg_put_hex_word (count++);
      dbg_put_string (") SHT4x T: ");
      dbg_put_float (sht_temp);
      dbg_put_string ("(??C), H: ");
      dbg_put_float (sht_humi);
      dbg_put_string ("(%), rawData(by ZSSC): 0x");
      dbg_put_hex_word (sense_value);
      dbg_put_string (" , SENT: 0x");
      dbg_put_hex_word (SENT1DATH);
      dbg_put_string ("\r\n");

      delay_10ms (50);
    }

  return 0;
}

/**
 * @brief ??? ????? ???? 32??? ?????? ?¬à???? Helper ???
 */
int
ZSSC4151_Read_Command_Response32 (uint8_t command, uint32_t *response_data)
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
int
ZSSC4151_Clear_Status (void)
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
void
ZSSC4151_Check_Status (void)
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

//

void
Check_Reset_Cause (void)
{
  dbg_put_string ("--- Last Reset Cause ---\r\n");

  // POR (Power-on Reset) ?¡À???? ?????? ???? ???????? ???? ????????.
  // ???? ??????? '????' ????????.
  if (RCONbits.POR)
    {
      dbg_put_string ("-> Power-on Reset (Normal Startup)\r\n");
    }
  else
    {
      // POR?? ?????, ??? ?????? ???? ?????? ???????.
      if (RCONbits.BOR)
        {
          dbg_put_string ("-> Brown-out Reset (Power Dip Detected!)\r\n");
        }
      if (RCONbits.WDTO)
        {
          dbg_put_string ("-> WATCHDOG TIMER RESET!\r\n");
        }
      if (RCONbits.EXTR)
        {
          dbg_put_string ("-> External Reset (MCLR Pin)\r\n");
        }
      if (RCONbits.CM)
        {
          dbg_put_string ("-> Configuration Mismatch Reset\r\n");
        }
      if (RCONbits.IOPUWR)
        {
          dbg_put_string ("-> Illegal Opcode / Uninitialized W Reg Reset\r\n");
        }
      if (RCONbits.TRAPR)
        {
          dbg_put_string ("-> Trap Conflict Reset\r\n");
        }
    }

  // ???? ???? ?????? ????? ?????? ???? ??? ?¡À???? ????????.
  RCON = 0;

  dbg_put_string ("------------------------\r\n\r\n");
}

/**
 * @brief ZSSC4151 NVRAM?? 16??? ??????? ???? ??? (?????? ????? ????)
 * @param reg_addr ??????? ?? NVRAM ???????? ???
 * @param data ?? 16??? ??????
 * @return 0: ????, -1: ????
 */

int
ZSSC4151_Write_NVM (unsigned char reg_addr, unsigned int data)
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

//

int
I2C1_SendByte (unsigned char data)
{
  I2C1TRN = data; // ?????? ??????? ???? ????????? ?¥å?
  while (I2C1STATbits.TBF); // ???? ????? ????? ?????? ???
  I2C1_WaitForIdle ();

  // ACK/NACK ???
  if (I2C1STATbits.ACKSTAT)
    { // 1??? NACK
      return -1;
    }
  return 0;
}

//

uint8_t
I2C1_ReadByte (bool ack)
{
  I2C1CON1bits.RCEN = 1;
  while (I2C1CON1bits.RCEN);

  I2C1CON1bits.ACKDT = ack ? 0 : 1;
  I2C1CON1bits.ACKEN = 1;

  while (I2C1CON1bits.ACKEN);

  return I2C1RCV;
}

// ?????? ZSSC4151_Probe_CM ???

bool
ZSSC4151_Probe_CM (void)
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

/**
 * @brief ZSSC4151?? NVM ???? ?????? ?????? ?¬à? ?©ª???? ???????.
 */
void
Dump_NVM_Map (void)
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

//

void
I2C_Scan (void)
{
  dbg_put_string ("I2C scan start...\r\n");
  for (uint8_t addr7 = 0x08; addr7 <= 0x77; addr7++)
    {
      uint8_t w = (addr7 << 1) | 0; // write
      if (!I2C1_Start ())
        {
          I2C1_Stop ();
          continue;
        }
      bool ok = I2C1_Write (w); // ACK?? true
      I2C1_Stop ();

      if (ok)
        {
          dbg_put_string ("  ACK @ 0x");
          dbg_put_hex_byte (addr7);
          dbg_put_string (" (W=0x");
          dbg_put_hex_byte (w);
          dbg_put_string (", R=0x");
          dbg_put_hex_byte (w | 1);
          dbg_put_string (")\r\n");
        }
    }
  dbg_put_string ("I2C scan done.\r\n");
}

//

void
Init_Hw ()
{
  RCONbits.SWDTEN = 0; // ?????????? ????? ???? ??????

  t2_tick = 0;
  t1_tick = 0;
  //
  // Timer 1 to generate an interrupt every 250ms
  //
  T1CONbits.TON = 0; // Disable Timer1
  T1CONbits.TCS = 0; // Select internal instruction cycle clock
  T1CONbits.TGATE = 0; // Disable Gated Timer mode
  T1CONbits.TCKPS = 0x3; // Select 1:256 Prescaler
  PR1 = 39062; // Load the period value (250ms/(256*25ns))
  TMR1 = 0;
  IPC0bits.T1IP = 0x03; // Set Timer 1 Interrupt Priority Level
  IFS0bits.T1IF = 0; // Clear Timer 1 Interrupt Flag
  IEC0bits.T1IE = 1; // Enable Timer1 interrupt

  //
  // Timer 2 to generate an interrupt every 10ms
  //
  T2CONbits.TON = 0; // Disable Timer2
  T2CONbits.TCS = 0; // Select internal instruction cycle clock
  T2CONbits.TGATE = 0; // Disable Gated Timer mode
  T2CONbits.TCKPS = 0x3; // Select 1:256 Prescaler
  PR2 = 1562; // Load the period value (10ms/(256*25ns))
  TMR2 = 0x00; // Clear timer register    
  IPC1bits.T2IP = 0x02; // Set Timer 2 Interrupt Priority Level
  IFS0bits.T2IF = 0; // Clear Timer 2 Interrupt Flag
  IEC0bits.T2IE = 1; // Enable Timer2 interrupt

  T3CONbits.TON = 0; // Stop Timer3
  T3CONbits.TCS = 0; // ?? ??
  T3CONbits.TGATE = 0; // ??? ?? ?
  T3CONbits.TCKPS = 0x1; // Prescaler = 1:8 (tick=200ns)

  TMR3 = 0;
  PR3 = 0xFFFF; // ??? (??? delay_us()?? ???)

  IFS0bits.T3IF = 0; // ???? ??? ???
  IEC0bits.T3IE = 1; // Timer3 ???? enable    

  T1CONbits.TON = 1; // Start Timer1
  T2CONbits.TON = 1; // Start Timer2    
}

//

void
oscConfig (void)
{
  // Configure PLL for FOSC = 80MHz from 8MHz crystal
  PLLFBD = 38; // M = 40, Fcyc = 40MHz
  CLKDIVbits.PLLPRE = 0; // N1 = 2
  CLKDIVbits.PLLPOST = 0; // N2 = 2

  // Initiate Clock Switch to Primary Oscillator with PLL (NOSC=0b011)
  __builtin_write_OSCCONH (0x03);
  __builtin_write_OSCCONL (OSCCON | 0x01);

  // Wait for Clock switch to occur
  while (OSCCONbits.COSC != 0b011);

  // Wait for PLL to lock
  while (OSCCONbits.LOCK != 1);
}
// REFCLKO ?? ?? ?? (dsPIC33EV128GM002)

/**
 * Initializes and enables the Reference Clock Output on pin RB4.
 */
void
REFCLKO_Init (void)
{
  // --- Peripheral Pin Select (PPS) ---
  __builtin_write_OSCCONL (OSCCON & ~_OSCCON_IOLOCK_MASK);
  // Map REFCLKO function (code 49) to the RB4 pin (RP36)
  RPOR1bits.RP36R = 0x31; // 0x31 is the code for REFCLKO
  __builtin_write_OSCCONL (OSCCON | _OSCCON_IOLOCK_MASK);

  // --- Configure Reference Oscillator ---
  REFOCONbits.ROSEL = 0; // Select System Clock (FOSC) as the source
  REFOCONbits.RODIV = 0; // Do not divide the clock (Divider = 1)
  REFOCONbits.ROON = 1; // Enable the Reference Clock Output
}

//

void
clearIntrflags (void)
{
  /* Clear Interrupt Flags */

  IFS0 = 0;
  IFS1 = 0;
  IFS2 = 0;
  IFS3 = 0;
  IFS4 = 0;
  IPC16bits.U1EIP = 6; //service the LIN framing error before the RX
  IPC2bits.U1RXIP = 4;
}

//

void
delay_10ms (uint16_t num)
{
  t2_tick = 0;
  while (t2_tick < num)
    {
      __asm__ volatile ("nop");
    }
}

//

void
delay_us (unsigned int us)
{
  unsigned int pr;

  // tick = 200ns, 1us = 5 ticks
  pr = us * 5;
  if (pr > 65535) pr = 65535;

  TMR3 = 0; // ??? ???
  PR3 = pr; // ?? ?? ??

  t3_done = 0;
  T3CONbits.TON = 1; // ??? ??

  while (!t3_done); // ??? ??? ??
}

/*****************************************************/

/**
 * UART1 ??? ????? ??
 */
void
UART1_Init (void)
{
  TRISBbits.TRISB11 = 0;
  RPOR4bits.RP43R = 1; // U1TX to RB11 (RP43)

  // --- UART Mode ?? ---
  U1MODEbits.STSEL = 0; // 1-Stop bit
  U1MODEbits.PDSEL = 0; // No Parity, 8-Data bits
  U1MODEbits.ABAUD = 0; // Auto-Baud Disabled
  U1MODEbits.BRGH = 0; // Standard-Speed mode

  // --- UART Status and Control ?? ---
  U1STAbits.URXISEL = 0; // Interrupt after one RX character is received

  U1BRG = BAUD38400;

  // --- UART ?? ??? ---
  U1MODEbits.UARTEN = 1; // Enable UART
  U1STAbits.UTXEN = 1; // Enable UART TX

  delay_10ms (1);
}

//

void
dbg_putc (char c)
{
  while (U1STAbits.UTXBF); // ?? ??? ?? ? ?? ??
  U1TXREG = c;
}

/**
 * @brief 1?????(8???) ???? 2??? 16???? ??????? ???????. (??: 0xFE)
 * @param val ????? 8??? ??
 */
void
dbg_put_hex_byte (uint8_t val)
{
  const char hex_digits[] = "0123456789ABCDEF";
  char buffer[3]; // "FF" + NULL ????('\0')

  // ???? 4??? (? ??¡Æ ???)
  buffer[0] = hex_digits[(val >> 4) & 0x0F];

  // ???? 4??? (?? ??¡Æ ???)
  buffer[1] = hex_digits[val & 0x0F];

  // ????? ????
  buffer[2] = '\0';

  dbg_put_string (buffer);
}

/**
 * @brief 2?????(16???) ???? 4??? 16???? ??????? ???????. (??: 0xBEEF)
 * @param val ????? 16??? ??
 */
void
dbg_put_hex_word (uint16_t val)
{
  const char hex_digits[] = "0123456789ABCDEF";
  char buffer[5]; // "FFFF" + NULL ????('\0')

  buffer[0] = hex_digits[(val >> 12) & 0x0F]; // ????? 4???
  buffer[1] = hex_digits[(val >> 8) & 0x0F];
  buffer[2] = hex_digits[(val >> 4) & 0x0F];
  buffer[3] = hex_digits[val & 0x0F]; // ?????? 4???

  buffer[4] = '\0';

  dbg_put_string (buffer);
}

//

void
dbg_put_string (char *s)
{
  while (*s)
    { // loop until *s =\0, end of string
      dbg_putc (*s++);
    } // send the character and point to the next one
}

// UART1 float ???

void
dbg_put_float (float temp)
{
  // ?? ??
  if (temp < 0)
    {
      dbg_putc ('-');
      temp = -temp;
    }

  // ?? ??
  int int_part = (int) temp;
  int dec_part = (int) ((temp - int_part) * 100 + 0.5);

  // ??? ??
  if (dec_part >= 100)
    {
      int_part++;
      dec_part = 0;
    }

  // ?? ?? ??
  if (int_part == 0)
    {
      dbg_putc ('0');
    }
  else
    {
      static char buffer[12];
      int i = 0;

      while (int_part > 0)
        {
          buffer[i++] = (int_part % 10) + '0';
          int_part /= 10;
        }

      while (i > 0) dbg_putc (buffer[--i]);
    }

  // ???? ?? ?? ??
  dbg_putc ('.');
  dbg_putc ((dec_part / 10) + '0');
  dbg_putc ((dec_part % 10) + '0');
}

// --- I2C Driver Functions ---
// ### ????? ?¥ê?: I2C ???? ???? ??? ###

void
I2C1_BusReset (void)
{
  uint8_t i;

  I2C1CON1bits.I2CEN = 0; // I2C ??? ??????

  // ???? ???????? ????
  TRISBbits.TRISB5 = 0; // SDA?? ???????
  LATBbits.LATB5 = 1; // SDA?? High??

  TRISBbits.TRISB6 = 0; // SCL?? ???????

  // 9???? ??? ??? ????
  for (i = 0; i < 9; i++)
    {
      LATBbits.LATB6 = 0; // SCL low
      delay_us (1000);
      LATBbits.LATB6 = 1; // SCL high
      delay_us (1000);
    }

  // Stop ??? ???? ????
  I2C1_Start ();
  I2C1_Stop ();

  I2C1CON1bits.I2CEN = 0; // I2C ??? ?????? ?? ?????? ???

  // ???? ??? ??????? (I2C ??? ????)
  TRISBbits.TRISB6 = 1;
  TRISBbits.TRISB5 = 1;
}

//

void
I2C1_Init (void)
{
  // 1. I2C ?? ???? (?? ?)
  I2C1CONLbits.I2CEN = 0;

  TRISBbits.TRISB5 = 1; // SDA1 (???)
  TRISBbits.TRISB6 = 1; // SCL1 (???)

  // 6. I2C ?? ?? ??
  I2C1BRG = (FCY / I2C1_CLOCK) - (FCY / 10000000) - 1;

  I2C1CONLbits.DISSLW = 1; // Slew Rate disable (100kHz ????? ?? ????)
  I2C1CONLbits.ACKDT = 0; // ACK = 0 ????
  I2C1CONLbits.I2CEN = 1; // I2C ??? Enable    
}

//

void
I2C1_WaitForIdle (void)
{
  while (I2C1STATbits.TRSTAT || (I2C1CONL & 0x1F));
}

//

bool
I2C1_Start (void)
{
  I2C1_WaitForIdle ();
  I2C1CONLbits.SEN = 1;
  while (I2C1CONLbits.SEN);

  return true;
}

//

bool
I2C1_Stop (void)
{
  I2C1_WaitForIdle ();
  I2C1CONLbits.PEN = 1;
  while (I2C1CONLbits.PEN);
  return true;
}

//

bool
I2C1_Restart (void)
{
  I2C1_WaitForIdle ();
  I2C1CONLbits.RSEN = 1;
  while (I2C1CONLbits.RSEN);
  return true;
}

//
//

bool
I2C1_Write (uint8_t data)
{
  I2C1_WaitForIdle ();
  I2C1TRN = data;

  while (I2C1STATbits.TBF); // TBF(Transmit Buffer Full) ?¡À??? ???
  I2C1_WaitForIdle ();

  if (I2C1STATbits.ACKSTAT) return false;
  return true;
}

//

uint8_t
I2C1_Read (bool send_ack)
{
  I2C1_WaitForIdle ();
  I2C1CONLbits.RCEN = 1; // ???? ????
  while (!I2C1STATbits.RBF); // RBF(Receive Buffer Full) ?¡À??? ???

  uint8_t data = I2C1RCV;

  I2C1_WaitForIdle ();
  I2C1CONLbits.ACKDT = send_ack ? 0 : 1; // ACK ??? NACK ????
  I2C1CONLbits.ACKEN = 1;
  while (I2C1CONLbits.ACKEN);

  return data;
}

//

void
I2C1_Ack (void)
{
  I2C1_WaitForIdle ();

  I2C1CONLbits.ACKDT = 0; // ACK
  I2C1CONLbits.ACKEN = 1;

  while (I2C1CONLbits.ACKEN);
}

//

void
I2C1_Nack (void)
{
  I2C1_WaitForIdle ();

  I2C1CONLbits.ACKDT = 1; // NACK
  I2C1CONLbits.ACKEN = 1;

  while (I2C1CONLbits.ACKEN);
}

// --- CRC ??? ??? (Sensirion CRC-8, poly=0x31, init=0xFF) ---

static uint8_t
SHT4x_CalcCRC (uint8_t *data, uint8_t len)
{
  uint8_t crc = 0xFF;
  for (uint8_t i = 0; i < len; i++)
    {
      crc ^= data[i];
      for (uint8_t j = 0; j < 8; j++)
        {
          if (crc & 0x80) crc = (crc << 1) ^ 0x31;
          else crc <<= 1;
        }
    }
  return crc;
}

// --- ???? ?? ?¬Ò? ??? ---

bool
SHT4x_Read_TH (float *T, float *H)
{
  uint8_t raw_data[6];
  unsigned int raw_temp, raw_humi;

  // 1. ???? ???? ??????
  if (!I2C1_Start ()) return false;
  if (!I2C1_Write (SHT4X_WRITE_ADDR))
    {
      I2C1_Stop ();
      return false;
    }
  if (!I2C1_Write (SHT4X_CMD_MEASURE_HPM))
    {
      I2C1_Stop ();
      return false;
    }
  I2C1_Stop ();

  // 2. ???? ??? (??? 15ms ??? ?? ??????? 20ms)
  delay_10ms (2);

  // 3. ?????? ?¬Ò? (6?????: Temp[2]+CRC + Humi[2]+CRC)
  if (!I2C1_Start ()) return false;
  if (!I2C1_Write (SHT4X_READ_ADDR))
    {
      I2C1_Stop ();
      return false;
    }

  for (int i = 0; i < 5; i++)
    raw_data[i] = I2C1_Read (true);
  raw_data[5] = I2C1_Read (false); // ?????? ??????? NACK

  I2C1_Stop ();

  // 4. CRC ????
  if (SHT4x_CalcCRC (raw_data, 2) != raw_data[2]) return false;
  if (SHT4x_CalcCRC (&raw_data[3], 2) != raw_data[5]) return false;

  // 5. ?????? ???
  raw_temp = ((uint16_t) raw_data[0] << 8) | raw_data[1];
  raw_humi = ((uint16_t) raw_data[3] << 8) | raw_data[4];

  *T = -45.0f + 175.0f * ((float) raw_temp / 65535.0f);
  *H = -6.0f + 125.0f * ((float) raw_humi / 65535.0f);

  if (*H > 100.0f) *H = 100.0f;
  if (*H < 0.0f) *H = 0.0f;

  return true;
}

//==============================================================================
// ZSSC4151 NVM ?? ?? ??
//==============================================================================
//

int
ZSSC4151_EnterCommandMode (void)
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
int
ZSSC4151_Start_Normal_Mode (void)
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

void
Crc16_update (uint16_t *crc, uint8_t data)
{
  *crc = *crc ^ ((uint16_t) data << 8);

  for (int i = 0; i < 8; i++)
    {
      if (*crc & 0x8000) *crc = (*crc << 1) ^ 0x1021;
      else *crc = (*crc << 1);
    }
}

//

uint16_t
Calculate_ZSSC_CRC (const uint16_t *nvm_data)
{
  uint16_t crc = 0xFFFF;

  // ???? ?????? ?¥ê?: CRC ??? ?????? 0x01???? 0x78?????? ???? ????
  for (int i = 1; i < 0x79; i++)
    {
      uint16_t word = nvm_data[i];
      Crc16_update (&crc, (uint8_t) (word >> 8));
      Crc16_update (&crc, (uint8_t) (word & 0xFF));
    }

  return crc;
}


//

bool
ZSSC4151_Read_Normal_Mode_Output (uint16_t* read_word)
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
int
ZSSC4151_Read_Output_Data (uint8_t ram_addr, uint16_t *read_data)
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

bool
ZSSC4151_ReadNvm (uint8_t addr, uint16_t *val)
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
int
ZSSC4151_Unlock_NVM (void)
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
int
ZSSC4151_ReadNvmWord (uint8_t nvm_addr, uint16_t *read_data)
{
  const uint8_t CMD_READ_NVM = 0x24;
  uint8_t command_echo, msb, lsb;

  // --- 1???: ???? ???? ??????? ???? ?????? ---
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

  // --- 2???: Restart?? ?¬Ò? ??? ??? ---
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
int
ZSSC4151_WriteNvmWord (uint8_t nvm_addr, uint16_t write_data)
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

//

void
SENT1_TX_Init (void)
{
  RPOR4bits.RP43R = 0b0110101;

  TRISBbits.TRISB11 = 0; // RB14 ???? ?????? ??????? ????

  // Set up SENT interrupts
  IPC45bits.SENT1IP = 5; // SENT TX/RX completion interrupt priority
  IFS11bits.SENT1IF = 0; // Clear SENT TX/RX completion interrup flag
  IEC11bits.SENT1IE = 1; // Enable SENT TX/RX completion interrupt

  IPC45bits.SENT1EIP = 6; // SENT ERROR interrupt priority
  IFS11bits.SENT1EIF = 0; // Clear SENT ERROR interrup flag
  IEC11bits.SENT1EIE = 1; // Enable SENT ERROR interrupt

  // Initialize SENT registers for transmit mode (no frame time specified due to no pause)
  SENT1CON2 = (int) (tickTime * peripheralClk) - 1;
  SENT1CON1bits.TXM = 1; // sync handshaking mode
  SENT1CON1bits.CRCEN = 1; // CRC enable, 0=off, 1=on
  SENT1CON1bits.PPP = 0; // Pause, 0=off, 1=on
  SENT1CON1bits.NIBCNT = 6; // nibbles of data
  SENT1CON1bits.SNTEN = 1; // enable SENT module
  SENT1DATH = 0;
  SENT1DATL = 0; // initialize the SENT data registers

}

//

void
ADC_Init (void)
{
  // 1. Configure AN0 pin as analog input
  TRISAbits.TRISA0 = 1; // RA0 (AN0) pin as input
  ANSELAbits.ANSA0 = 1;

  // 2. Configure AD1CON1 register
  AD1CON1bits.ADON = 0; // Turn ADC OFF before configuration
  AD1CON1bits.ADSIDL = 0; // Continue module operation in Idle mode
  AD1CON1bits.FORM = 0b00; // Data Output Format: Integer
  AD1CON1bits.SSRC = 0b111; // Conversion Trigger: Internal counter ends sampling and starts conversion (auto-convert)
  AD1CON1bits.ASAM = 0; // Sampling begins when SAMP bit is manually set

  // 3. Configure AD1CON2 register
  AD1CON2bits.VCFG = 0b000; // Voltage Reference: AVdd and AVss
  AD1CON2bits.CSCNA = 0; // Do not scan inputs

  // 4. Configure AD1CON3 register
  AD1CON3bits.ADRC = 0; // Clock Source: System Clock
  AD1CON3bits.SAMC = 16; // Auto Sample Time: 16 TAD
  AD1CON3bits.ADCS = 3; // ADC Conversion Clock: TAD = 4 * Tcy (Tcy = 1/FCY)

  // 5. Configure AD1CHS0 register
  AD1CHS0bits.CH0SA = 0; // Select AN0 for CH0 positive input

  // 6. Turn ADC ON
  AD1CON1bits.ADON = 1;
  delay_us (20); // Wait for ADC stabilization
}

//

void
InitSENT1_TX (void)
{
#if 0    
  //
  // initialize the SENT hardware port on the Starter Kit
  //
  RPOR8bits.RP69R = 0x39; // map SENT1 transmitter to pin RD5, low byte
  _TRISD5 = 0; // digital output pin

  // Set up SENT interrupts
  IPC45bits.SENT1IP = 5; // SENT TX/RX completion interrupt priority
  IFS11bits.SENT1IF = 0; // Clear SENT TX/RX completion interrup flag
  IEC11bits.SENT1IE = 1; // Enable SENT TX/RX completion interrupt

  IPC45bits.SENT1EIP = 6; // SENT ERROR interrupt priority
  IFS11bits.SENT1EIF = 0; // Clear SENT ERROR interrup flag
  IEC11bits.SENT1EIE = 1; // Enable SENT ERROR interrupt

  // Initialize SENT registers for transmit mode (no frame time specified due to no pause)
  SENT1CON2 = (int) (tickTime * peripheralClk) - 1;
  SENT1CON1bits.TXM = 1; // sync handshaking mode
  SENT1CON1bits.CRCEN = 1; // CRC enable, 0=off, 1=on
  SENT1CON1bits.PPP = 0; // Pause, 0=off, 1=on
  SENT1CON1bits.NIBCNT = 6; // nibbles of data
  SENT1CON1bits.SNTEN = 1; // enable SENT module
  SENT1DATH = 0;
  SENT1DATL = 0; // initialize the SENT data registers
#endif
}

/* code for Timer1 ISR, called every 250ms*/
void __attribute__ ((__interrupt__, no_auto_psv))
_T1Interrupt (void)
{
  t1_tick++; // increment the 'slow tick'

  IFS0bits.T1IF = 0; //Clear Timer1 interrupt flag

}

/* code for Timer2 ISR, called every 10ms*/
void __attribute__ ((__interrupt__, no_auto_psv))
_T2Interrupt (void)
{
  t2_tick++; // we increment the variable f_tick

  IFS0bits.T2IF = 0; //Clear Timer2 interrupt flag

}

//

void __attribute__ ((__interrupt__, no_auto_psv))
_T3Interrupt (void)
{
  IFS0bits.T3IF = 0;
  T3CONbits.TON = 0; // ??? ??
  t3_done = 1; // ?? ??? set
}

/******************************************************************************
 * Function:        SENT1 Tx/Rx Interrupt
 *****************************************************************************/
void __attribute__ ((__interrupt__, __auto_psv__))
_SENT1Interrupt (void)
{
  /* Interrupt Service Routine code goes here */
  if (SENT1CON1bits.RCVEN == 1) // was a RX message?
    {
      // Read data from SENT registers
      datal = (SENT1DATL >> 4); // Format to 12 bit data
      datah = SENT1DATH; // switch data + pot

      sent_rx = 1; // a message was received
    };

  IFS11bits.SENT1IF = 0; // clear interrupt flag
}
