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
#if         !NVRAM_RW_TEST          
//    SENT1_TX_Init ();
#endif    

#if         NVRAM_RW_TEST        
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
    
    dbg_put_string("\r\nSystem Initialized. Starting Main Loop...\r\n");
    
    // 2. 메인 무한 루프
    while (1)
    {
        int16_t raw = 0;        
        float temp = 0.0f, humi = 0.0f;
        float sensing_percent = 0.0f;

        // 센서 값 읽기
        int zssc_ok = Get_ZSSC4151_BridgeRaw(&raw);
        bool sht_ok = SHT4x_Read_TH(&temp, &humi);

        if (zssc_ok == 0) 
        {
            // --- 고객사 사양에 따른 Raw 값 -> 퍼센트 변환 로직 ---
            if (raw <= 10000) sensing_percent = 0.0f;
            else if (raw <= 15000)  sensing_percent = 2.0f * (float)(raw - 10000) / (float)(15000 - 10000);                // 구간 1: 10000 (0%) ~ 15000 (2%)
            else if (raw <= 20000)  sensing_percent = 2.0f + 1.0f * (float)(raw - 15000) / (float)(20000 - 15000);   // 구간 2: 15000 (2%) ~ 20000 (3%)
            else if (raw <= 40000)  sensing_percent = 3.0f + 1.0f * (float)(raw - 20000) / (float)(40000 - 20000);   // 구간 3: 20000 (3%) ~ 40000 (4%)
            else sensing_percent = 4.0f;      // bridge_raw_val > 40000
        }
        else dbg_put_string("Error: Failed to get ZSSC4151 Raw Value.\r\n");

        if (!sht_ok) dbg_put_string("Error: Failed to get SHT4x Value.\r\n");
        
        // --- 데이터를 각 자리 숫자로 분해하여 6개의 니블 생성 ---        
        // 니블 1, 2: 센싱값 (예: 2.4 -> 2, 4)
        uint8_t sense_digit1 = (uint8_t)sensing_percent; // 정수부
        uint8_t sense_digit2 = (uint8_t)((sensing_percent - sense_digit1) * 10); // 소수부 첫째자리

        // 니블 3, 4: 온도 (예: 34 -> 3, 4)
        uint8_t temp_int = (uint8_t)temp;
        uint8_t temp_digit1 = temp_int / 10; // 십의 자리
        uint8_t temp_digit2 = temp_int % 10; // 일의 자리

        // 니블 5, 6: 습도 (예: 45 -> 4, 5)
        uint8_t humi_int = (uint8_t)humi;
        uint8_t humi_digit1 = humi_int / 10; // 십의 자리
        uint8_t humi_digit2 = humi_int % 10; // 일의 자리
                
        // --- CRC 계산을 위해 데이터 니블 배열 생성 ---
        uint8_t data_nibbles[6];
        data_nibbles[0] = sense_digit1;
        data_nibbles[1] = sense_digit2;
        data_nibbles[2] = temp_digit1;
        data_nibbles[3] = temp_digit2;
        data_nibbles[4] = humi_digit1;
        data_nibbles[5] = humi_digit2;
        
    // --- CRC 계산 함수 호출 ---
        uint8_t crc_nibble = Calculate_SENT_CRC(data_nibbles);        
        
        // --- SENT 데이터 프레임 생성 ---
        uint8_t status_nibble = 0x1; // 상태: 정상

        SENT1DATH = ((uint16_t)status_nibble << 12) |
                  ((uint16_t)sense_digit1 << 8)    |
                  ((uint16_t)sense_digit2 << 4)    |
                  ((uint16_t)temp_digit1);
                  
        // SENT1DATL의 데이터 1~6, CRC 추가
        SENT1DATL = ((uint16_t)temp_digit2 << 12)  |
                ((uint16_t)humi_digit1 << 8)    |
                ((uint16_t)humi_digit2 << 4)    |
                ((uint16_t)crc_nibble); 

        // SENT 프레임 전송 시작
        SENT1STATbits.SYNCTXEN = 1;
        while(SENT1STATbits.SYNCTXEN == 1);        
        
        // --- 디버그 메시지 출력 ---
        dbg_put_dec_word (count++);
        dbg_put_string (") Raw: ");
        dbg_put_dec_word(raw);        
        dbg_put_string (", ");        
        dbg_put_float(sensing_percent);
        dbg_put_string(" %, Temp: ");
        dbg_put_float(temp);
        dbg_put_string(" C, Humi: ");
        dbg_put_float(humi);
        dbg_put_string(" % -> SENT Frame: 0x");
        dbg_put_hex_word(SENT1DATH);
        dbg_put_hex_word(SENT1DATL);
        dbg_put_string(" | CRC: 0x");
        dbg_put_hex_byte(crc_nibble);
        dbg_put_string("\r\n");        

        // 1초 대기 후 다음 측정 시작
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
