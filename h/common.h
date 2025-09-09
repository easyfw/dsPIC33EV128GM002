#ifndef COMMON_H_ // Start of COMMON_H_ 
#define COMMON_H_

#include <stdbool.h>
#include <stdint.h> 

// TEST Option
#define TEST_CLK                                            0       // CLOCK Redirection
#define NVRAM_N_UART                                 0      // 1 = NVRAM Test Start

// only generate SENT Pulse Waveform (SENT=O, UART=x))
#define SENT_PULSE_ONLY                             0  // 0 : SENT=x, UART=O, 1 : SENT=O, UART=x

#define AFTER_1_TIME_WRITING_NVRAM      1    // After First 1 time NVRAM wring, it should be set to 0. 

// CLOCK
#define FCAN                                    40000000UL      // Fcyc = 1/2Fpll
#define FCY                                       40000000UL      // 

// I2C/UART
#define BAUD38400                           ((FCAN/38400)/16) - 1   // this is what the demo UART serial baud rate is
#define I2C1_CLOCK                          100000L                             // 100kHz

#define NUM_DIGITS                          5                        // floating point digits to print
#define STRING_BUFFER_SIZE          64                      // arbitrary length message buffer

// CLOCK
void oscConfig (void);

// HW initialize
void Init_Hw ();
void REFCLKO_Init (void);

void SENT1_TX_Init (void);
uint8_t Calculate_SENT_CRC(uint8_t data_nibbles[]);

void ADC_Init (void);

// Send a character to the serial port
void UART1_Init (void);

// Message printf()
void dbg_putc (char c);
void dbg_put_hex_byte (uint8_t val);
void dbg_put_hex_word (uint16_t val);
void dbg_put_float (float temp);
void dbg_put_string (char *s);
void dbg_put_dec_byte(uint8_t value);
void dbg_put_dec_word(uint16_t value);

// Delay
void delay_10ms (uint16_t num);
void delay_us (unsigned int us);

// I2C
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


#endif // End of COMMON_H_ 