#include <stm8s.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define true 1
#define false 0

// means baud rate of 160000
#define BAUDRATEDIV 100
// means baud rate of 8000
//#define BAUDRATEDIV 2000
#define MAX_RECVCOLDATA 254

/*    TIM2 params   */
// fCPU / (fCPU / 1.25*10^-6) - 1
#define LED_TIM2_ARR 19

// data low (0)
// Duty Cycle of 30%
#define LED_LOWCK 6
// data high (1)
// Duty Cycle of 70%
#define LED_HIGHCK 14
uint8_t led_deltack = LED_HIGHCK - LED_LOWCK;

// Duty Cycle of 100%
#define LED_TIM2_HIGH 20
// Duty Cycle of 0%
#define LED_TIM2_LOW 0


/*    TIM4 params   */
// fCPU / (fCPU / 10^-5) - 1  // or in other words, interval of 10us
#define TIM4_ARR 159

#define TIM4_MSTOTICK(ms) ms*100


/*    program codes    */
#define STARTDATA 0x7
#define DATA 0x8


#define MIN(n, m) (n < m)? n: m

#define WAIT_SREG(reg, n) while(!(reg & n));

#define SETLEDBIT(n) TIM2->SR1 = 0;\
TIM2->CCR1L = n;\
while(!(TIM2->SR1 & TIM2_SR1_UIF));


void deinit_gpio(GPIO_TypeDef *gpio){
  gpio->CR1 = GPIO_CR1_RESET_VALUE;
  gpio->CR2 = GPIO_CR2_RESET_VALUE;
  gpio->DDR = GPIO_DDR_RESET_VALUE;
}

// str is null-terminated string
void uart1_writestr(const char *str){
  int idx = 0;
  while(str[idx]){
    UART1->DR = str[idx];
    WAIT_SREG(UART1->SR, UART1_SR_TC);
    idx++;
  }
}

uint8_t uart1_waitforbyte(){
  WAIT_SREG(UART1->SR, UART1_SR_RXNE);
  return UART1->DR;
}

void uart1_readbytes(uint8_t *data, uint16_t length){
  uint16_t i = 0;
  for(; i < length; i++)
    data[i] = uart1_waitforbyte();
  
  char __teststr[16];
  sprintf(__teststr, "rbs %d\n", i);
  uart1_writestr(__teststr);
}

int uart1_readstr(char *str, int length){
  for(int i = 0; i < length; i++){
    str[i] = uart1_waitforbyte();
    if(str[i] == '\n')
      return i;
  }

  return 0;
}

// the timer will use 10us per tick
void tim4_wait(uint32_t tickovf){
  while(tickovf){
    TIM4->SR1 = 0;
    WAIT_SREG(TIM4->SR1, TIM4_SR1_UIF);
    tickovf--;
  }
}


// colslen have to be a multiplication of 3
// and the function will reformat the color format of GRB to RGB
void led_sendcol(uint8_t *cols, uint8_t colslen, bool stop){
  int idx = 0;
  uint8_t data[24];
  while(idx < colslen){
    // setting up the data
    uint8_t idxbit = 0;
    for(uint8_t o = 0; o < 3; o++){
      uint8_t col = cols[idx];
      for(uint8_t i = 0; i < 8; i++){
        data[idxbit] = ((col >> i) & 1) * led_deltack + LED_LOWCK;
        idxbit++;
      }

      idx++;
    }
    
    SETLEDBIT(LED_TIM2_LOW);
    SETLEDBIT(data[15]);
    SETLEDBIT(data[14]);
    SETLEDBIT(data[13]);
    SETLEDBIT(data[12]);
    SETLEDBIT(data[11]);
    SETLEDBIT(data[10]);
    SETLEDBIT(data[9]);
    SETLEDBIT(data[8]);
    SETLEDBIT(data[7]);
    SETLEDBIT(data[6]);
    SETLEDBIT(data[5]);
    SETLEDBIT(data[4]);
    SETLEDBIT(data[3]);
    SETLEDBIT(data[2]);
    SETLEDBIT(data[1]);
    SETLEDBIT(data[0]);
    SETLEDBIT(data[23]);
    SETLEDBIT(data[22]);
    SETLEDBIT(data[21]);
    SETLEDBIT(data[20]);
    SETLEDBIT(data[19]);
    SETLEDBIT(data[18]);
    SETLEDBIT(data[17]);
    SETLEDBIT(data[16]);
    TIM2->CCR1L = LED_TIM2_LOW;
  }

  if(stop)
    tim4_wait(5); // 50 us
  else
    TIM2->CCR1L = LED_TIM2_HIGH;
}

 
uint8_t _ledbuffer[MAX_RECVCOLDATA*3];
int main(){
  disableInterrupts();

  // de-initiating
  deinit_gpio(GPIOD);

  CLK->HSITRIMR = 0;
  CLK->CKDIVR = 0;

  // setting up pin
  GPIOD->DDR |= 1 << 4; // PD4 is output mode
  GPIOD->CR1 |= 1 << 4; // push pull
  GPIOD->CR2 |= 1 << 4; // fast mode

  // setting up tim2
  TIM2->IER = 0;
  TIM2->EGR = TIM2_EGR_UG;
  TIM2->CCMR1 = TIM2_OCMODE_PWM2 | TIM2_CCMR_OCxPE;
  TIM2->CCER1 = TIM2_CCER1_CC1P | TIM2_CCER1_CC1E;
  TIM2->PSCR = TIM2_PRESCALER_1;
  TIM2->ARRH = LED_TIM2_ARR >> 8;
  TIM2->ARRL = LED_TIM2_ARR & 0xFF;
  TIM2->CCR1H = LED_TIM2_LOW >> 8;
  TIM2->CCR1L = LED_TIM2_LOW & 0xFF;
  TIM2->CR1 = TIM2_CR1_CEN;

  //setting up tim4
  TIM4->IER = 0;
  TIM4->EGR = TIM4_EGR_UG;
  TIM4->PSCR = TIM4_PRESCALER_1;
  TIM4->ARR = TIM4_ARR;
  TIM4->CR1 = TIM4_CR1_CEN;
  
  // setting up uart1
  UART1->BRR2 = ((BAUDRATEDIV >> 12) & 0xF) | (BAUDRATEDIV & 0xF);
  UART1->BRR1 = (BAUDRATEDIV >> 4) & 0xFF;
  UART1->CR1 = 0;
  UART1->CR2 = UART1_CR2_REN;
  UART1->CR2 |= UART1_CR2_TEN;
  UART1->CR3 = 0;
  UART1->CR4 = 0;
  UART1->CR5 = 0;
  UART1->SR = 0;

  _ledbuffer[0] = 0xff;
  _ledbuffer[1] = 0xff;
  _ledbuffer[2] = 0xff;

  led_sendcol(_ledbuffer, 1, true);  
  
  uint16_t leddatalen = 0;
  uint16_t dataindex = 0;
  while(true){
    uint8_t code = uart1_waitforbyte();
    switch(code){
      // note: if something wrong in here, then the problem might reside in how this board treat numbers
      break; case STARTDATA:{
        leddatalen = uart1_waitforbyte();
        leddatalen |= uart1_waitforbyte() << 8;
        dataindex = 0;
      }

      break; case DATA:{
        if(dataindex < leddatalen){
          uint16_t ledlen = MIN(MAX_RECVCOLDATA, leddatalen-dataindex);
          uart1_readbytes(_ledbuffer, ledlen*3);
          
          dataindex += ledlen;
          led_sendcol(_ledbuffer, ledlen*3, (dataindex >= leddatalen));
        }
      } 
    }
  }
}