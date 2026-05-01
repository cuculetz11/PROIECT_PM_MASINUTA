#include <avr/io.h>
// #include "motor.h"

void motor_steering_init() {
    // Setăm pinii ca ieșire
    // PD6 = PWM (Pinul de Enable de pe L298N)
    // PB0 = IN1 (Direcție Stânga)
    // PB1 = IN2 (Direcție Dreapta)
    
    DDRD |= (1 << PD6); 
    DDRB |= (1 << PB0) | (1 << PB1);

    // Inițializare PWM pe Timer 0 (Fast PWM, non-inverting pe OC0A)
    // Laboratorul 3!
    TCCR0A = (1 << COM0A1) | (1 << WGM01) | (1 << WGM00);
    TCCR0B = (1 << CS01) | (1 << CS00); // Prescaler 64
    
    OCR0A = 0; // Motoarele pornesc oprite (Duty cycle 0)
}

void motor_steer(int direction, uint8_t power) {
    // power este între 0 și 255 (0% - 100% duty cycle)
    OCR0A = power; 

    if (direction == 1) {        // Virează Dreapta
        PORTB |= (1 << PB0);
        PORTB &= ~(1 << PB1);
    } else if (direction == -1) { // Virează Stânga
        PORTB &= ~(1 << PB0);
        PORTB |= (1 << PB1);
    } else {                     // Revine la centru (sau oprit liber)
        PORTB &= ~(1 << PB0);
        PORTB &= ~(1 << PB1);
    }
}