/******************************************************************************/
/* Files to Include                                                           */
/******************************************************************************/

/* Device header file */
#if defined(__PIC24E__)
	#include <p24Exxxx.h>
#elif defined(__PIC24F__)
	#include <p24Fxxxx.h>
#elif defined(__PIC24H__)
	#include <p24Hxxxx.h>
#endif

#include "libpic30.h"
#include "uart.h"

//#include <stdint.h>        /* Includes uint16_t definition                    */
//#include <stdbool.h>       /* Includes true/false definition                  */

//#include "system.h"        /* System funct/params, like osc/peripheral config */
//#include "user.h"          /* User funct/params, such as InitApp              */

/*******************************
 * Configuration selections
 ******************************/
// FOSCSEL and FOSC default to FRC with divider and pin 10 = Fcpu
_FWDT(FWDTEN_OFF); 			// Turn off watchdog timer
_FICD(JTAGEN_OFF & ICS_PGD1);  // JTAG is disabled; ICD on PGC1, PGD1
_FOSC(IOL1WAY_OFF);

#define DATA_WORDS 8


/*************************************************
 *
 * Set up the variables and constants for PSOC SPI
 *
 *************************************************/
#define PSOC_TX_LENGTH 12               // Data Length
#define PSOC_SS LATBbits.LATB7          // Slave Select for the PSOC SPI
unsigned char MTR_PSOC_TX_BUF[PSOC_TX_LENGTH];   // PSOC TX data buffer
unsigned char MTR_PSOC_RX_BUF[PSOC_TX_LENGTH];
void MTR_PSOC_SPI_INIT(void);
void MTR_PSOC_SPI(void);
void Motorpsoc_Foreward(void);
void Motorpsoc_ClrBuf(void);
void Motorpsoc_Brake(void);
void Motorpsoc_Backward(void);
void Motorpsoc_Reverse(void);
void Motorpsoc_Left(void);
void Motorpsoc_Right(void);
void wirelessinit(void);



#define MA_DUTY   (MTR_PSOC_TX_BUF[0])
#define MB_DUTY   (MTR_PSOC_TX_BUF[1])
#define MA_CTL    (MTR_PSOC_TX_BUF[2])
#define MB_CTL    (MTR_PSOC_TX_BUF[3])

#define DIR_MASK            0x80//0b10000000
#define BRAKE_MASK          0x40//0b01000000
#define COAST_MASK          0x20//0b00100000
#define RESET_MASK          0x08//0b00001000
#define HALFBRIDGE_MASK     0x01//0b00000001


/**************************************************
 *
 * Set up a timer to control the SPI
 *
 *************************************************/
void timerinit(void);

/**************************************************
 *
 * UART
 *
 *************************************************/
char recievechar;
#define FCY 1000000
#define BAUD 9600   //4800 has minimized error with 1MHz clock
#define BRGVAL ((FCY/(16*BAUD))-1)




char min_data[DATA_WORDS];


void init();
void zero_dc();
void update_display();

int DELAY;
int SCRATCH;
int i;


#define Delay(x) DELAY = x+1; while(--DELAY){ Nop(); Nop(); }
#define XBLNK LATBbits.LATB15
#define GSLAT_M LATBbits.LATB6

//////// Main program //////////////////////////////////////////

int main()
{

    init();
    // XBLNK = 1;
    //timerinit();
    Motorpsoc_ClrBuf();
    wirelessinit();

    while(1)
    {

        /* check for receive errors */
        if(U1STAbits.FERR == 1)
        {
            continue;
        }
        /* must clear the overrun error to keep uart receiving */
        if(U1STAbits.OERR == 1)
        {
            U1STAbits.OERR = 0;
            continue;
        }
        /* get the data */
        if(U1STAbits.URXDA == 1)
        {
            recievechar = U1RXREG;
            switch(recievechar)
            {
                case 'a':       // LEFT
                    Motorpsoc_Right();
                    break;
                case 'w':       //FORWARD
                    Motorpsoc_Backward();
                    break;
                case 's':
                    Motorpsoc_Foreward();
                    break;
                case 'd':
                    Motorpsoc_Left();
                    break;
                case ' ':
                    Motorpsoc_Brake();
                    break;
            }
            MTR_PSOC_SPI();
        }
        //if(!U1STAbits.UTXBF){
        //    WriteUART1(0x55);         // Use to determine the clocking frequency
        //}

    }

}

