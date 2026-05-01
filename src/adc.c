#include <avr/io.h>
#include "adc.h"

void adc_init() {
    ADCSRA = (1 << ADEN) | (7 << ADPS0); // Enable ADC and set prescaler to 128
    ADMUX = (1 << REFS0); // AVcc reference voltage
}

uint16_t myAnalogRead(uint8_t channel) {
    channel &= 0b00000111;

    ADMUX &= 0b11100000;
    ADMUX |= channel;
    
    ADCSRA |= (1 << ADSC);
    
    while (ADCSRA & (1 << ADSC));
    
    return ADC;
}