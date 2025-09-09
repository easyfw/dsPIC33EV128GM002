/*
 * File:   common.c
 * Author: easyf
 *
 * Created on 2025�� 8�� 31�� (��), ���� 4:32
 */

#include <stdbool.h>
#include <stdint.h> 

#include <p33EV128GM002.h>
#include "..\h\common.h"

//
extern volatile uint32_t t1_tick, t2_tick;
extern volatile uint8_t t3_done;

extern uint16_t tickTime;               // Tick time in us
extern float peripheralClk;             // in Mhz

//
void oscConfig (void)
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

//
void Init_Hw ()
{
    RCONbits.SWDTEN = 0;    // 
       
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

/**
 * Initializes and enables the Reference Clock Output on pin RB4.
 */
void REFCLKO_Init (void)
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

/**
 * UART1 ??? ????? ??
 */
void UART1_Init (void)
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
void SENT1_TX_Init (void)
{
    // 1. RP43 ��(RB11)�� ������� ����
    TRISBbits.TRISB11 = 0;

    // 2. PPS �������� ��� ����
    __builtin_write_OSCCONL(OSCCON & 0xBF);

    // 3. SENT1 ����� RP43 ������ ���� (�����ͽ�Ʈ TABLE 11-3 & TABLE 4-14 ���)
    //    SENT1 Function Code: 0b111001
    RPOR4bits.RP43R = 0b111001; 

    // 4. PPS �������� �� ���
    __builtin_write_OSCCONL(OSCCON | 0x40);
    
    // --- SENT ��� ���� (��Ȱ��ȭ ���¿��� ����) ---
    SENT1CON1bits.SNTEN = 0;

        // Initialize SENT registers for transmit mode (no frame time specified due to no pause)
    SENT1CON2 = (int) (tickTime * peripheralClk) - 1;
    SENT1CON1bits.RCVEN = 0; // sync handshaking mode
    SENT1CON1bits.TXM = 1; // sync handshaking mode
    SENT1CON1bits.CRCEN = 1; // CRC enable, 0=off, 1=on
    SENT1CON1bits.TXPOL = 0; 
    SENT1CON1bits.PPP = 0; // Pause, 0=off, 1=on
    SENT1CON1bits.NIBCNT = 6; // nibbles of data
    SENT1CON1bits.SNTEN = 1; // enable SENT module
    
    // ���ͷ�Ʈ�� ������� �����Ƿ� ��Ȱ��ȭ
    IEC11bits.SENT1IE = 0;
    IFS11bits.SENT1IF = 0;

    // --- ��� ���� �Ϸ� �� SENT ��� Ȱ��ȭ ---
    SENT1CON1bits.SNTEN = 1;
    
}

//
void delay_10ms (uint16_t num)
{
    t2_tick = 0;
    while (t2_tick < num)
    {
        __asm__ volatile ("nop");
    }
}

//
void delay_us (unsigned int us)
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

//
void dbg_putc (char c)
{
    while (U1STAbits.UTXBF); // ?? ??? ?? ? ?? ??
    U1TXREG = c;
}

/**
 * @brief 1?????(8???) ???? 2??? 16???? ??????? ???????. (??: 0xFE)
 * @param val ????? 8??? ??
 */
void dbg_put_hex_byte (uint8_t val)
{
    const char hex_digits[] = "0123456789ABCDEF";
    char buffer[3]; // "FF" + NULL ????('\0')

    // ???? 4??? (? ??�� ???)
    buffer[0] = hex_digits[(val >> 4) & 0x0F];

    // ???? 4??? (?? ??�� ???)
    buffer[1] = hex_digits[val & 0x0F];

    // ????? ????
    buffer[2] = '\0';

    dbg_put_string (buffer);
}

/**
 * @brief 2?????(16???) ???? 4??? 16???? ??????? ???????. (??: 0xBEEF)
 * @param val ????? 16??? ??
 */
void dbg_put_hex_word (uint16_t val)
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

/******************************************************************************
 * Function:        dbg_put_decimal_byte
 * Description:     Prints an 8-bit unsigned integer (0-255) in decimal format.
 * Input:           value - The uint8_t value to print.
 *****************************************************************************/
void dbg_put_dec_byte(uint8_t value)
{
    char buffer[4]; // 8��Ʈ �ִ밪 255�� 3�ڸ� + �� ����
    int i = 0;

    if (value == 0)
    {
        dbg_putc('0');
        return;
    }

    // ���ڸ� ���ڿ��� ��ȯ (�������� ���ۿ� �����)
    while (value > 0)
    {
        buffer[i++] = (value % 10) + '0';
        value /= 10;
    }
    buffer[i] = '\0'; // �� ���ڷ� ������

    // ���۸� �������� ����Ͽ� �ùٸ� ������ ǥ��
    while (i > 0)
    {
        i--;
        dbg_putc(buffer[i]);
    }
}

/******************************************************************************
 * Function:        dbg_put_decimal_word
 * Description:     Prints a 16-bit unsigned integer (0-65535) in decimal format.
 * Input:           value - The uint16_t value to print.
 *****************************************************************************/
void dbg_put_dec_word(uint16_t value)
{
    char buffer[6]; // 16��Ʈ �ִ밪 65535�� 5�ڸ� + �� ����
    int i = 0;

    if (value == 0)
    {
        dbg_putc('0');
        return;
    }

    // ���ڸ� ���ڿ��� ��ȯ (�������� ���ۿ� �����)
    while (value > 0)
    {
        buffer[i++] = (value % 10) + '0';
        value /= 10;
    }
    buffer[i] = '\0'; // �� ���ڷ� ������

    // ���۸� �������� ����Ͽ� �ùٸ� ������ ǥ��
    while (i > 0)
    {
        i--;
        dbg_putc(buffer[i]);
    }
}

//
void dbg_put_string (char *s)
{
    while (*s)
    { // loop until *s =\0, end of string
        dbg_putc (*s++);
    } // send the character and point to the next one
}

// UART1 float ???
void dbg_put_float (float temp)
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


//
int I2C1_SendByte (unsigned char data)
{
    I2C1TRN = data; // ?????? ??????? ???? ????????? ?��?
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
uint8_t I2C1_ReadByte (bool ack)
{
    I2C1CON1bits.RCEN = 1;
    while (I2C1CON1bits.RCEN);

    I2C1CON1bits.ACKDT = ack ? 0 : 1;
    I2C1CON1bits.ACKEN = 1;

    while (I2C1CON1bits.ACKEN);

    return I2C1RCV;
}

//
void I2C_Scan (void)
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

// --- I2C Driver Functions ---
// ### ????? ?��?: I2C ???? ???? ??? ###
void I2C1_BusReset (void)
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
void I2C1_Init (void)
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
void I2C1_WaitForIdle (void)
{
    while (I2C1STATbits.TRSTAT || (I2C1CONL & 0x1F));
}

//
bool I2C1_Start (void)
{
    I2C1_WaitForIdle ();
    I2C1CONLbits.SEN = 1;
    while (I2C1CONLbits.SEN);

    return true;
}

//
bool I2C1_Stop (void)
{
    I2C1_WaitForIdle ();
    I2C1CONLbits.PEN = 1;
    while (I2C1CONLbits.PEN);
    return true;
}

//
bool I2C1_Restart (void)
{
    I2C1_WaitForIdle ();
    I2C1CONLbits.RSEN = 1;
    while (I2C1CONLbits.RSEN);
    return true;
}

//
//
bool I2C1_Write (uint8_t data)
{
    I2C1_WaitForIdle ();
    I2C1TRN = data;

    while (I2C1STATbits.TBF); // TBF(Transmit Buffer Full) ?��??? ???
    I2C1_WaitForIdle ();

    if (I2C1STATbits.ACKSTAT) return false;
    return true;
}

//
uint8_t I2C1_Read (bool send_ack)
{
    I2C1_WaitForIdle ();
    I2C1CONLbits.RCEN = 1; // ???? ????
    while (!I2C1STATbits.RBF); // RBF(Receive Buffer Full) ?��??? ???

    uint8_t data = I2C1RCV;

    I2C1_WaitForIdle ();
    I2C1CONLbits.ACKDT = send_ack ? 0 : 1; // ACK ??? NACK ????
    I2C1CONLbits.ACKEN = 1;
    while (I2C1CONLbits.ACKEN);

    return data;
}

//
void I2C1_Ack (void)
{
    I2C1_WaitForIdle ();

    I2C1CONLbits.ACKDT = 0; // ACK
    I2C1CONLbits.ACKEN = 1;

    while (I2C1CONLbits.ACKEN);
}

//
void I2C1_Nack (void)
{
    I2C1_WaitForIdle ();

    I2C1CONLbits.ACKDT = 1; // NACK
    I2C1CONLbits.ACKEN = 1;

    while (I2C1CONLbits.ACKEN);
}

/******************************************************************************
 * Function:        Calculate_SENT_CRC
 * Description:     Calculates the 4-bit CRC for a SENT message frame 
 * according to SAE J2716 standard (Legacy, CRC-4/2.1).
 * Uses a lookup table for efficiency.
 * Input:           data_nibbles[] - An array of 6 uint8_t values (data nibbles).
 * Output:          4-bit CRC value (uint8_t).
 *****************************************************************************/
uint8_t Calculate_SENT_CRC(uint8_t data_nibbles[]) 
{
    // SAE J2716 CRC Lookup Table
    const uint8_t crc_lookup[16] = {0, 13, 7, 10, 14, 3, 9, 4, 1, 12, 6, 11, 15, 2, 8, 5};
    
    uint8_t crc_seed = 5; // Initial CRC seed value
    
    // The CRC calculation includes the 6 data nibbles
    crc_seed = crc_lookup[crc_seed ^ data_nibbles[0]];
    crc_seed = crc_lookup[crc_seed ^ data_nibbles[1]];
    crc_seed = crc_lookup[crc_seed ^ data_nibbles[2]];
    crc_seed = crc_lookup[crc_seed ^ data_nibbles[3]];
    crc_seed = crc_lookup[crc_seed ^ data_nibbles[4]];
    crc_seed = crc_lookup[crc_seed ^ data_nibbles[5]];
    
    return crc_seed;
}