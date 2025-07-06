#include "Ultra.h"
#define ARM_MATH_CM4

#define DISTANCE_THRESHOLD_LOW 6.0    // Turn relay OFF if distance < 6cm
#define DISTANCE_THRESHOLD_HIGH 15.0  // Turn relay ON if distance > 15cm

void my_GPIO_Init(void);
void my_TIM2_us_Delay(uint32_t delay); //TIM2 for generating 10us pulse for trig pin
uint32_t data;
double time, dist;

double percentage = 0.0;
uint8_t relayState = 0; 
uint8_t overrideActive = 0; 

void my_GPIO_Init(){
    
    my_RCC->my_AHB1ENR |= 1; //Enable GPIOA clock
    my_GPIOA->my_MODER |= 1<<10; //Set the PA5 pin to output mode
    
    my_GPIOA->my_MODER &= ~(0x00003000); //Set PA6 to input mode
    
    my_RCC->my_AHB1ENR |= (1<<1); //Enable GPIOB clock
    my_GPIOB->my_MODER |= 1<<12; //Set PB6 to output mode
    
    my_GPIOB->my_BSRR |= 0x00000040; // Set PB6 HIGH to turn relay OFF
    relayState = 0;
}

void my_TIM2_us_Delay(uint32_t delay){
    my_RCC->my_APB1ENR |=1; //Start the clock for the timer peripheral
    my_TIM2->my_ARR = (int)(delay/0.0625); // Total period of the timer
    my_TIM2->my_CNT = 0;
    my_TIM2->my_CR1 |= 1; //Start the Timer
    while(!(my_TIM2->my_SR & my_TIM_SR_UIF)){} //Polling the update interrupt flag
    my_TIM2->my_SR &= ~(0x0001); //Reset the update interrupt flag
}

void my_UpdateRelayState(double distance) {
    if (overrideActive && distance < DISTANCE_THRESHOLD_LOW) {
        overrideActive = 0;
    }
    
    if (!overrideActive) {
        if (distance > DISTANCE_THRESHOLD_HIGH && relayState == 0) {
            my_GPIOB->my_BSRR |= 0x00400000; // Set PB6 LOW to turn relay ON
            relayState = 1;
        } 
        else if (distance < DISTANCE_THRESHOLD_LOW && relayState == 1) {
            my_GPIOB->my_BSRR |= 0x00000040; // Set PB6 HIGH to turn relay OFF
            relayState = 0;
        }
    }
}

// Function to manually override and turn off relay
void my_SetRelayOff(void) {
    overrideActive = 1;
    
    my_GPIOB->my_BSRR |= 0x00000040; // Set PB6 HIGH to turn relay OFF
    relayState = 0;
}

// Function to disable override and return to normal operation
void my_ResetOverride(void) {
    overrideActive = 0;
}

// Function to measure distance using ultrasonic sensor
int measureDistance(void) {
    data = 0;
    
    while (my_GPIOA->my_IDR & 64) {
    }
    
    //1. Sending 10us pulse to trigger the ultrasonic sensor
    my_GPIOA->my_BSRR |= 0x00200000; // Make PA5 low again 
    my_TIM2_us_Delay(2);
    my_GPIOA->my_BSRR |= 0x00000020; // PA5 set to High
    my_TIM2_us_Delay(10); // wait for 10us
    my_GPIOA->my_BSRR |= 0x00200000; // Make PA5 low again
    
    while (!(my_GPIOA->my_IDR & 64)) {
        data++;
        if (data > 100000) {
            return 0; 
        }
    }
    
    data = 0; 
    
    //2. Measure the pulse width of the pulse sent from the echo pin by polling IDR for port A
    while (my_GPIOA->my_IDR & 64) {
        data = data + 1;
        if (data > 100000) {
            return 0; 
        }
    }
    
    //3.Converting the gathered data into distance in cm
    if (data > 0) {
        time = data * (0.0625 * 0.00001); // Convert to seconds
        dist = ((time * 340) / 2) * 100;  // Convert to cm (speed of sound = 340m/s)
        
        if (dist > 400) dist = 400; // Maximum reasonable distance for HC-SR04
        if (dist < 2) dist = 2;     // Minimum reasonable distance for HC-SR04
        
        percentage = 100 - ((dist / 15.0) * 100);
        
        if (percentage > 100) percentage = 100;
        if (percentage < 0) percentage = 0;
        
        //4. Update relay state based on measured distance
        my_UpdateRelayState(dist);
        
        return 1; 
    }
    
    return 0;
}

// Initialize ultrasonic sensor
void initUltrasonicSensor(void) {
    my_RCC->my_CFGR |= 0<<10; 
    my_GPIO_Init();
    my_GPIOA->my_BSRR |= 0x00200000; 
    my_TIM2_us_Delay(100000);
}