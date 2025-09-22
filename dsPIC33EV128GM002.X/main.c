/*
 * File:   main.c
 * Author: easyf
 *
 * Created on 2025/8/13
 */
#include <stdio.h>

#include <p33EV128GM002.h>
#include "..\h\main.h"
#include "..\h\common.h"
#include "..\h\zSSC4151.h"
#include "..\h\sHT4x.h"

// --- �а��� �ϴ� RAM ���� ���� ---
#define RAM_START_ADDRESS_TO_READ 0x00
#define RAM_WORD_COUNT_TO_READ    0x18+1 // 0���� 20������ �� 21�� ����

//
int main(void)
{
    // 1. System Initialization
    oscConfig();
    Init_Hw();
    I2C1_Init();
    
#if     UART_ENABLE    
    UART1_Init();
#else
    SENT1_TX_Init (void);    
#endif    
    
#if     NVRAM_WRITE_TEST
    bool cmd_mode_ok = (ZSSC4151_EnterCommandMode () == 0);
    dbg_put_string ("Entering Command Mode to program NVM...\r\n");
        
    if (ZSSC4151_Clear_Status () != 0) dbg_put_string ("Warning: Failed to clear status flags.\r\n");
        
    if (!cmd_mode_ok)
    {
        dbg_put_string ("Fatal: Failed to Enter Command Mode!!\r\n");
        while (1); 
    }
    dbg_put_string ("Command Mode is OK!!\r\n");

    uint16_t nvm_buffer[128];
    
#if NVM_RESPONSE_TIME    
    for (int i = 0; i < 128; i++) nvm_buffer[i] = nvm_response_time_config[i];
#else
    for (int i = 0; i < 128; i++) nvm_buffer[i] = harsh_emc_nvm_config[i];
#endif    
    
    // Check Read Only Value
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

    uint16_t new_crc = Calculate_ZSSC_CRC (nvm_buffer);
    nvm_buffer[0x00] = new_crc;

    dbg_put_string ("OK: New CRC calculated: 0x");
    dbg_put_hex_word (new_crc);
    dbg_put_string ("\r\n");
         
    if (ZSSC4151_Unlock_NVM () != 0)
    {
        dbg_put_string ("NVRAM Unlock is failure!!\r\n");
        while (1);
    }
    dbg_put_string ("NVRAM Unlock is OK!!\r\n");

    // Write to NVRRAM
    dbg_put_string ("Writing data to NVM (0x00 to 0x78)...\r\n");
        
    bool write_ok = true;
    for (int addr = 0; addr < 0x79; addr++)
    {
        if (ZSSC4151_WriteNvmWord (addr, nvm_buffer[addr]) != 0)
        {
            dbg_put_hex_byte (addr);
            dbg_put_string ("\r\n");
            write_ok = false;
            break;
        }
        delay_10ms(1);
    }

    if (!write_ok)
    {
        dbg_put_string ("Fatal: NVRAM write process aborted!\r\n");
        while (1);
    }
    dbg_put_string ("OK: NVRAM write commands sent.\r\n");

    dbg_put_string ("Waiting for NVM programming to complete...\r\n");
    delay_10ms (5);      // waiting for stable

    // Verification
    dbg_put_string ("Verifying written data...\r\n");
        
    bool verify_ok = true;
   
    for (int addr = 0; addr < 0x79; addr++)
    {
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
            dbg_put_hex_byte (addr);
            dbg_put_string (" | Wrote: 0x");
            dbg_put_hex_word (nvm_buffer[addr]);
            dbg_put_string (" | Read: 0x");
            dbg_put_hex_word (read_val);
            dbg_put_string ("\r\n");
        }
        
    }

    if (verify_ok) dbg_put_string ("\r\nVerification PASS!!\r\n");
    else 
    {
        dbg_put_string ("\r\nVerification FAIL!!\r\n");
        while(1);
    }

    dbg_put_string("Programming complete. Starting Normal Mode...\r\n");
        
    
    if (ZSSC4151_Start_Normal_Mode() != 0) 
    {
        dbg_put_string("Fatal: Failed to start Normal Mode after NVM programming.\r\n");
        while(1);
    }
        
    delay_10ms(10); 
    dbg_put_string("Chip is now in Normal Mode.\r\n");      
    while(1);    
#endif    
        
