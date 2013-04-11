#include <msp430.h> 

/*
 * main.c
 */

//MSP430 NUMBER 1

static void config_clocks();
static void config_interrupts();
static void config_ports();

//interrupt handlers
__interrupt void Port_1(void);
__interrupt void Port_2(void);
__interrupt void Timer1_A1 (void);

//clock variables
unsigned int button, whatButton;
unsigned int almWatch;
unsigned int temp[3];

//Display items
unsigned int tube1,tube2;//1 is hour
unsigned int tubeSel, digit;

//clock fuctions
void handleButton();
void getTube();
void setTube();
void alarmOn();
void alarmOff();
void initVars();


int main(void) {
    IE1=NMIIE;                    //enable nmi
    WDTCTL=WDTPW+WDTHOLD+WDTNMI;  //select nmi function on RST/NMI

	config_interrupts();
	config_ports();
	initVars();

	while(1){


		handleButton();

		__bis_SR_register(LPM3_bits + GIE);//sleep
	}
}




static void config_clocks(){

	//for no crystal
	DCOCTL = CALDCO_1MHZ;//DCO frequency step set
	BCSCTL1 = CALBC1_1MHZ;//basic lock system control reg1 freq. range set


	BCSCTL2 = 0x06;//0b00000110SM clock is DO clock/8=125000Hz; main is dco

	BCSCTL3 = 0x20;//ACLK is VLOCLK @ 12kHz

	//if crystal
	//BCSCTL3 = 0b00000000;//Aclk is crystal, bits 3&2 determine capacitor select.
	//1,6,10,12.5pF


}

static void config_interrupts(){

	TA0CTL = TASSEL_2 + TACLR + MC_1 + ID_3; //SMCLK, clear TAR, count up, SM/8 = 15625Hz;
	CCTL0 = CCIE; // CCR0 interrupt enabled
	CCR0 = 0x3D08; //15625/(15624+1) = interrupt @ 1Hz

}

void initVars(){
	button, whatButton = 0;
	almWatch = 0;
	int i;
	for(i=0;i<3;i++){temp[i];}
	tube1,tube2 = 0;
	tubeSel, digit = 0;
}

static void config_ports(){
	//p1.0 Z IN
	//p1.1 alarm IN
	//p1.2 PWM alarm action OUT
	//p1.3-1.6  XIN
	//p1.7 snooze button IN

	//p2.0-2.3 Tube2 OUT
	//p2.4-2.6 tube 1 OUT
	//p2.7 alarm LED IN

	P1SEL |= 0x04;//0b00000100
	P1DIR = 0x7C;//0b01111100
	P1REN |= 0x83;//0b10000011
	P1OUT &= 0x83;//0b10000011
	P1IE |= 0x82;//0b10000010don't' want zin to trigger
	P1IES |= 0x82;//0b10000010don't' want zin to trigger
	P1IFG &= 0x00;


	//1.2 PWM
	TA1CTL = TASSEL_1 + MC_0;//aclk @12000Hz
	TA1CCR0 = 6;// PWM period, 2000Hz
	TA1CCR1 = 3;//4000Hz 50% duty
	TA1CCTL1 = OUTMOD_7;

	P2DIR = 0x7F;//p2.7 IN
	P2IE &= 0x80;//interrupt for alarm switch

	_BIS_SR(GIE);//enable interrupts could also use _EINT();
}

/*------------------------------------------------------------------------------
 *functional routines
------------------------------------------------------------------------------*/

void handleButton(){

	if (button){
		switch(whatButton){
		case 1://alarm
			alarmOn();
			break;
		case 2://snooze
			alarmOff();
			break;
		default:
			break;
		}
		button=0;//button handled
	}

}

void alarmOn(){
	TA1CTL |= MC_1;//count up
}

void alarmOff(){
	TA1CTL &= MC_0;//stop count
}

void checkAlarm(){
	if(almWatch){
		almWatch++;
		if(almWatch == 60){//
			alarmOff();
		}
	}
}

void getTube(){

	if((P1IN & 0x01) == 0x00){//if tube1
		tube1 = ((P1IN & 0x38)<<1);
		tube1 |= 0x8F;
		tubeSel = 1;
	}
	else if((P1IN & 0x01) == 0x01){//if tube2
		tube2 = ((P1IN & 0x78)>>3);
		tube2 |= 0xF0;
		tubeSel = 2;
	}

}

void setTube(){

	unsigned int temp1, temp2;
	temp1,temp2 = 0xFF;
	temp1 &= P2OUT;//store port


	if(tubeSel==1){
			temp2 &= tube1;
			temp1 |= 0x70;
	}
	else if(tubeSel ==2){
			temp2 &= tube2;
			temp1 |= 0x0F;
	}

	temp1 = (temp1 & temp2);//determine new output
	P2OUT = temp1;
	tubeSel=0;
}


/*------------------------------------------------------------------------------
 *interrupt service routines
------------------------------------------------------------------------------*/
#pragma vector = TIMER1_A1_VECTOR
__interrupt void Timer1_A1 (void)
{

	checkAlarm();//each second check if it's on
	_bic_SR_register_on_exit(LPM3_bits); //clear flag

}

#pragma vector=PORT1_VECTOR
__interrupt void Port_1(void)
 {

	//check if snooze or alarm
	P1IFG &= 0x00; //clear interrupt flag
	int i;

	for(i=0;i<50000;i++){}//debounce for 50ms?

	if((P1IN & 0x82) == 0x80){//p1.1 alm
		button=1;
		whatButton=1;
	}
	else if((P1IN & 0x82) == 0x02){//p1.7 snooze
		button=1;
		whatButton=2;
	}
	_bic_SR_register_on_exit(LPM3_bits);//clear flag

 }

#pragma vector=PORT2_VECTOR
__interrupt void Port_2(void)
 {

	P2IFG &= 0x00; //clear interrupt flag



	_bic_SR_register_on_exit(LPM3_bits);
 }

#pragma vector=NMI_VECTOR
__interrupt void nmi(void)
{

	IFG1&=~NMIIFG;                      //clear nmi interrupt flag
	int i;
	IE1=NMIIE;                          // enable nmi
	WDTCTL = WDTPW + WDTHOLD+WDTNMI;    // select nmi function on RST/NMI

	//debounce slightly

	for(i=0;i<100;i++);

	getTube();
	setTube();

}



