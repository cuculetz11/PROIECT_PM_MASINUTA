#include <stdio.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#include "usart.h"
// Lăsăm adc și ultrasonic deoparte pentru testul ăsta scurt

#define PM_BAUD 9600

// Variabilă globală în care salvăm comanda primită de la telefon
// "volatile" îi spune procesorului că această variabilă se schimbă "din umbră" (în ISR)
volatile char comanda_bt = 0; 

// Această rutină (ISR) "sare" automat de fiecare dată când Bluetooth-ul primește o literă
// Această rutină (ISR) "sare" automat
ISR(USART_RX_vect) {
    char caracter_primit = UDR0; // Citim ce a venit
    
    // Dacă NU este "Enter" (\r sau \n), atunci îl salvăm
    if (caracter_primit != '\r' && caracter_primit != '\n') {
        comanda_bt = caracter_primit; 
    }
}
int main()
{
  USART0_init(CALC_USART_UBRR(PM_BAUD));
  USART0_use_stdio();

  sei(); // Extrem de important! Activează întreruperile globale

  printf("Sistem pornit! Astept comenzi Bluetooth...\n");

  while(1) {
      
      // Dacă am primit ceva nou
      if (comanda_bt != 0) {
          printf("Am primit de la telefon comanda: %c\n", comanda_bt);
          
          // Aici vei pune mai târziu logica:
          // if (comanda_bt == 'F') motor_fata();
          // if (comanda_bt == 'S') motor_stop();

          comanda_bt = 0; // Resetăm variabila ca să așteptăm următoarea comandă
      }
      
      // Mașina își poate vedea de treabă aici (citit senzori, etc) fără să se blocheze
      _delay_ms(100); 
  }

  return 0;
}