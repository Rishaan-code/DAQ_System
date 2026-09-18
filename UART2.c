/* UART2.c
 * Rishaan Malik and Abhinav Chodisetti 
 * Data: 4/7/2026
 * PA22 UART2 Rx from other microcontroller PA8 UART1 Tx<br>
 */

#include <ti/devices/msp/msp.h>
#include "UART2.h"
#include "../inc/Clock.h"
#include "../inc/LaunchPad.h"
#include "../inc/FIFO1.h"
#define PA8INDEX  18 // UART1_TX  SPI0_CS0  UART0_RTS TIMA0_C0  TIMA1_C0N
#define PA9INDEX  19 // UART1_RX  SPI0_PICO UART0_CTS TIMA0_C1  RTC_OUT   TIMA0_C0N TIMA1_C1N CLK_OUT
#define PA22INDEX 46 // UART2_RX  TIMG8_C1  UART1_RTS TIMA0_C1  CLK_OUT   TIMA0_C0N TIMG6_C1

uint32_t LostData;

// power Domain PD0
// for 80MHz bus clock, UART clock is ULPCLK 40MHz
// initialize UART2 for 2000 baud rate
// no transmit, interrupt on receive timeout
void UART2_Init(void){
    // do not reset or activate PortA, already done in LaunchPad_Init
    // RSTCLR to UART2 peripherals
    UART2->GPRCM.RSTCTL = 0xB1000003; // reset UART2
    UART2->GPRCM.PWREN = 0x26000001;  // activate UART2
    Clock_Delay(24);                   // time for uart to activate

    // configure PA22 as UART2 Rx, input enable on (bit 18)
    IOMUX->SECCFG.PINCM[PA22INDEX] = 0x00040082;

    UART2->CLKSEL = 0x08;     // bus clock (40 MHz)
    UART2->CLKDIV = 0x00;     // no divide

    UART2->CTL0 &= ~0x01;     // disable UART2 before configuring

    UART2->CTL0 = 0x00020018; // enable fifos, rx mode

    // baud rate: 40,000,000/16 = 2,500,000; 2,500,000/2000 = 1250
    UART2->IBRD = 1250;       // integer baud rate divisor
    UART2->FBRD = 0;          // no fractional part needed

    UART2->LCRH = 0x00000030; // 8 data bits, 1 stop bit, no parity

    Fifo1_Init();             // initialize software FIFO for received data

    // arm receiver timeout interrupt only (bit 0 = RTIM)
    // no TX interrupt (bit 11) and no RX FIFO interrupt (bit 10)
    UART2->CPU_INT.IMASK = 0x0001;

    // IFLS: bits 11-8 RXTOSEL=4, bits 6-4 RXIFLSEL=2, bits 2-0 TXIFLSEL=2
    // RXTOSEL=4 means timeout triggered after 4 bits of idle
    UART2->IFLS = 0x0422;

    NVIC->ICPR[0] = 1<<14;   // clear any pending UART2 interrupt (IRQ 14)
    NVIC->ISER[0] = 1<<14;   // enable UART2 interrupt in NVIC

    // set priority bits 23-22 in NVIC->IP[3] for interrupt 14
    NVIC->IP[3] = (NVIC->IP[3]&(~0x00FF0000))|(2<<22);

    UART2->CTL0 |= 0x01;     // enable UART2
}
//------------UART2_InChar------------
// Get new serial port receive data from FIFO1
// Input: none
// Output: Return 0 if the FIFO1 is empty
//         Return nonzero data from the FIFO1 if available
char UART2_InChar(void){
  return Fifo1_Get(); // write this
}


void UART2_IRQHandler(void){
    uint32_t status;
    // reading IIDX acknowledges the interrupt
    status = UART2->CPU_INT.IIDX;

    if(status == 0x01){ // 0x01 = receiver timeout RTOUT
        GPIOB->DOUTTGL31_0 = BLUE; // first toggle
        GPIOB->DOUTTGL31_0 = BLUE; // second toggle

        // drain hardware FIFO into software FIFO
        // STAT bit 2 is RXFE, when 0 means data is available
        while((UART2->STAT & 0x04) == 0){
            Fifo1_Put((char)UART2->RXDATA);
        }

        GPIOB->DOUTTGL31_0 = BLUE; // third toggle
    }
}
