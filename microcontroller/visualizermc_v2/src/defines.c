#include "defines.h"

void __rcc_init(){
  __HAL_RCC_USART1_CLK_ENABLE();
  __HAL_RCC_TIM2_CLK_ENABLE();

  // for HSE RC
  __HAL_RCC_GPIOD_CLK_ENABLE();

  // for USART1
  __HAL_RCC_GPIOA_CLK_ENABLE();
}

void __gpio_init(){
  // setup USART1
  GPIOA->CRH &= ~(GPIO_CRH_CNF9 | GPIO_CRH_MODE9 | GPIO_CRH_CNF10 | GPIO_CRH_MODE10);
  GPIOA->CRH |= 
    GPIO_CRH_MODE9_0 | GPIO_CRH_CNF9_1 |
    GPIO_CRH_CNF10_1
  ;

  GPIOA->ODR |= GPIO_PIN_10;    // means pull up input


  // setup USART2
  GPIOA->CRL &= ~(GPIO_CRL_CNF2 | GPIO_CRL_MODE2 | GPIO_CRL_CNF3 | GPIO_CRL_MODE3);
  GPIOA->CRL |=
    GPIO_CRL_MODE2_0 | GPIO_CRL_CNF2_1 |
    GPIO_CRL_CNF3_1
  ;

  GPIOA->ODR |= GPIO_PIN_3;     // means pull up input
}