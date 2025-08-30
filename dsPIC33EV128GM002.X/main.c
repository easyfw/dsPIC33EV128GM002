/*
 * File:   main.c
 * Author: easyf
 *
 * Created on 2025/8/13
 */
#include <stdio.h>

#include <p33EV128GM002.h>
#include "main.h"
#include "common.h"
#include "zSSC4151.h"
#include "sHT4x.h"

//
int main (void)
{
    oscConfig ();

    Init_Hw ();
    UART1_Init ();
    I2C1_Init ();

    REFCLKO_Init ();
    ADC_Init ();
    SENT1_TX_Init ();

    bool cmd_mode_ok = (ZSSC4151_EnterCommandMode () == 0);

    dbg_put_string ("Clearing IC Status flags...\r\n");
    
    if (ZSSC4151_Clear_Status () != 0)
    {
        dbg_put_string ("Warning: Failed to clear status flags.\r\n");
    }

    if (!cmd_mode_ok)
    {
        dbg_put_string ("Failed to Enter Command Mode!!\r\n");
        while (1); 
    }
    dbg_put_string ("Command Mode is OK!!\r\n");

    uint16_t nvm_buffer[128];
    for (int i = 0; i < 128; i++)
    {
        nvm_buffer[i] = harsh_emc_nvm_config[i];
    }

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

    // For NVRAM W/R testing
    nvm_buffer[0x2E] = 0x4444;
    nvm_buffer[0x2F] = 0x6666;

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
    }

    if (!write_ok)
    {
        dbg_put_string ("Fatal: NVRAM write process aborted!\r\n");
        while (1);
    }
    dbg_put_string ("OK: NVRAM write commands sent.\r\n");

    dbg_put_string ("Waiting for NVM programming to complete...\r\n");
    delay_10ms (5);     // waiting for stable

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

    if (verify_ok) dbg_put_string ("\r\nVerification PASS!!\r\n");
    else dbg_put_string ("\r\nVerification FAIL!!\r\n");

    dbg_put_string ("\r\n--- ZSSC4151 Normal Operation Mode ---\r\n");

    ZSSC4151_Check_Status ();

    // ???? ??? ???? 1: Normal Mode ???? ????? ???? ????
    if (ZSSC4151_Start_Normal_Mode () != 0)
    {
        dbg_put_string ("Fatal: Failed to start Normal Mode!\r\n");
        while (1);
    }
    dbg_put_string ("OK: Normal Mode Started. Acquiring data...\r\n\r\n");
    dbg_put_string ("\r\n    ---- ???? ??????, ZSSC4151 raw ?????? ??, SENT ?????? ??? ----\r\n\r\n");

    while (1)
    {
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
    T3CONbits.TON = 0; // ??? ??
    t3_done = 1; // ?? ??? set
}

/******************************************************************************
 * Function:        SENT1 Tx/Rx Interrupt
 *****************************************************************************/
void __attribute__ ((__interrupt__, __auto_psv__)) _SENT1Interrupt (void)
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