/*******************************
 * Initialize oscillator and main clock
 * Set IO pins and output to zero
 * Initialize drivers' data to zero
 * Get initial RPG state
 * Initialize LED driver clock
 ******************************/
void init() {
	//__builtin_write_OSCCONH(0x07);

	//_IOLOCK =0;
	OSCCON = 0x0701;		// Use fast RCoscillator with Divide by 16
	CLKDIV = 0x0200;		// Divide by 4
	OSCTUN = 17;                    // Tune for Fcpu = 1 MHz

	// unlock peripheral pin mapping
        /*
	__builtin_write_OSCCONL(0x46);
	__builtin_write_OSCCONL(0x57);
	__builtin_write_OSCCONL(0x17);
        */
	TRISA = 0x0004;
	// Set up IO. 1 is input
	TRISB = 0x003F;
	AD1PCFGL = 0xFFFF;

	Nop();



	// Turn off drivers
	//LATB = 0x3040;
	// XBLNK = 0;
	GSLAT_M = 1;
        PSOC_SS = 1;


	Delay(200);

	// Connect a SPI peripheral to the DC pins
	// Clear and disable interrupts
	_SPI1IF = 0;
	_SPI1IE = 0;

	// SCK1 is 01000
	// SDO1 is 00111
	// Connect it to the output pins RP11 and RP10
	//RPOR5 = 0x0807;
        MTR_PSOC_SPI_INIT();

	// Turn it on in master mode
	SPI1CON1 = 0x0135;              // Preset scaler 1MHz/48
	SPI1STATbits.SPIEN = 1;

	// Just in case
	Delay(8000)

	// lock peripheral pin mapping
	//__builtin_write_OSCCONL(0x46);
	//__builtin_write_OSCCONL(0x57);
	//__builtin_write_OSCCONL(0x57);

}

void MTR_PSOC_SPI(){
    int i;
    //SPI1BUF = 0x55;
    //while(!SPI1STATbits.SPITBF);
    Delay(50);


    for(i = 0; i < PSOC_TX_LENGTH; i++)
    {
        PSOC_SS = 0;
        SPI1BUF = MTR_PSOC_TX_BUF[i];
        //if(i < (PSOC_TX_LENGTH - 1)){
        //    while(!SPI1STATbits.SPIRBF);
        //}
        MTR_PSOC_RX_BUF[i] = SPI1BUF;
            // Wait for the recieve data
        
        Delay(50);                      // Need this so data isn't overwritten
                                        // in the buffer, and for PSOC
    }
    //SPI1BUF = 0xFF;
    //Delay(50);
    Nop();
    PSOC_SS = 1;

    /*
     * Check to see if these statements are needed
     *  they shouldn't do anything because
     */
    SPI1BUF = 0x2A;
    Delay(50);
    SPI1BUF = 0x20;
    Delay(50);
    SPI1BUF = 0x20;
    Delay(50);
    SPI1BUF = 0x33;
    Delay(50);
    SPI1BUF = 0x00;
    Delay(50);
    SPI1BUF = 0xFF;
    Delay(50);
}

void MTR_PSOC_SPI_INIT(){

    __builtin_write_OSCCONL(0x46);
    __builtin_write_OSCCONL(0x57);
    __builtin_write_OSCCONL(0x17);        // Unlock Peripherals,
                                          // IOLOCK did not work
    /*  Consilidate these Peripheral Assignments    */

    RPOR4bits.RP8R = 7;          // Data out on Pin 8
    RPOR3bits.RP6R = 8;          // Serial Clock on Pin 6
    RPINR20bits.SDI1R = 9;      // Pin 9 Set as SPI

    RPOR5bits.RP11R = 3;        // TX line is RP11 pin
    RPINR18bits.U1RXR = 10;     // RX line is RP10 pin
                                // Slave Select set up on Pin 7
    //RPOR5 = 0x0807;


    __builtin_write_OSCCONL(0x46);
    __builtin_write_OSCCONL(0x57);
    __builtin_write_OSCCONL(0x57);          // Lock Peripherals



}

