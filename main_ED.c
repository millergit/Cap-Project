#include "bsp.h"
#include "mrfi.h"
#include "nwk_types.h"
#include "nwk_api.h"
#include "bsp_leds.h"
#include "bsp_buttons.h"
#include "vlo_rand.h"


//EZ430 in remote

static void linkTo(void);

__interrupt void ADC10_ISR(void);
__interrupt void Timer_A (void);

static linkID_t sLinkID1 = 0;
/* Temperature offset set at production */
volatile int * tempOffset = (int *)0x10F4;

/* Work loop semaphores */
static volatile uint8_t sSelfMeasureSem = 0;


void main (void)
{

  BSP_Init();

  BCSCTL3 |= LFXT1S_2;                      // LFXT1 = VLO
  TACCTL0 = CCIE;                           // TACCR0 interrupt enabled
  TACCR0 = 12000;                           // ~ 1 sec
  TACTL = TASSEL_1 + MC_1;                  // ACLK, upmode

  /* Keep trying to join (a side effect of successful initialization) until
   * successful. Toggle LEDS to indicate that joining has not occurred.
   */

  linkTo();

  while(1);
}

static void linkTo()
{
  uint8_t msg[3];

  /* Keep trying to link... */
  while (SMPL_SUCCESS != SMPL_Link(&sLinkID1))
  {
    BSP_TOGGLE_LED1();
    BSP_TOGGLE_LED2();
    /* Go to sleep (LPM3 with interrupts enabled)
     * Timer A0 interrupt will wake CPU up every second to retry linking
     */
    __bis_SR_register(LPM3_bits+GIE);
  }

  /* Turn off LEDs. */
  BSP_TURN_OFF_LED1();
  BSP_TURN_OFF_LED2();

  /* Put the radio to sleep */
  SMPL_Ioctl(IOCTL_OBJ_RADIO, IOCTL_ACT_RADIO_SLEEP, 0);

  while (1)
  {
    /* Go to sleep, waiting for interrupt every second to acquire data */
    __bis_SR_register(LPM3_bits);

    /* Time to measure */
    if (sSelfMeasureSem) {
      volatile long temp;
      int degC, volt;
      int results[2];

      /* Get temperature */
      ADC10CTL1 = INCH_10 + ADC10DIV_4;       // Temp Sensor ADC10CLK/5
      ADC10CTL0 = SREF_1 + ADC10SHT_3 + REFON + ADC10ON + ADC10IE + ADC10SR;
      /* Allow ref voltage to settle for at least 30us (30us * 8MHz = 240 cycles)
       * See SLAS504D for settling time spec
       */
      __delay_cycles(240);
      ADC10CTL0 |= ENC + ADC10SC;             // Sampling and conversion start
      __bis_SR_register(CPUOFF + GIE);        // LPM0 with interrupts enabled
      results[0] = ADC10MEM;                  // Retrieve result
      ADC10CTL0 &= ~ENC;

      /* Get voltage */
      ADC10CTL1 = INCH_11;                     // AVcc/2
      ADC10CTL0 = SREF_1 + ADC10SHT_2 + REFON + ADC10ON + ADC10IE + REF2_5V;
      __delay_cycles(240);
      ADC10CTL0 |= ENC + ADC10SC;             // Sampling and conversion start
      __bis_SR_register(CPUOFF + GIE);        // LPM0 with interrupts enabled
      results[1] = ADC10MEM;                  // Retrieve result

      /* Stop and turn off ADC */
      ADC10CTL0 &= ~ENC;
      ADC10CTL0 &= ~(REFON + ADC10ON);

      /* oC = ((A10/1024)*1500mV)-986mV)*1/3.55mV = A10*423/1024 - 278
       * the temperature is transmitted as an integer where 32.1 = 321
       * hence 4230 instead of 423
       */
      temp = results[0];
      degC = ((temp - 673) * 4230) / 1024;
      if( (*tempOffset) != 0xFFFF )
      {
        degC += (*tempOffset);
      }

      /* message format,  UB = upper Byte, LB = lower Byte
      -------------------------------
      |degC LB | degC UB |  volt LB |
      -------------------------------
         0         1          2
      */
      temp = results[1];
      volt = (temp*25)/512;
      msg[0] = degC&0xFF;
      msg[1] = (degC>>8)&0xFF;
      msg[2] = volt;

      /* Get radio ready...awakens in idle state */
      SMPL_Ioctl( IOCTL_OBJ_RADIO, IOCTL_ACT_RADIO_AWAKE, 0);


      /* No AP acknowledgement, just send a single message to the AP */
      SMPL_Send(SMPL_LINKID_USER_UUD, msg, sizeof(msg));

      /* Put radio back to sleep */
      SMPL_Ioctl( IOCTL_OBJ_RADIO, IOCTL_ACT_RADIO_SLEEP, 0);

      /* Done with measurement, disable measure flag */
      sSelfMeasureSem = 0;
    }
  }
}
/*------------------------------------------------------------------------------
 * ADC10 interrupt service routine
 *----------------------------------------------------------------------------*/
#pragma vector=ADC10_VECTOR
__interrupt void ADC10_ISR(void)
{
  __bic_SR_register_on_exit(CPUOFF);        // Clear CPUOFF bit from 0(SR)
}

/*------------------------------------------------------------------------------
 * Timer A0 interrupt service routine
 *----------------------------------------------------------------------------*/
#pragma vector=TIMERA0_VECTOR
__interrupt void Timer_A (void)
{
  sSelfMeasureSem = 1;
  __bic_SR_register_on_exit(LPM3_bits);        // Clear LPM3 bit from 0(SR)
}
