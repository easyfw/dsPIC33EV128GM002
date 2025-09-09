/*
 * File:   main.c
 * Author: easyf
 *
 * Created on 2025/8/13
 * * --- ���� �� �м� ���� ---
 * 1. [������ ���� �ذ�] NVM ����/���� �Ϸ� �� ���� ����(while(1))�� ���� 
 * ���� ���� ���� �д� ���� ������ �������� ���ϴ� ������ �����߽��ϴ�.
 * 2. [���� ����] Ĩ�� ���� �ΰ� �� �ڵ����� Normal Mode�� �����ϹǷ�, ���ʿ��ϰ� 
 * Normal Mode ���� ����� �ٽ� ������ �߻��ϴ� ������ �����߽��ϴ�.
 * 3. [���� ����] ���� �䱸���׿� ����, Normal Mode �߿��� I2C ����� ������
 * 'RdOutMemBurst (0x2E)' ����� ����Ͽ� RAM 0x41������ ���� ���� ����
 * ���� �о������ ������ �����߽��ϴ�.
 * 4. [NVM ���α׷��� Ȱ��ȭ] I2C �б� ���� ������ �ٺ� ������ '�����α׷��ֵ� NVM'
 * ������ �ذ��ϱ� ����, ���� �� NVM ���α׷��� ������ �׻� ����ǵ��� �����߽��ϴ�.
 * 5. [�����] Normal Mode ��ȯ �� I2C ����� �����Ǵ� ������ Ȯ���ϱ� ����
 * 'ZSSC4151_Start_Normal_Mode()' ȣ���� �ּ� ó���Ͽ� Command Mode�� �ӹ������� �����߽��ϴ�.
 * 6. [����� �ڵ� �ݿ�] ����ڲ��� �������ֽ� I2C RAM �б� �Լ� �ڵ�� ��ü�߽��ϴ�.
 */
#include <stdio.h>

#include <p33EV128GM002.h>
#include "..\h\main.h"
#include "..\h\common.h"
#include "..\h\zSSC4151.h"
#include "..\h\sHT4x.h"

//
int main (void)
{
    oscConfig ();

    Init_Hw ();
    UART1_Init ();
    I2C1_Init ();
#if      TEST_CLK
    REFCLKO_Init ();    
#endif    

#if          SENT_PULSE_ONLY
    SENT1_TX_Init ();
#endif    
    
#if          ! SENT_PULSE_ONLY
#if         AFTER_1_TIME_WRITING_NVRAM    
    {
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
        for (int i = 0; i < 128; i++) nvm_buffer[i] = harsh_emc_nvm_config[i];

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
//        nvm_buffer[0x2E] = 0x4444;
//        nvm_buffer[0x2F] = 0x6666;

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
        else 
        {
            dbg_put_string ("\r\nVerification FAIL!!\r\n");
            while(1);
        }

       dbg_put_string("NVM Programming complete. Staying in Command Mode for debugging.\r\n");
    }
#endif    
#endif
    
    dbg_put_string("\r\nSystem Initialized.\r\n");

    dbg_put_string("ZSSC4151 is running. Starting Main Loop...\r\n");
    delay_10ms(100); // ���� ����ȭ
  
    // 3. ���� ���� ����
    while (1)
    {
        uint8_t i2c_rx_buffer[2];
        uint16_t bridge_15bit_val = 0;
        float temp = 0.0f, humi = 0.0f;
        float sensing_percent = 0.0f;
        
        int zssc_ok = ZSSC4151_Read_Ram_In_Normal_Mode(ZSSC_RAM_ADDR_BRIDGE_RESULT, 1, i2c_rx_buffer);
        bool sht_ok = SHT4x_Read_TH(&temp, &humi);

        if (zssc_ok == 0) 
        {
            uint16_t bridge_result_16bit = ((uint16_t)i2c_rx_buffer[0] << 8) | i2c_rx_buffer[1];   
            
            bool is_error = (bridge_result_16bit & 0x8000); 
            bridge_15bit_val = bridge_result_16bit & 0x7FFF;

            if (is_error) dbg_put_string("Warning: ZSSC4151 Diagnostic Error Flag is set!\r\n");
            
            int32_t val = bridge_15bit_val;
            float scale_factor = 0.0f;
            
            if ((SENSOR_15BIT_MAX - SENSOR_15BIT_MIN) > 0) 
            {
                 scale_factor = (float)(val - SENSOR_15BIT_MIN) / (float)(SENSOR_15BIT_MAX - SENSOR_15BIT_MIN);
            }           
        
            if (scale_factor < 0.0f) scale_factor = 0.0f;
            if (scale_factor > 1.0f) scale_factor = 1.0f;
            sensing_percent = scale_factor * 4.0f;
        } 
        else  dbg_put_string("Error: Failed to read ZSSC4151 RAM.\r\n");
        
        if (!sht_ok) 
        {
            dbg_put_string("Error: Failed to get SHT4x Value.\r\n");
            temp = 0.0f;
            humi = 0.0f;
        }       
        
        // --- �����͸� �� �ڸ� ���ڷ� �����Ͽ� 6���� �Ϻ� ���� ---
        uint8_t sense_digit1 = (uint8_t)sensing_percent;
        uint8_t sense_digit2 = (uint8_t)((sensing_percent - sense_digit1) * 10);
        uint8_t temp_int = (uint8_t)temp;
        uint8_t temp_digit1 = (temp_int / 10) % 10;
        uint8_t temp_digit2 = temp_int % 10;
        uint8_t humi_int = (uint8_t)humi;
        uint8_t humi_digit1 = (humi_int / 10) % 10;
        uint8_t humi_digit2 = humi_int % 10;
        
        // --- SENT ������ ������ ���� ---
        uint8_t status_nibble = 0x1;
        SENT1DATH = ((uint16_t)status_nibble << 12) | ((uint16_t)sense_digit1 << 8) | ((uint16_t)sense_digit2 << 4) | ((uint16_t)temp_digit1);
        SENT1DATL = ((uint16_t)temp_digit2 << 12) | ((uint16_t)humi_digit1 << 8) | ((uint16_t)humi_digit2 << 4);

        SENT1STATbits.SYNCTXEN = 1;
        while(SENT1STATbits.SYNCTXEN == 1);
        
        // --- ����� �޽��� ��� ---
        static uint16_t count = 0; 
        dbg_put_hex_word (count++);
        dbg_put_string (") Sense: ");
        dbg_put_float(sensing_percent);
        dbg_put_string(" % (15bit Raw: 0x");
        dbg_put_hex_word(bridge_15bit_val);
        dbg_put_string("), Temp: ");
        dbg_put_float(temp);
        dbg_put_string(" C, Humi: ");
        dbg_put_float(humi);
        dbg_put_string(" % -> SENT Frame: 0x");
        dbg_put_hex_word(SENT1DATH);
        dbg_put_hex_word(SENT1DATL);
        dbg_put_string("\r\n");
        
        __asm__ volatile ("clrwdt");        
        
        delay_10ms(100);
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