void Motorpsoc_Foreward() {


    if (MA_DUTY ==0 && MB_DUTY == 0) {  // if stopped
        MA_CTL = BRAKE_MASK | COAST_MASK;   // reverse motor direction and go forwards
        MB_CTL = BRAKE_MASK | COAST_MASK;
    }

    if (MA_CTL & DIR_MASK)  {    // if going backwards, decrease duty cycle
        MA_CTL = DIR_MASK | BRAKE_MASK | COAST_MASK;    // add DIR_MASK to reverse motors
        MB_CTL = DIR_MASK | BRAKE_MASK | COAST_MASK;

        if (MA_DUTY >= 80)    // a duty cycle between 0 and 80 doesn't move the robot
          MA_DUTY -= 10;
        else
          MA_DUTY = 0;

        if (MB_DUTY >= 80)
          MB_DUTY -= 10;
        else
          MB_DUTY = 0;

    } else {                     // if going forward, increase duty cycle

       MA_CTL = BRAKE_MASK | COAST_MASK;
       MB_CTL = BRAKE_MASK | COAST_MASK;

       if (MA_DUTY < 80)   // a duty cycle between 0 and 80 doesn't move the robot
        MA_DUTY = 80;
       else if (MA_DUTY < 192) // max duty cycle at 192
         MA_DUTY += 10;
       else
         MA_DUTY = 192;

       if (MB_DUTY < 80)
        MB_DUTY = 80;
       else if (MB_DUTY < 192)
         MB_DUTY += 10;
       else
         MB_DUTY = 192;

    }
}

void Motorpsoc_ClrBuf() {
  /*    Clear out the SPI TX buffer     */
  MA_DUTY = 0;
  MB_DUTY = 0;
  MA_CTL = 0;
  MB_CTL = 0;
  MTR_PSOC_TX_BUF[4] = 0xaa;
  MTR_PSOC_TX_BUF[5] = 0xaa;
  MTR_PSOC_TX_BUF[6] = 0xaa;
  MTR_PSOC_TX_BUF[7] = 0xaa;
  MTR_PSOC_TX_BUF[8] = 0xaa;
  MTR_PSOC_TX_BUF[9] = 0xaa;
  MTR_PSOC_TX_BUF[10] = 0xaa;
  MTR_PSOC_TX_BUF[11] = 0xaa;
  /*    Clear out the SPI Rx Buffer     */
  MTR_PSOC_RX_BUF[0] = 0;
  MTR_PSOC_RX_BUF[1] = 0;
  MTR_PSOC_RX_BUF[2] = 0;
  MTR_PSOC_RX_BUF[3] = 0;
  MTR_PSOC_RX_BUF[4] = 0;
  MTR_PSOC_RX_BUF[5] = 0;
  MTR_PSOC_RX_BUF[6] = 0;
  MTR_PSOC_RX_BUF[7] = 0;
  MTR_PSOC_RX_BUF[8] = 0;
  MTR_PSOC_RX_BUF[9] = 0;
  MTR_PSOC_RX_BUF[10] = 0;
  MTR_PSOC_RX_BUF[11] = 0;

}

void timerinit(){

    T1CONbits.TON = 0; // Disable Timer
    T1CONbits.TCS = 0; // Select internal instruction cycle clock
    T1CONbits.TGATE = 0; // Disable Gated Timer mode
    T1CONbits.TCKPS = 0b11; // Select 1:1 Prescaler
    TMR1 = 0x00;  // Clear timer register
    PR1 = 0x7FFF;  // Load the period value
    IPC0bits.T1IP = 0x01; // Set Timer1 Interrupt Priority Level
    IFS0bits.T1IF = 0; // Clear Timer1 Interrupt Flag
    IEC0bits.T1IE = 0; // Enable Timer1 interrupt
    T1CONbits.TON = 1; // Start Timer

}

void Motorpsoc_Brake() {
  MA_CTL &= ~BRAKE_MASK;
  MB_CTL &= ~BRAKE_MASK;
  MA_DUTY = 0;
  MB_DUTY = 0;

  MTR_PSOC_SPI();
}

