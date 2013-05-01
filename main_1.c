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
__interrupt void nmi(void);
__interrupt void Timer1_A1(void);

//clock variables
unsigned int whatButton,almTrigger,snoozeCount;
unsigned int almWatch;

//Display items
unsigned char tube1,tube2;//1 is hour
unsigned int tubeSel, digit;

//clock functions
void handleButton(int whatButton);
void getTube();
void setTube();
void alarmOn();
void alarmOff();
void initVars();


int main(void) {
	IE1=NMIIE;                    //enable nmi
	WDTCTL=WDTPW+WDTHOLD+WDTNMI+WDTNMIES;  //select nmi function on RST/NMI

	config_clocks();
	config_interrupts();
	config_ports();
	initVars();

	while(1){


		__bis_SR_register(LPM0_bits + GIE);//sleep
	}
}




static void config_clocks(){

	//for no crystal
	DCOCTL = CALDCO_1MHZ;//DCO frequency step set
	BCSCTL1 = CALBC1_1MHZ;//basic lock system control reg1 freq. range set

	BCSCTL2 = 0x06;//0b00000110 SM clock is DO clock/8=125000Hz; main is dco
	BCSCTL3 = 0x20;//ACLK is VLOCLK @ 12kHz

	//if crystal
	//BCSCTL3 = 0b00000000;//Aclk is crystal, bits 3&2 determine capacitor select.
	//1,6,10,12.5pF


}

static void config_interrupts(){

	TA1CTL = TASSEL_1 + TACLR + MC_1; //ACLK, clear TAR, count up
	TA1CCTL0 = CCIE; // CCR0 interrupt enabled
	TA1CCR0 = 11999; //12000/(11999+1) = interrupt @ 1Hz

}

void initVars(){
	whatButton=0;
	almWatch=0;
	tube1=0;
	tube2=0;
	tubeSel=0;
	digit=0;
}

static void config_ports(){
	//p1.0 Z IN
	//p1.1 alarm IN
	//p1.2 alarm switch IN
	//p1.3-1.6  XIN
	//p1.7 snooze button IN

	//p2.0-2.3 Tube2 OUT
	//p2.4,2.5 tube 1 OUT partial
	//p2.6 PWM alarm action OUT
	//p2.7 tube 1 OUT partial

	P1DIR = 0x00;// all input
	P1REN = 0x86;//0b1000 0110 // enable resistors for snooze and alarm switch
	P1OUT = 0x86;//0b1000 0110 //pull up type resistor
	P1IES = 0x86;//0b1000 0110 //alm in and snooze on hi/lo transition
	P1IE = 0x86;//0b1000 0110 //alm in, snooze, alm switch
	
	P1IFG &= 0x00;//clear flag
	
	P2SEL = 0x40;//select PWM out
	P2DIR = 0xFF;//all output
	P2OUT = 0x00; //p2.7 is pullup resistor

	//2.6 PWM
	TA0CTL = 0x0100;// 0b0000 0001 0000 0000;//aclk @12000Hz
	CCR0 = 6;// PWM period, 2000Hz
	CCR1 = 3;//4000Hz 50% duty
	CCTL1 = OUTMOD_7;


	_BIS_SR(GIE);//enable interrupts could also use _EINT();
}

/*------------------------------------------------------------------------------
 *functional routines
------------------------------------------------------------------------------*/

void handleButton(int whatButton){

	switch(whatButton){
	case 1://alarm
		if((P1IN & 0x04)== 0x04){//alarm switch high = on
			alarmOn();
			almTrigger = 1;
		}
		break;
	case 2://alarm switch
		almTrigger = 0;
		alarmOff();
		break;
	case 3://snooze
		alarmOff();
		break;
	default:
		break;
	}

}



void alarmOn(){

	TA0CTL = 0x0110;//count up
	almWatch=1;
		
}

void alarmOff(){

	TA0CTL = 0x0100;//stop count
	
	if(almTrigger){
		snoozeCount=1;
	}
	
}

void checkAlarm(){
	if(almWatch){
		almWatch++;
		if(almWatch == 600){//10 min auto off
			alarmOff();
			almWatch = 0;
		}
	}
	if(snoozeCount){//snooze button hit - alarm not reset
		snoozeCount++;
		if(snoozeCount == 600){//10m snooze
			alarmOn();
		}
	}
}

void getTube(){
	unsigned char tempx = 0xFF;

	if((P1IN & 0x01) == 0x00){//if tube1
		tubeSel = 1;
		tube1 = ((P1IN & 0x38)<<1);//bit shift to match port 2 tube 1 out
		tube1 |= 0x80;
		tempx = ((tube1 & 0x40)<<1);
		tempx |= 0x7F;
		tube1 &= tempx;
		tube1 |= 0x4F;
	}
	else if((P1IN & 0x01) == 0x01){//if tube2
		tubeSel = 2;
		tube2 = ((P1IN & 0x78)>>3);//bit shift to match port 2 tube 2 out
		tube2 |= 0xF0;

	}

}

void setTube(){

	unsigned char temp1, temp2;
	temp1 = 0xFF;
	temp2 = 0xFF;
	temp1 &= P2OUT;//store port


	if(tubeSel==1){
		temp2 &= tube1;
		temp1 |= 0xB0;
	}
	else if(tubeSel==2){
		temp2 &= tube2;
		temp1 |= 0x0F;
	}

	temp1 = (temp1 & temp2);//determine new output before changing P2
	P2OUT = temp1;
	tubeSel=0;
}


/*------------------------------------------------------------------------------
 *interrupt service routines
------------------------------------------------------------------------------*/
#pragma vector = TIMER1_A0_VECTOR
__interrupt void Timer1_A0 (void)
{

	checkAlarm();//each second check if it's on

}

#pragma vector=PORT1_VECTOR
__interrupt void Port_1(void)
{

	//check if snooze or alarm
	unsigned int i;
	for(i=0;i<10000;i++);

	if((P1IN & 0x02) == 0x00){//p1.1 alm
		handleButton(1);
	}
	if((P1IN & 0x04) == 0x00){//p1.2 alm switch		
		//ONLY TRIGGER FOR EDGE THAT REPRESENTS ALARM OFF
		handleButton(2);
	}
	if((P1IN & 0x80) == 0x00){//p1.7 snooze
		handleButton(3);
	}

	P1IFG &= 0x00; //clear interrupt flag

}

#pragma vector=NMI_VECTOR
__interrupt void nmi(void)
{
	IFG1&=~NMIIFG;                      //clear nmi interrupt flag
	IE1 |= NMIIE;                          // enable nmi
	WDTCTL = WDTPW + WDTHOLD + WDTNMI + WDTNMIES;    // select nmi function on RST/NMI (hi/lo)
	P1IE = 0x86;//0b1000 0110 //alm in, snooze, alm switch

	//debounce slightly
	unsigned int i;
	for(i=0;i<100;i++);

	getTube();
	setTube();

}



