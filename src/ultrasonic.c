#include <avr/io.h>
#include <util/delay.h>

void ultrasonic_init() {
    DDRD |= (1 << PD2);  // Trig ca ieșire
    DDRD &= ~(1 << PD4); // Echo ca intrare
}

uint16_t get_distance() {
    // 1. Trimitem semnalul de Trigger
    PORTD &= ~(1 << PD2);
    _delay_us(2);
    PORTD |= (1 << PD2);
    _delay_us(10);
    PORTD &= ~(1 << PD2);

    // 2. Așteptăm să înceapă răspunsul (Echo HIGH)
    uint32_t counter = 0;
    while (!(PIND & (1 << PD4))) {
        counter++;
        if (counter > 100000) return 0; // Timeout
    }

    // 3. Măsurăm cât timp stă Echo pe HIGH
    TCNT1 = 0; // Resetăm Timer 1
    TCCR1B = (1 << CS11); // Pornim Timer 1 (Prescaler 8)

    while (PIND & (1 << PD4)) {
        if (TCNT1 > 40000) break; // Prea departe
    }

    TCCR1B = 0; // Oprim Timer 1

    // 4. Calculăm distanța în cm
    // Distanța = (Timp * Viteza Sunetului) / 2
    // La 16MHz cu prescaler 8, un tick e 0.5us
    return TCNT1 / 116; 
}