#if         RAM_READ_TEST    
    dbg_put_string("\r\nSystem Initialized.\n");
    dbg_put_string("Starting ZSSC4151 RAW RAM Read Test...\n\n");
    
    delay_10ms(10);

    static uint8_t ram_buffer[RAM_WORD_COUNT_TO_READ * 2];
    float temp = 0.0f, humi = 0.0f;
    int16_t br_val;   
     
    // 2. Main Test Loop
    while(1)
    {
        // 2.1. Enter Command Mode
        if (ZSSC4151_EnterCommandMode() != 0) 
        {
            dbg_put_string(" -> Failed to enter Command Mode!\n\n");
            delay_10ms(100); 
            while(1);
        }
        dbg_put_string("1. Entered Command Mode.\n");
        
        // 2.2. Clear Status Flags
        ZSSC4151_Clear_Status();
        dbg_put_string("2. Clearing status flags...\n");

        // 2.3. Copy NVM configuration to Shadow RAM
        if (ZSSC4151_CopyNvmToShadow() != 0) 
        {
            dbg_put_string(" -> Failed to copy NVM to Shadow RAM!\n\n");
            delay_10ms(100); 
            while(1);            
        }
        delay_10ms(1); 
        dbg_put_string("3. Copied NVM to Shadow RAM.\n");

        // 2.4. Start Measurement Cycle
        if (ZSSC4151_StartMeasCycle() != 0) 
        {
            dbg_put_string(" -> Failed to send StartMeasCycle command!\n\n");
            delay_10ms(100); 
            while(1);
        }
        dbg_put_string("4. StartMeasCycle command sent.\n");
        delay_10ms(2); // Wait for measurements to complete.

        // 2.5. [Diagnostics] Check Final Chip Status
        dbg_put_string("5. Checking FINAL chip status...\n");
        ZSSC4151_Check_Status_Registers();

        // 2.6. Read Raw Data from RAM
        dbg_put_string("6. Reading RAM data...\n");
        int result = ZSSC4151_ReadRam_Corrected(RAM_START_ADDRESS_TO_READ, RAM_WORD_COUNT_TO_READ, ram_buffer);
        bool sht_ok = SHT4x_Read_TH(&temp, &humi);   
        
        if (result == 0)
        {
            dbg_put_string(" -> Read Success!\n");
            
            // Print the read data
            for (int i = 0; i < RAM_WORD_COUNT_TO_READ; i++)
            {
                // Combine two bytes into a 16-bit word (assuming Big Endian from sensor)
                int16_t word_data = ((int16_t)ram_buffer[i*2] << 8) | (uint8_t)ram_buffer[i*2 + 1];
                
                dbg_put_string("[0x");
                dbg_put_hex_word(RAM_START_ADDRESS_TO_READ + i);
                dbg_put_string("] 0x");
                dbg_put_hex_word(word_data);
                dbg_put_string(",  ");
            }
        }
        else dbg_put_string(" -> Read Failed!\n");
        dbg_put_string("\r\n");
        
        if (!sht_ok) 
        {
            dbg_put_string("Error: Failed to get SHT4x Value.\r\n");
            temp = 0.0f;
            humi = 0.0f;
        }      
        
        int16_t data_0x01 = ((int16_t)ram_buffer[2] << 8) | (uint8_t)ram_buffer[3];        
        int16_t data_0x15 = ((int16_t)ram_buffer[42] << 8) | (uint8_t)ram_buffer[43];             
        br_val = data_0x01 - data_0x15;        
        
        uint8_t sense_int = (uint8_t)br_val;
        uint8_t sense_digit1 = (sense_int / 10) % 10;
        uint8_t sense_digit2 = sense_int % 10;        
        
        uint8_t temp_int = (uint8_t)temp;
        uint8_t temp_digit1 = (temp_int / 10) % 10;
        uint8_t temp_digit2 = temp_int % 10;
        uint8_t humi_int = (uint8_t)humi;
        uint8_t humi_digit1 = (humi_int / 10) % 10;
        uint8_t humi_digit2 = humi_int % 10;                
        
        // --- SENT 
        uint8_t status_nibble = 0x1;
        SENT1DATH = ((uint16_t)status_nibble << 12) | ((uint16_t)sense_digit1 << 8) | ((uint16_t)sense_digit2 << 4) | ((uint16_t)temp_digit1);        
        SENT1DATL = ((uint16_t)temp_digit2 << 12) | ((uint16_t)humi_digit1 << 8) | ((uint16_t)humi_digit2 << 4);

        SENT1STATbits.SYNCTXEN = 1;
        while(SENT1STATbits.SYNCTXEN == 1);

        static uint16_t count = 0; 
        dbg_put_dec_word (++count);
        dbg_put_string (") br_val: ");       
        dbg_put_dec_word(br_val);
        dbg_put_string(", Temp: ");
        dbg_put_float(temp);
        dbg_put_string(" C, Humi: ");
        dbg_put_float(humi);
        dbg_put_string(" % -> SENT Frame: 0x");
        dbg_put_hex_word(SENT1DATH);
        dbg_put_hex_word(SENT1DATL);
        dbg_put_string("\r\n");                 
        
        delay_10ms(200); // Wait 2 seconds before the next cycle
    }
#endif
    
    return 0;
}

/* code for Timer1 ISR, called every 250ms*/
void __attribute__ ((__interrupt__, no_auto_psv)) _T1Interrupt (void)
{
    t1_tick++; // increment the 'slow tick'
    IFS0bits.T1IF = 0; //Clear Timer1 interrupt flag
}

/* code for Timer2 ISR, called every 10ms*/
void __attribute__ ((__interrupt__, no_auto_psv)) _T2Interrupt (void)
{
    t2_tick++; // we increment the variable f_tick
    IFS0bits.T2IF = 0; //Clear Timer2 interrupt flag
}

//
void __attribute__ ((__interrupt__, no_auto_psv)) _T3Interrupt (void)
{
    IFS0bits.T3IF = 0;
    T3CONbits.TON = 0; // Ÿ�̸� ����
    t3_done = 1; // �Ϸ� �÷��� set
}

/******************************************************************************
 * Function:        SENT1 Tx/Rx Interrupt
 *****************************************************************************/
void __attribute__ ((__interrupt__, __auto_psv__)) _SENT1Interrupt (void)
{
    IFS11bits.SENT1IF = 0; // clear interrupt flag
}