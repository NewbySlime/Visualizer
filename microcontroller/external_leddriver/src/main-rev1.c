#include <stm8s.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define true 1
#define false 0

// means baud rate of 1000000
#define BAUDRATEDIV 16

// max receiving for continous mode is based on how long the data is sent and processed that the numbers didn't reach reset code (300 us)
// since it needs to send a color data (~24 us) and how long it is processed for the first time (~50 us) plus sending a "done" code (~8 us)
// so maximal waiting time is (~8 + (delay from master) + ~24*n + ~45) us (the process of sending the data to LED isn't accounted, since
// the timing on the LED side is resetted)
// thus the maximum of receiving the color data in continous mode is 10 (floor((300 - ~53) / (~24)), delay from master isn't counted) 
#define MAX_RECVCOLDATA_CONTINOUS 10
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
static const uint8_t led_deltack = LED_HIGHCK - LED_LOWCK;

// Duty Cycle of 100%
#define LED_TIM2_HIGH 20
// Duty Cycle of 0%
#define LED_TIM2_LOW 0


/*    TIM4 params   */
// fCPU / (fCPU / 10^-5) - 1  // or in other words, interval of 10us
#define TIM4_ARR 159

#define TIM4_MSTOTICK(ms) ms*100


/*    program codes    */
// [SET] giving color data to the program
#define DATA 0x8
// [SET] setting the receiving config between continous or single shot mode
#define RECV_CONFIG 0x9
// [GET] tell master how many color (3 bytes) data it can receive in one data batch
#define MAXDATA 0x10
// [] tell master that the received data has been processed
#define DONE 0x11

#define RECV_CONFIG_CONTINOUS 1
#define RECV_CONFIG_SINGLE 0      // DEFAULT  


#define MIN(n, m) (n < m)? n: m

// 2/3 cycles 0.125/0.1875us
#define WAIT_SREG(reg, n) while(!(reg & n))

#define SETLEDBIT(n)\
  TIM2->SR1 = 0;\
  TIM2->CCR1L = n;\
  while(!(TIM2->SR1 & TIM2_SR1_UIF));


void deinit_gpio(GPIO_TypeDef *gpio){
  gpio->CR1 = GPIO_CR1_RESET_VALUE;
  gpio->CR2 = GPIO_CR2_RESET_VALUE;
  gpio->DDR = GPIO_DDR_RESET_VALUE;
}


void uart1_writebytes(const uint8_t *data, uint16_t length){
  for(uint16_t i = 0; i < length; i++){
    UART1->DR = data[i];
    WAIT_SREG(UART1->SR, UART1_SR_TC);
  }
}

// str is null-terminated string
void uart1_writestr(const char *str){
  uart1_writebytes((const uint8_t*)str, strlen(str));
}

uint8_t uart1_waitforbyte(){
  WAIT_SREG(UART1->SR, UART1_SR_RXNE);
  return UART1->DR;
}

void uart1_readbytes(uint8_t *data, uint16_t length){
  for(uint16_t i = 0; i < length; i++)
    data[i] = uart1_waitforbyte();
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


void led_reset(){
  TIM2->CCR1L = LED_TIM2_LOW;
  tim4_wait(5); // 50 us
}


#define _LOOP2(n)\
  data[idxbit] = ((col >> (n)) & 1) * led_deltack + LED_LOWCK;\
  idxbit++

#define _LOOP1()\
  col = cols[idx];\
  _LOOP2(0);\
  _LOOP2(1);\
  _LOOP2(2);\
  _LOOP2(3);\
  _LOOP2(4);\
  _LOOP2(5);\
  _LOOP2(6);\
  _LOOP2(7);\
  idx++
  


// colslen have to be a multiplication of 3
// and the function will reformat the color format from RGB to GRB
int idx = 0;
uint8_t data[24];
uint8_t idxbit = 0;
uint8_t col = 0;
void led_sendcol(uint8_t *cols, uint8_t colslen){
  idx = 0;

  while(idx < colslen){
    // setting up the data
    idxbit = 0;
    _LOOP1();
    _LOOP1();
    _LOOP1();
    
    SETLEDBIT(LED_TIM2_LOW);

    // G
    SETLEDBIT(data[15]);
    SETLEDBIT(data[14]);
    SETLEDBIT(data[13]);
    SETLEDBIT(data[12]);
    SETLEDBIT(data[11]);
    SETLEDBIT(data[10]);
    SETLEDBIT(data[9]);
    SETLEDBIT(data[8]);

    // R
    SETLEDBIT(data[7]);
    SETLEDBIT(data[6]);
    SETLEDBIT(data[5]);
    SETLEDBIT(data[4]);
    SETLEDBIT(data[3]);
    SETLEDBIT(data[2]);
    SETLEDBIT(data[1]);
    SETLEDBIT(data[0]);

    // B
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
}

#define _prog_senddone()\
  uint8_t code = DONE;\
  uart1_writebytes(&code, sizeof(uint8_t))


char _teststr[64];
uint8_t _ledbuffer[MAX_RECVCOLDATA*3];
int main(){
  disableInterrupts();

  // de-initiating
  deinit_gpio(GPIOD);

  CLK->HSITRIMR = 0;
  CLK->CKDIVR = 0;

  // setting up pin
  // PD4 is output mode
  GPIOD->DDR |= 1 << 4;
  GPIOD->CR1 |= 1 << 4; // push pull
  GPIOD->CR2 |= 1 << 4; // fast mode

  // PB4 is input mode
  GPIOB->CR1 |= 1 << 4; // pull up

  // PB5 is output mode
  GPIOB->CR1 |= 1 << 5; // pull up


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
  UART1->BRR2 = ((BAUDRATEDIV >> 8) & 0xF0) | (BAUDRATEDIV & 0x0F);
  UART1->BRR1 = (BAUDRATEDIV >> 4) & 0xFF;
  UART1->CR1 = 0;
  UART1->CR2 = UART1_CR2_REN | UART1_CR2_TEN;
  UART1->CR3 = 0;
  UART1->CR4 = 0;
  UART1->CR5 = 0;
  UART1->SR = 0;

  _ledbuffer[0] = 0xff;
  _ledbuffer[1] = 0xff;
  _ledbuffer[2] = 0xff;

  led_sendcol(_ledbuffer, 1);  

  uint16_t leddatalen;
  uint16_t dataindex;
  uint16_t _div;
  uint16_t _maxdata = MAX_RECVCOLDATA;
  while(true){
    uint8_t code = uart1_waitforbyte();
    switch(code){
      break; case DATA:{
        dataindex = 0;
        leddatalen = uart1_waitforbyte() | (uart1_waitforbyte() << 8);

        uint16_t ledlen = MIN(MAX_RECVCOLDATA, leddatalen-dataindex);
        uart1_readbytes(_ledbuffer, ledlen*3);
        
        dataindex += ledlen;
        led_sendcol(_ledbuffer, ledlen*3);

        _prog_senddone();
      }

      break; case RECV_CONFIG:{
        uint8_t _config = uart1_waitforbyte();
        if(_config == RECV_CONFIG_CONTINOUS)
          _maxdata = MAX_RECVCOLDATA_CONTINOUS;
        else if(_config == RECV_CONFIG_SINGLE);
          _maxdata = MAX_RECVCOLDATA;
        
        _prog_senddone();
      }

      break; case MAXDATA:{
        uart1_writebytes(&_maxdata, sizeof(_maxdata));
      }
    }
  }
}