void Motorpsoc_Backward() {

    if (MA_DUTY ==0 && MB_DUTY == 0) {  // if stopped
        MA_CTL = DIR_MASK | BRAKE_MASK | COAST_MASK;   // reverse motor direction and go backwards
        MB_CTL = DIR_MASK | BRAKE_MASK | COAST_MASK;
    }

    if (MA_CTL & DIR_MASK)  {    // if going backwards, increase duty cycle
        MA_CTL = DIR_MASK | BRAKE_MASK | COAST_MASK;
        MB_CTL = DIR_MASK | BRAKE_MASK | COAST_MASK;

       if (MA_DUTY < 90)   // a duty cycle between 0 and 80 doesn't move the robot
        MA_DUTY = 90;
       else if (MA_DUTY < 192) // max duty cycle at 192
         MA_DUTY += 10;
       else
         MA_DUTY = 192;

       if (MB_DUTY < 90)
        MB_DUTY = 90;
       else if (MB_DUTY < 192)
         MB_DUTY += 10;
       else
         MB_DUTY = 192;

    } else {                     // if going forward, decrease duty cycle
       MA_CTL = BRAKE_MASK | COAST_MASK;
       MB_CTL = BRAKE_MASK | COAST_MASK;

      if (MA_DUTY >= 90) // a duty cycle between 0 and 80 doesn't move the robot
        MA_DUTY -= 10;
      else
        MA_DUTY = 0;

      if (MB_DUTY >= 90)
        MB_DUTY -= 10;
      else
        MB_DUTY = 0;

    }

//  SPI_Motorpsoc();

}

void Motorpsoc_Reverse() {    // reverse direction

  if (MA_DUTY ==0 && MB_DUTY == 0) {       // must be stopped

    if (MA_CTL & DIR_MASK)         {
        MA_CTL = BRAKE_MASK | COAST_MASK;
        MB_CTL = BRAKE_MASK | COAST_MASK;
    } else {
        MA_CTL = DIR_MASK | BRAKE_MASK | COAST_MASK;
        MB_CTL = DIR_MASK | BRAKE_MASK | COAST_MASK;

    }
  }

}

void Motorpsoc_Left() {
    if (MA_CTL & DIR_MASK)  {
        MA_CTL = DIR_MASK | BRAKE_MASK | COAST_MASK;
        MB_CTL = DIR_MASK | BRAKE_MASK | COAST_MASK;
    } else {
       MA_CTL = BRAKE_MASK | COAST_MASK;
       MB_CTL = BRAKE_MASK | COAST_MASK;

    }
  if (MA_DUTY >= 10)// Was 32
    MA_DUTY -= 2;
  else
    MA_DUTY = 0;
  if (MB_DUTY < 224)
    MB_DUTY += 2;
  else
    MB_DUTY = 255;

  //MTR_PSOC_SPI();
}

void Motorpsoc_Right() {
    if (MA_CTL & DIR_MASK)  {
        MA_CTL = DIR_MASK | BRAKE_MASK | COAST_MASK;
        MB_CTL = DIR_MASK | BRAKE_MASK | COAST_MASK;
    } else {
       MA_CTL = BRAKE_MASK | COAST_MASK;
       MB_CTL = BRAKE_MASK | COAST_MASK;

    }
  if (MA_DUTY < 224)
    MA_DUTY += 2;
  else
    MA_DUTY = 255;
  if (MB_DUTY >= 10)
    MB_DUTY -= 2;
  else
    MB_DUTY = 0;

  //MTR_PSOC_SPI();
}

void wirelessinit(){
    // For Fcy 1MHz and Baud rate of 4800 UxBRG = 12
    
    U1MODEbits.UARTEN   = 0;    // Disable the unit
    U1MODEbits.PDSEL    = 0;    // No Parity, 8 data bits
    U1MODEbits.STSEL    = 0;    // One Stop bit
    U1MODEbits.ABAUD    = 0;    // Disables autobaud
    U1MODEbits.BRGH     = 1;    // Baud Rate determined for this setting
    U1BRG               = 12;//BRGVAL;    //for 9600, 12 for 4800

    U1STAbits.URXISEL   = 0;    // Generate and interrupt when one char recieved
    IEC0bits.U1RXIE     = 0;    // Disabled the RX character interrupt
    IEC0bits.U1TXIE     = 0;//disabled    // Enable UART TX interrupt
    U1MODEbits.UARTEN   = 1;    // Enable UART
    U1STAbits.UTXEN     = 1;    // Enable UART TX

}

void __attribute__((interrupt,auto_psv)) _U1RXInterrupt(void){

    recievechar = U1RXREG;
    IFS0bits.U1RXIF = 0;

}

void __attribute__((interrupt,auto_psv)) _U1TXInterrupt(void)
{
      IFS0bits.U1TXIF = 0; // clear TX interrupt flag
      U1TXREG = 'a'; // Transmit one character
}