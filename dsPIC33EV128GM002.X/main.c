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

//
int main (void)
{
    oscConfig ();

    Init_Hw ();
    UART1_Init ();
    I2C1_Init ();
#if     TEST_CLK
    REFCLKO_Init ();    
#endif    

#if         SENT_PULSE_ONLY
    SENT1_TX_Init ();
#endif    
    
#if        ! SENT_PULSE_ONLY
#if         NVRAM_N_UART        
    bool cmd_mode_ok = (ZSSC4151_EnterCommandMode () == 0);
    dbg_put_string ("Clearing IC Status flags...\r\n");
    
    if (ZSSC4151_Clear_Status () != 0) dbg_put_string ("Warning: Failed to clear status flags.\r\n");
    
    if (!cmd_mode_ok)
    {
        dbg_put_string ("Failed to Enter Command Mode!!\r\n");
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
    
    while(1);
#endif
#endif
    
    dbg_put_string("\r\nSystem Initialized.\r\n");

    // ZSSC4151을 Normal 모드로 전환 
    dbg_put_string("Starting ZSSC4151 in Normal Mode...\r\n");
    
    if (ZSSC4151_Start_Normal_Mode() != 0) 
    {
        dbg_put_string("Fatal: Failed to start Normal Mode.\r\n");
        while(1);
    }
    
    dbg_put_string("ZSSC4151 is running. Starting Main Loop...\r\n");
    delay_10ms(100); // 센서 안정화
  
    // 3. 메인 무한 루프
    while (1)
    {
        uint16_t bridge_result_16bit = 0;
        uint16_t bridge_15bit_val = 0;
        float temp = 0.0f, humi = 0.0f;
        float sensing_percent = 0.0f;

        // RAM 0x41에서 16비트 결과값을 직접 읽기
        int zssc_ok = (ZSSC4151_ReadRamBurst(ZSSC_RAM_ADDR_BRIDGE_RESULT, 1, (uint8_t*)&bridge_result_16bit) == 0);
        bool sht_ok = SHT4x_Read_TH(&temp, &humi);

        if (zssc_ok) 
        {
            bool is_error = (bridge_result_16bit & 0x8000); // MSB가 에러 플래그인지 확인
            bridge_15bit_val = bridge_result_16bit & 0x7FFF; // 15비트 데이터 추출

            if (is_error) dbg_put_string("Warning: ZSSC4151 Diagnostic Error Flag is set!\r\n");

            // 15비트 값을 0~4.0 % 범위로 변환
            float scale_factor = 0.0f;
            if ((SENSOR_15BIT_MAX - SENSOR_15BIT_MIN) > 0) scale_factor = (float)(bridge_15bit_val - SENSOR_15BIT_MIN) / (float)(SENSOR_15BIT_MAX - SENSOR_15BIT_MIN);

            if (scale_factor < 0.0f) scale_factor = 0.0f;
            if (scale_factor > 1.0f) scale_factor = 1.0f;
            sensing_percent = scale_factor * 4.0f;

        } 
        else  dbg_put_string("Error: Failed to read ZSSC4151 RAM.\r\n");
        
        // sht_ok에 대한 에러 처리
        if (!sht_ok) 
        {
            dbg_put_string("Error: Failed to get SHT4x Value.\r\n");
            // 에러 발생 시 온도/습도 값을 안전한 값(0)으로 설정
            temp = 0.0f;
            humi = 0.0f;
        }        
        
        // --- 데이터를 각 자리 숫자로 분해하여 6개의 니블 생성 ---
        uint8_t sense_digit1 = (uint8_t)sensing_percent;
        uint8_t sense_digit2 = (uint8_t)((sensing_percent - sense_digit1) * 10);
        uint8_t temp_int = (uint8_t)temp;
        uint8_t temp_digit1 = (temp_int / 10) % 10;
        uint8_t temp_digit2 = temp_int % 10;
        uint8_t humi_int = (uint8_t)humi;
        uint8_t humi_digit1 = (humi_int / 10) % 10;
        uint8_t humi_digit2 = humi_int % 10;
        
        // --- SENT 데이터 프레임 생성 ---
        uint8_t status_nibble = 0x1;
        SENT1DATH = ((uint16_t)status_nibble << 12) | ((uint16_t)sense_digit1 << 8) | ((uint16_t)sense_digit2 << 4) | ((uint16_t)temp_digit1);
        SENT1DATL = ((uint16_t)temp_digit2 << 12) | ((uint16_t)humi_digit1 << 8) | ((uint16_t)humi_digit2 << 4);

        SENT1STATbits.SYNCTXEN = 1;
        while(SENT1STATbits.SYNCTXEN == 1);
        
        // --- 디버그 메시지 출력 ---
        dbg_put_hex_word (count++);
        dbg_put_string (") Sense: ");
        dbg_put_float(sensing_percent);
        dbg_put_string(" % (15bit Raw: 0x");
        dbg_put_hex_word(bridge_15bit_val); // 15비트 Raw 값 출력 추가
        dbg_put_string("), Temp: ");
        dbg_put_float(temp);
        dbg_put_string(" C, Humi: ");
        dbg_put_float(humi);
        dbg_put_string(" % -> SENT Frame: 0x");
        dbg_put_hex_word(SENT1DATH);
        dbg_put_hex_word(SENT1DATL);
        dbg_put_string("\r\n");
        
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
    T3CONbits.TON = 0; // ??? ??
    t3_done = 1; // ?? ??? set
}

/******************************************************************************
 * Function:        SENT1 Tx/Rx Interrupt
 *****************************************************************************/
void __attribute__ ((__interrupt__, __auto_psv__)) _SENT1Interrupt (void)
{
    IFS11bits.SENT1IF = 0; // clear interrupt flag
}
