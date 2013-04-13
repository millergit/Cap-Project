#include <msp430.h> 

/*
 * main.c
 */

//MSP430 NUMBER 1

static void config_clocks();
static void config_interrupts();
static void config_ports();

//interrupt handlers
__interrupt void nmi(void);

//Display items
unsigned int tube1,tube2;//1 is hour
unsigned int tubeSel, digit;

//clock fuctions
void getTube();
void setTube();
void alarmOn();
void alarmOff();
void initVars();


int main(void) {
    IE1=NMIIE;                    //enable nmi
    WDTCTL=WDTPW+WDTHOLD+WDTNMI+WDTNMIES;  //select nmi function on RST/NMI

	config_interrupts();
	config_ports();
	initVars();

	while(1){

		__bis_SR_register(LPM3_bits + GIE);//sleep
	}
}




static void config_clocks(){

	//for no crystal
	DCOCTL = CALDCO_1MHZ;//DCO frequency step set
	BCSCTL1 = CALBC1_1MHZ;//basic lock system control reg1 freq. range set

	BCSCTL2 = 0x06;//0b00000110 SM clock is DO clock/8=125000Hz; main is dco
	BCSCTL3 = 0x20;//ACLK is VLOCLK @ 12kHz

}

static void config_interrupts(){
	

}

void initVars(){
	tube1,tube2 = 0;
	tubeSel, digit = 0;
}

static void config_ports(){

	//p1.0-p1.3 tube 5 OUT
	//p1.4-p1.7 tube 6 OUT

	//p2.0-p2.3 X IN
	//p2.4 Z IN
	//p2.5 mode IN
	//p2.6,p2.7 2 to 4 LEDs OUT

	P1DIR = 0xFF;//all out
	P1OUT &= 0x00;

	P2DIR = 0xC0;//p2.7 IN
	P2REN |= 0x20;//enable resistor
	P2OUT |= 0x40; //p2.7 is pullup resistor
	P2IE &= 0x00;//P2 interrupts off

	_BIS_SR(GIE);//enable interrupts could also use _EINT();
}

/*------------------------------------------------------------------------------
 *functional routines
------------------------------------------------------------------------------*/

void getTube(){


}

void setTube(){

}


/*------------------------------------------------------------------------------
 *interrupt service routines
------------------------------------------------------------------------------*/

#pragma vector=NMI_VECTOR
__interrupt void nmi(void)
{

	IFG1&=~NMIIFG;                      //clear nmi interrupt flag
	int i;
	IE1 |= NMIIE;                          // enable nmi
	WDTCTL = WDTPW + WDTHOLD + WDTNMI + WDTNMIES;    // select nmi function on RST/NMI (hi/lo)

	//debounce slightly

	for(i=0;i<100;i++);

	getTube();
	setTube();

}


