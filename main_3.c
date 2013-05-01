#include <msp430.h> 

/*
 * main.c
 */

//MSP430 NUMBER 3

static void config_clocks();
static void config_interrupts();
static void config_ports();

//interrupt handlers
__interrupt void nmi(void);
__interrupt void Port_2(void);

//Display items
unsigned char tube5,tube6, twoFourCnt;//1 is hour
unsigned int tubeSel, digit;
unsigned char twoFourTable[4] = {0x9F,0xBF,0xDF,0xFF};

//clock functions
void getTube();
void setTube();
void initVars();


int main(void) {
	IE1=NMIIE;                    //enable nmi
	WDTCTL=WDTPW+WDTHOLD+WDTNMI+WDTNMIES;  //select nmi function on RST/NMI

	config_clocks();
	config_interrupts();
	config_ports();
	initVars();

	while(1){

		__bis_SR_register(LPM1_bits + GIE);//sleep
	}
}




static void config_clocks(){

	//for no crystal
	DCOCTL = CALDCO_1MHZ;//DCO frequency step set
	BCSCTL1 = CALBC1_1MHZ;//basic lock system control reg1 freq. range set
	BCSCTL2 = 0x06;//SM clock is DO clock/8=125000Hz; main is dco
	BCSCTL3 |= LFXT1S_2; // LFXT1 = VLO

}

static void config_interrupts(){


}

void initVars(){
	tube5 = 0;
	tube6 = 0;
	tubeSel = 0;
	digit = 0;
	twoFourCnt = 0;
}

static void config_ports(){

	//p1.0-p1.3 tube 5 OUT
	//p1.4-p1.7 tube 6 OUT

	//p2.0-p2.3 X IN
	//p2.4 Z IN
	//p2.5,p2.6
	//2 to 4 LEDs OUT
	//p2.7 mode IN

	P1DIR = 0xFF;//all out
	P1OUT = 0x00;

	P2SEL = 0x00;
	P2DIR = 0x60;
	P2REN = 0x9F;
	P2OUT = 0x9F;//pullup resistors & start at mode 00
	P2IES = 0x80;
	P2IE = 0x80;//mode interrupt


	_BIS_SR(GIE);//enable interrupts could also use _EINT();
}

/*------------------------------------------------------------------------------
 *functional routines
------------------------------------------------------------------------------*/

void getTube(){

	if((P2IN & 0x10) == 0x00){//tube 5
		tubeSel=5;
		tube5 = (P2IN & 0x0F);
		tube5 |= 0xF0;
	}
	else if((P2IN & 0x10) == 0x10){//tube 6
		tubeSel=6;
		tube6 = ((P2IN & 0x0F)<<4);// shift to match other port
		tube6 |= 0x0F;
	}


}

void setTube(){

	unsigned int temp1, temp2;
	temp1 = 0xFF;
	temp2 = 0xFF;
	temp1 &= P1OUT;//store port


	if(tubeSel==5){
		temp2 &= tube5;
		temp1 |= 0x0F;
	}
	else if(tubeSel==6){
		temp2 &= tube6;
		temp1 |= 0xF0;
	}

	temp1 = (temp1 & temp2);//determine new output before changing P2
	P1OUT = temp1;
	tubeSel=0;
}



/*------------------------------------------------------------------------------
 *interrupt service routines
------------------------------------------------------------------------------*/

#pragma vector=PORT2_VECTOR
__interrupt void Port_2(void)
{

	unsigned char replace;
	unsigned int i;
	for(i=0;i<10000;i++);
	replace = P2OUT;

	if((P2IN & 0x80) == 0x00){// p2.7 mode
		twoFourCnt++;
		twoFourCnt = twoFourCnt%4;
		replace |= 0x60;
		replace &= twoFourTable[twoFourCnt];
		}

	P2OUT = replace; //alter 2.5 2.6 for LEDs

	P2IFG = 0x00;//clear interrupt flag

}

#pragma vector=NMI_VECTOR
__interrupt void nmi(void)
{

	IFG1&=~NMIIFG;                      //clear nmi interrupt flag
	IE1 |= NMIIE;                          // enable nmi
	WDTCTL = WDTPW + WDTHOLD + WDTNMI + WDTNMIES;    // select nmi function on RST/NMI (hi/lo)

	//debounce slightly
	unsigned int i;
	for(i=0;i<100;i++);

	getTube();
	setTube();

	P2IES = 0x80;
	P2IE = 0x80;//mode interrupt

	_BIS_SR(GIE);//enable interrupts could also use _EINT();
}



