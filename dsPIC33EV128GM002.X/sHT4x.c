/*
 * File:   sHT4x.c
 * Author: easyf
 *
 * Created on 2025년 8월 31일 (일), 오전 5:13
 */
#include <stdbool.h>
#include <stdint.h> 
#include "..\h\sHT4x.h"

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

// --- CRC ??? ??? (Sensirion CRC-8, poly=0x31, init=0xFF) ---
static uint8_t SHT4x_CalcCRC (uint8_t *data, uint8_t len)
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

// --- ???? ?? ?б? ??? ---

bool SHT4x_Read_TH (float *T, float *H)
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

  // 3. ?????? ?б? (6?????: Temp[2]+CRC + Humi[2]+CRC)
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