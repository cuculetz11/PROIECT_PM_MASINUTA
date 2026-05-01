#include <stdio.h>
#include <avr/io.h>
#include <util/delay.h>

#include "adc.h"
#include "task1.h"

void task1(void)
{
    // Folosim 0 pentru canalul PC0 (unde am conectat AO al senzorului)
    uint16_t coin_value = myAnalogRead(0); 
    
    // Pragul determinat de tine: 24 (gol) vs 1000 (monedă)
    if (coin_value > 500) {
        printf("Moneda detectata! Valoare ADC: %d\n", coin_value);
    } else {
        printf("Senzor liber... (%d)\n", coin_value);
    }
    
    _delay_ms(500);
}