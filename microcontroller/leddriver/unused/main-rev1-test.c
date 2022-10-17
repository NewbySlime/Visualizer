#include <stm8s.h>
#include <string.h>
#include <stdio.h>

#define true 1
#define false 0

#define BAUDRATE 9600
#define LED_LENGTH 1

/*    TIM2 params   */
// fCPU / (fCPU / 1.25*10^-6) - 1
#define LED_TIM2_ARR 31249

// data low (0)
// Duty Cycle of 35%
#define LED_LOWCK 10937
// data high (1)
// Duty Cycle of 65%
#define LED_HIGHCK 20313
uint16_t led_deltack = LED_HIGHCK - LED_LOWCK;

// Duty Cycle of 100%
#define LED_TIM2_HIGH 31250
// Duty Cycle of 0%
#define LED_TIM2_LOW 0


/*    TIM3 params   */
// fCPU / (fCPU / 10^-5) - 1  // or in other words, interval of 10us
#define TIM3_ARR 160




#define SETLEDBIT(n) sent = false;\
next_data = n & 0xff;\
next_data1 = (n >> 8) & 0xff;\
while(!sent);


void deinit_gpio(GPIO_TypeDef *gpio){
  gpio->CR1 = GPIO_CR1_RESET_VALUE;
  gpio->CR2 = GPIO_CR2_RESET_VALUE;
  gpio->DDR = GPIO_DDR_RESET_VALUE;
}

void set_uart_baudrate(uint8_t *reg1, uint8_t *reg2, uint32_t rate){
  uint32_t div = F_CPU/rate;
  div = div > 0xFFFF? 0xFFFF: div;
  *reg1 = (div >> 4) & 0xFF;
  *reg2 = (div >> 12) | (div & 0x0F);
}

// note: will not clear register
void wait_sreg(uint8_t *reg, uint8_t value){
  while((*reg & value) != value)
    ;
}

// str is null-terminated string
void uart1_writestr(const char *str){
  int idx = 0;
  while(str[idx]){
    UART1->DR = str[idx];
    wait_sreg(&UART1->SR, UART1_SR_TC);
    idx++;
  }
}

uint8_t uart1_waitforbyte(){
  wait_sreg(&UART1->SR, UART1_SR_RXNE);
  return UART1->DR;
}

int uart1_readstr(char *str, int length){
  for(int i = 0; i < length; i++){
    str[i] = uart1_waitforbyte();
    if(str[i] == '\n')
      return i;
  }

  return 0;
}

// parameters for interrupt
volatile bool up = true;

// FIXME try use another timer
void tim2_wait(uint32_t tickovf){
  up = false;
  while(tickovf > 0){
    while(!up)
      ;
    up = false;
    tickovf--;
  }
}


// parameters for interrupt
volatile bool sent = false;
volatile uint8_t next_data = 0;
volatile uint8_t next_data1 = 0;

// cols length has to be LED_LENGTH
// and each element has to be around 0x00000000 - 0x00ffffff
void led_sendcol(uint32_t *cols){
  int idx = 0;
  uint16_t data[24];
  while(idx < LED_LENGTH){
    uint32_t col = cols[idx];

    // setting up the data
    for(int i = 0; i < 24; i++){
      data[i] = ((col >> i) & 1) * led_deltack + LED_LOWCK;

      char strtest[16];
      sprintf(strtest, "%d\n", data[i]);
      uart1_writestr(strtest);
    }

    SETLEDBIT(data[0]);
    SETLEDBIT(data[1]);
    SETLEDBIT(data[2]);
    SETLEDBIT(data[3]);
    SETLEDBIT(data[4]);
    SETLEDBIT(data[5]);
    SETLEDBIT(data[6]);
    SETLEDBIT(data[7]);
    SETLEDBIT(data[8]);
    SETLEDBIT(data[9]);
    SETLEDBIT(data[10]);
    SETLEDBIT(data[11]);
    SETLEDBIT(data[12]);
    SETLEDBIT(data[13]);
    SETLEDBIT(data[14]);
    SETLEDBIT(data[15]);
    SETLEDBIT(data[16]);
    SETLEDBIT(data[17]);
    SETLEDBIT(data[18]);
    SETLEDBIT(data[19]);
    SETLEDBIT(data[20]);
    SETLEDBIT(data[21]);
    SETLEDBIT(data[22]);
    SETLEDBIT(data[23]);
    SETLEDBIT(LED_TIM2_LOW);

    idx++;
  }
}


