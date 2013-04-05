#include <msp430.h> 

/*
 * main.c
 */

//MSP430 NUMBER 1

static void config_clocks();
static void config_interrupts();
static void config_ports();

//interrupt handlers
__interrupt void Port_2(void);
__interrupt void Timer1_A3 (void);

//clock variables
unsigned int button, whatButton;
unsigned int almPower, almWatch;
unsigned int temp[3];

//Display items
unsigned int tube1,tube2;//1 is hour
unsigned int tubeSel, digit;

//clock fuctions
void getTube();
void checkAlarm();


int main(void) {
    IE1=NMIIE;                    //enable nmi
    WDTCTL=WDTPW+WDTHOLD+WDTNMI;  //select nmi function on RST/NMI

	config_interrupts();
	config_ports();
	initVars();

	while(1){


		checkAlarm();

		__bis_SR_register(LPM3_bits + GIE);//sleep
	}
}


static void config_clocks(){

	//for no crystal
	DCOCTL = CALDCO_1MHZ;//DCO frequency step set
	BCSCTL1 = CALBC1_1MHZ;//basic lock system control reg1 freq. range set


	BCSCTL2 = 0b00000110;//SM clock is DO clock/8=125000Hz; main is dco

	BCSCTL3 = 0b00100000;//ACLK is VLOCLK @ 12kHz

	//if crystal
	//BCSCTL3 = 0b00000000;//Aclk is crystal, bits 3&2 determine capacitor select.
	//1,6,10,12.5pF


}

static void config_interrupts(){
	//no crystal, use more accurate DO clock
	TA0CTL = TASSEL_2 + TACLR + MC_1 + ID_3; //SMCLK, clear TAR, count up, SM/8 = 15625Hz;
	CCTL0 = CCIE; // CCR0 interrupt enabled
	CCR0 = 0x3D08; //15625/(15624+1) = interrupt @ 1Hz

	//crystal
	//CCR0 = 0x7FFF; //32768/(32767) = interrupt @ 1Hz


}

void initVars(){
	almPower = 0;
	almWatch = 0;
	tubeSel = 6;
}

static void config_ports(){
	//p1.0 Z IN
	//p1.1 alarm IN
	//p1.2 PWM alarm action OUT
	//p1.3-1.6  tube 2 OUT
	//p1.7 snooze button IN

	//p2.0-2.3 X IN
	//p2.4-2.6 tube 1 OUT
	//p2.7 alarm LED OUT

	P1SEL |= 0b00000100;
	P1DIR = 0b01111100;
	P1REN |= 0b10000011;
	P1OUT &= 0b10000011;
	P1IES |= 0b10000011;
	P1IFG &= 0b01111100;
	P1IE |= 0b10000011;

	//1.2 PWM
	TA1CTL = TASSEL_1 + MC_1;//aclk @12000Hz
	TA1CCR0 = 6;// PWM period, 2000Hz
	TA1CCR1 = 3;//4000Hz 50% duty
	TA1CCTL1 = OUTMOD_7;

	P2DIR = 0xF0;//p2.7 output
	P2IE &= 0xF0;//interrupt disable

	_BIS_SR(GIE);//enable interrupts could also use _EINT();
}

/*------------------------------------------------------------------------------
 *functional routines
------------------------------------------------------------------------------*/
void checkAlarm(){

	if(almWatch){
		almWatch++;
		if(almWatch == 10000){//10,000/1MHz =.01s*12000(other MSP)=120 cycles to see alarm
			almWatch = 0;
			P2OUT &= ~0x80;//clear alarm
		}
	}
	else if((almH==hour)&&(almM==min)&&(pm=almPm)){
		P2OUT |= 0x80;//send alarm beacon on p1.7
		almWatch=1;
	}

}

void getTube(){

}


/*------------------------------------------------------------------------------
 *interrupt service routines
------------------------------------------------------------------------------*/
#pragma vector=TIMER1_A3_VECTOR
__interrupt void Timer1_A3 (void)
{


	checkAlarm();
	_bic_SR_register_on_exit(LPM3_bits); //clear flag


}

#pragma vector=PORT1_VECTOR
__interrupt void Port_1(void)
 {

	//check snooze and alarm

	P1IFG &= 0x00; //clear interrupt flag

 }

#pragma vector=PORT2_VECTOR
__interrupt void Port_2(void)
 {

	//check snooze and alarm

	P2IFG &= 0x00; //clear interrupt flag

 }

#pragma vector=NMI_VECTOR
__interrupt void nmi(void)
{
	 IFG1&=~NMIIFG;                      //clear nmi interrupt flag
	 IE1=NMIIE;                          // enable nmi
	 WDTCTL = WDTPW + WDTHOLD+WDTNMI;    // select nmi function on RST/NMI

	//debounce slightly?
	for(i=0;i<5000;i++);
	//read appropriate pins
	getTube();

}



