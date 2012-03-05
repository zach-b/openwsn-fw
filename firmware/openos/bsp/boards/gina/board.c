/**
\brief GINA-specific definition of the "board" bsp module.

\author Thomas Watteyne <watteyne@eecs.berkeley.edu>, February 2012.
*/

#include "msp430x26x.h"
#include "board.h"
#include "debugpins.h"
// bsp modules
#include "leds.h"
#include "uart.h"
#include "spi.h"
#include "i2c.h"
#include "timers.h"
#include "radio.h"
#include "radiotimer.h"

//=========================== variables =======================================

//=========================== prototypes ======================================

//=========================== public ==========================================

void board_init() {
   // disable watchdog timer
   WDTCTL  = WDTPW + WDTHOLD;
   
   // setup clock speed
   BCSCTL1 = CALBC1_16MHZ;                       // MCLK at ~16MHz
   DCOCTL  = CALDCO_16MHZ;                       // MCLK at ~16MHz
   
   // initialize pins
   //-- radio RF_SLP_TR_CNTL (P4.7)
   P4OUT  &= ~0x80;                              // set low
   P4DIR  |=  0x80;                              // configure as output
   //-- radio IRQ_RF (P1.6)
   P1OUT  &= ~0x40;                              // set low
   P1DIR  &= ~0x40;                              // configure as low
   P1IES  &= ~0x40;                              // interrup when transition is low-to-high
   P1IE   |=  0x40;                              // enable interrupt
   // debug pins
   DEBUG_PIN_FRAME_INIT();
   DEBUG_PIN_SLOT_INIT();
   DEBUG_PIN_FSM_INIT();
   DEBUG_PIN_TASK_INIT();
   DEBUG_PIN_ISR_INIT();
   DEBUG_PIN_RADIO_INIT();
   
   // initialize bsp modules
   leds_init();
   uart_init();
   spi_init();
   timers_init();
   radio_init();
   radiotimer_init();
   
   // enable interrupts
   __bis_SR_register(GIE);
}

void board_sleep() {
   __bis_SR_register(GIE+LPM0_bits);             // sleep, but leave ACLK on
}

//=========================== private =========================================

//=========================== interrupt handlers ==============================

// DAC12_VECTOR

// DMA_VECTOR

#pragma vector = USCIAB1TX_VECTOR
__interrupt void USCIAB1TX_ISR(void) {
   CAPTURE_TIME();
   DEBUG_PIN_ISR_SET();
   if ( ((UC1IFG & UCB1TXIFG) && (UC1IE & UCB1TXIE)) ||
        ((UC1IFG & UCB1RXIFG) && (UC1IE & UCB1RXIE)) ) {
      isr_i2c_tx(1);                             // I2C: TX
   }
   if ( (UC1IFG & UCA1TXIFG) && (UC1IE & UCA1TXIE) ){
      if (uart_isr_tx()==1) {                    // UART: TX
         __bic_SR_register_on_exit(CPUOFF);
      }
   }
   DEBUG_PIN_ISR_CLR();
}

#pragma vector = USCIAB1RX_VECTOR
__interrupt void USCIAB1RX_ISR(void) {
   CAPTURE_TIME();
   DEBUG_PIN_ISR_SET();
   if ( ((UC1IFG & UCB1RXIFG) && (UC1IE & UCB1RXIE)) ||
         (UCB1STAT & UCNACKIFG) ) {
      isr_i2c_rx(1);                             // I2C: RX, bus 1
   }
   if ( (UC1IFG & UCA1RXIFG) && (UC1IE & UCA1RXIE) ){
      if (uart_isr_rx()==1) {                    // UART: RX
         __bic_SR_register_on_exit(CPUOFF);
      }
   }
   DEBUG_PIN_ISR_CLR();
}

#pragma vector = PORT1_VECTOR
__interrupt void PORT1_ISR (void) {
   CAPTURE_TIME();
   DEBUG_PIN_ISR_SET();
   if (P1IFG & 0x40) {
      P1IFG &= ~0x40;
      if (radio_isr()==1) {                      // radio:  SFD pin [P4.6]
         __bic_SR_register_on_exit(CPUOFF);
      }
   } else {
      while (1); // should never happen
   }
   DEBUG_PIN_ISR_CLR();
}

#pragma vector = PORT2_VECTOR
__interrupt void PORT2_ISR (void) {
   CAPTURE_TIME();
   DEBUG_PIN_ISR_SET();
#ifdef ISR_BUTTON
   if ((P2IFG & 0x80)!=0) {                      // button: [P2.7]
      P2IFG &= ~0x80;
      scheduler_push_task(ID_ISR_BUTTON);
      __bic_SR_register_on_exit(CPUOFF);
   }
#endif
   DEBUG_PIN_ISR_CLR();
}

#pragma vector = ADC12_VECTOR
__interrupt void ADC12_ISR (void) {
   CAPTURE_TIME();
   DEBUG_PIN_ISR_SET();
   ADC12IFG &= ~0x1F;
   __bic_SR_register_on_exit(CPUOFF);
   DEBUG_PIN_ISR_CLR();
}

// USCIAB0TX_VECTOR

#pragma vector = USCIAB0RX_VECTOR
__interrupt void USCIAB0RX_ISR (void) {
   CAPTURE_TIME();
   DEBUG_PIN_ISR_SET();
   if ( (IFG2 & UCA0RXIFG) && (IE2 & UCA0RXIE) ) {
      if (spi_isr()==1) {                        // SPI
         __bic_SR_register_on_exit(CPUOFF);
      }
   }
   if ( ((IFG2 & UCB0RXIFG) && (IE2 & UCB0RXIE)) ||
        (UCB0STAT & UCNACKIFG) ) {
      isr_i2c_rx(0);                             // I2C: RX, bus 0
   }
   DEBUG_PIN_ISR_CLR();
}

#pragma vector = TIMERA1_VECTOR
__interrupt void TIMERA1_ISR (void) {
   CAPTURE_TIME();
   DEBUG_PIN_ISR_SET();
   if (radiotimer_isr()==1) {                    // radiotimer
      __bic_SR_register_on_exit(CPUOFF);
   }
   DEBUG_PIN_ISR_CLR();
}

// TIMERA0_VECTOR

// WDT_VECTOR

#pragma vector = COMPARATORA_VECTOR
__interrupt void COMPARATORA_ISR (void) {
   CAPTURE_TIME();
   DEBUG_PIN_ISR_SET();
   __bic_SR_register_on_exit(CPUOFF);            // restart CPU
   DEBUG_PIN_ISR_CLR();
}

#pragma vector = TIMERB1_VECTOR
__interrupt void TIMERB1_ISR (void) {
   CAPTURE_TIME();
   DEBUG_PIN_ISR_SET();
   if (timer_isr_1()==1) {                       // timer: 1
      __bic_SR_register_on_exit(CPUOFF);
   }
   DEBUG_PIN_ISR_CLR();
}

#pragma vector = TIMERB0_VECTOR
__interrupt void TIMERB0_ISR (void) {
   CAPTURE_TIME();
   DEBUG_PIN_ISR_SET();
   if (timer_isr_0()==1) {                       // timer: 0
      __bic_SR_register_on_exit(CPUOFF);
   }
   DEBUG_PIN_ISR_CLR();
}

// NMI_VECTOR