// TODO list
//  - first, try gpio by just switching the registers (done)
//  - second, try tim2, by just pwm, then switching by polling, then switching by interrupt (done)
//  - third, try uart, full read and write (done)
//  - pulsing led on and off (just write 1 until last led, just checking if the timing is right)
//  - then sending actual color data to led
//  - if sending to led with no problem, try for uart with interrupt
int main(){
  disableInterrupts();

  // de-initiating
  deinit_gpio(GPIOD);

  CLK->HSITRIMR = 0;
  CLK->CKDIVR = 0;

  // setting up pin
  GPIOD->DDR |= 1 << 4; // PD4 is output mode
  GPIOD->CR1 |= 1 << 4; // push pull
  GPIOD->CR2 |= 0 << 4; // not fast mode
  /*
  GPIOD->ODR |= 1 << 4; // high

  GPIOD->DDR |= 1 << 3; // PD3 is output mode
  GPIOD->CR1 |= 1 << 3; // push pull

  GPIOD->DDR |= 1 << 2; // PD2 is output mode
  GPIOD->CR1 |= 1 << 2;
  */

  // setting up tim2
  TIM2->IER = TIM2_IER_UIE;
  TIM2->EGR = TIM2_EGR_UG;
  TIM2->CCMR1 = TIM2_OCMODE_PWM2;
  TIM2->CCER1 = TIM2_CCER1_CC1P | TIM2_CCER1_CC1E;
  TIM2->PSCR = TIM2_PRESCALER_512;
  //TIM2->PSCR = TIM2_PRESCALER_1;
  TIM2->ARRH = LED_TIM2_ARR >> 8;
  TIM2->ARRL = LED_TIM2_ARR & 0xFF;
  TIM2->CCR1H = LED_TIM2_LOW >> 8;
  TIM2->CCR1L = LED_TIM2_LOW & 0xFF;
  TIM2->CR1 = TIM2_CR1_CEN;

  
  set_uart_baudrate(&UART1->BRR1, &UART1->BRR2, BAUDRATE);
  UART1->CR1 = 0;
  UART1->CR2 = UART1_CR2_TEN | UART1_CR2_REN;
  UART1->CR3 = 0;
  UART1->CR4 = 0;
  UART1->CR5 = 0;
  UART1->SR = 0;

  enableInterrupts();

  uart1_writestr("hello esp8266!\n");

  while(true){
    uart1_writestr("sent led\n");
    uint32_t ledcol[] = {0x12,0x44};
    led_sendcol(ledcol);
    //uart1_writestr("testing\n");
    tim2_wait(1);

    /*
    SETLEDBIT(LED_HIGHCK);
    tim2_wait(100);
    SETLEDBIT(LED_TIM2_LOW);
    tim2_wait(800000);


    SETLEDBIT(LED_LOWCK);
    tim2_wait(100);
    SETLEDBIT(LED_TIM2_LOW);
    // 1s / 1.25us
    tim2_wait(800000);
    */
  }
}


/** on assembly
;	src\main.c: 179: TIM2->SR1 = 0;
	mov	0x5304+0, #0x00         ; 1 cycle
;	src\main.c: 180: TIM2->CCR1L = next_data;
	mov	0x5312+0, _next_data+0  ; 1 cycle
;	src\main.c: 181: sent = true;
	mov	_sent+0, #0x01          ; 1 cycle
;	src\main.c: 182: up = true;
	mov	_up+0, #0x01            ; 1 cycle
 
 * on total, it use 4 cycles
 * for this, it shouldn't use more than 20 cycles (minus cycles in setting up the data)
*/
void it_tim2_ovf() INTERRUPT(ITC_IRQ_TIM2_OVF){
  TIM2->SR1 = 0;
  TIM2->CCR1H = next_data1;
  TIM2->CCR1L = next_data;
  sent = true;
  up = true;
}