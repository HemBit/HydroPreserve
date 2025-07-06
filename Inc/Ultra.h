#ifndef ULTRA_H
#define ULTRA_H

#include <stdint.h>

typedef struct {
    volatile uint32_t my_MODER;
    volatile uint32_t my_OTYPER;
    volatile uint32_t my_OSPEEDR;
    volatile uint32_t my_PUPDR;
    volatile uint32_t my_IDR;
    volatile uint32_t my_ODR;
    volatile uint32_t my_BSRR;
    volatile uint32_t my_LCKR;
    volatile uint32_t my_AFRL;
    volatile uint32_t my_AFRH;
} MY_GPIO_TypeDef;

typedef struct {
    volatile uint32_t my_CR1;
    volatile uint32_t my_CR2;
    volatile uint32_t my_SMCR;
    volatile uint32_t my_DIER;
    volatile uint32_t my_SR;
    volatile uint32_t my_EGR;
    volatile uint32_t my_CCMR1;
    volatile uint32_t my_CCMR2;
    volatile uint32_t my_CCER;
    volatile uint32_t my_CNT;
    volatile uint32_t my_PSC;
    volatile uint32_t my_ARR;
} MY_TIM_TypeDef;

typedef struct {
    volatile uint32_t my_CR;
    volatile uint32_t my_PLLCFGR;
    volatile uint32_t my_CFGR;
    volatile uint32_t my_CIR;
    volatile uint32_t my_AHB1RSTR;
    volatile uint32_t my_AHB2RSTR;
    volatile uint32_t my_AHB3RSTR;
    volatile uint32_t my_RESERVED1;
    volatile uint32_t my_APB1RSTR;
    volatile uint32_t my_APB2RSTR;
    volatile uint32_t my_RESERVED2[2];
    volatile uint32_t my_AHB1ENR;
    volatile uint32_t my_AHB2ENR;
    volatile uint32_t my_AHB3ENR;
    volatile uint32_t my_RESERVED3;
    volatile uint32_t my_APB1ENR;
    volatile uint32_t my_APB2ENR;
} MY_RCC_TypeDef;

#define my_GPIOA ((MY_GPIO_TypeDef *) 0x40020000)
#define my_GPIOB ((MY_GPIO_TypeDef *) 0x40020400)
#define my_TIM2 ((MY_TIM_TypeDef *) 0x40000000)
#define my_RCC ((MY_RCC_TypeDef *) 0x40023800)

#define my_TIM_SR_UIF (1 << 0)

// Function prototypes
void initUltrasonicSensor(void);
int measureDistance(void);
void my_TIM2_us_Delay(uint32_t delay);
void my_UpdateRelayState(double distance);
void my_SetRelayOff(void);
void my_ResetOverride(void);

// External variables for accessing data
extern double percentage;
extern uint8_t relayState;
extern uint8_t overrideActive; 

#endif /* ULTRA_H */