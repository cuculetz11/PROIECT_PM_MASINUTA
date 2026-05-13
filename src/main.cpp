#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// --- PINI ---
#define TRIG_FATA 2
#define ECHO_FATA 3
#define TRIG_SPATE 4
#define ECHO_SPATE 5
#define LED_FATA 7

#define IN1 8   // PB0
#define IN2 9   // PB1
#define ENA 6   // PD6
#define PIN_ESC 10 // PB2

#define BUZZER A2 // Pinul A2 ca sa nu te incurce la upload
#define PIN_LDR A0

volatile char last_cmd = '0';
LiquidCrystal_I2C lcd(0x27, 16, 2);

// --- GRAFICA NOUA (Luata din codul tau misto) ---
byte eye[8]         = { B01000, B00100, B00110, B00110, B01111, B01111, B11111, B01110 };
byte blinked_eye[8] = { B00000, B00000, B00000, B00000, B00000, B11111, B00110, B00000 };
byte ebrow_1[8]     = { B00000, B00000, B00000, B00000, B00001, B00010, B00100, B00000 };
byte ebrow_2[8]     = { B00000, B00000, B01110, B10001, B00000, B00000, B00000, B00000 };
byte ebrow_3[8]     = { B00000, B00000, B00000, B00000, B10000, B01000, B00100, B00000 };
byte arrowAnim[8]   = { B00010, B00110, B01110, B11110, B01110, B00110, B00010, B00000 };

long get_dist(uint8_t trig, uint8_t echo) {
    digitalWrite(trig, LOW); delayMicroseconds(2);
    digitalWrite(trig, HIGH); delayMicroseconds(10);
    digitalWrite(trig, LOW);
    long duration = pulseIn(echo, HIGH, 20000); 
    if (duration == 0) return 999;
    return duration / 58;
}

void pwm_init() {
    pinMode(PIN_ESC, OUTPUT);
    TCCR1A = (1 << COM1B1) | (1 << WGM11);
    TCCR1B = (1 << WGM13) | (1 << WGM12) | (1 << CS11);
    ICR1 = 39999; 
    OCR1B = 3000; 

    pinMode(ENA, OUTPUT);
    TCCR0A = (1 << COM0A1) | (1 << WGM01) | (1 << WGM00);
    TCCR0B = (1 << CS01);
    OCR0A = 0; 
}

void setup() {
    Serial.begin(9600);
    
    pinMode(TRIG_FATA, OUTPUT); pinMode(ECHO_FATA, INPUT);
    pinMode(TRIG_SPATE, OUTPUT); pinMode(ECHO_SPATE, INPUT);
    pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
    pinMode(LED_FATA, OUTPUT); pinMode(BUZZER, OUTPUT);
    
    pwm_init();

    lcd.init();
    lcd.backlight();
    
    // Incarcam in memoria ecranului caracterele custom
    lcd.createChar(0, eye);
    lcd.createChar(1, blinked_eye);
    lcd.createChar(2, ebrow_1);
    lcd.createChar(3, ebrow_2);
    lcd.createChar(4, ebrow_3);
    lcd.createChar(5, arrowAnim);

    lcd.setCursor(0, 0); lcd.print(" Initializare...");
    delay(5000); // Timpul pentru ESC
    lcd.clear();
}

// Variabile globale pentru desenare inteligenta (fara delay)
int ultimul_mood = -1;
bool ultima_clipire = false;
unsigned long timp_urmat_clipire = 0;
bool ochi_inchisi = false;
uint16_t timer_serial = 0;
uint16_t frame_sageata = 0;

void loop() {
    unsigned long timp_curent = millis();

    // 1. Bluetooth
    if (Serial.available()) {
        char c = Serial.read();
        if (c != '\r' && c != '\n') last_cmd = c;
    }

    // 2. Senzori
    long df = get_dist(TRIG_FATA, ECHO_FATA);
    long ds = get_dist(TRIG_SPATE, ECHO_SPATE);
    int lumina = analogRead(PIN_LDR);

    if (lumina < 400) digitalWrite(LED_FATA, HIGH);
    else digitalWrite(LED_FATA, LOW);

    bool crash_iminent = (df < 15);
    if (crash_iminent || (ds < 15)) digitalWrite(BUZZER, HIGH);
    else digitalWrite(BUZZER, LOW);

    // 3. Motoare
    if (last_cmd != '0') {
        switch(last_cmd) {
            case 'w': OCR1B = 3400; break;
            case 'x': OCR1B = 2600; break;
            case 'c': OCR1B = 3000; break;
            case 's': digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW); OCR0A = 255; break;
            case 'd': digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH); OCR0A = 255; break;
            case 'a': digitalWrite(IN1, LOW); digitalWrite(IN2, LOW); OCR0A = 0; break;
        }
    }

    // ==========================================
    // 4. ANIMATII INTELIGENTE (Non-blocking)
    // ==========================================
    
    int mood = 0; // 0 = Normal, 1 = Angry (Crash), 2 = Marsarier
    if (last_cmd == 'x') mood = 2;
    else if (crash_iminent) mood = 1;

    // Logica de clipire aleatorie
    if (!ochi_inchisi && timp_curent > timp_urmat_clipire) {
        ochi_inchisi = true;
        timp_urmat_clipire = timp_curent + 150; // Tine ochii inchisi 150ms
    } else if (ochi_inchisi && timp_curent > timp_urmat_clipire) {
        ochi_inchisi = false;
        timp_urmat_clipire = timp_curent + random(2000, 5000); // Clipeste o data la 2-5 secunde
    }

    // Desenam ecranul DOAR DACA s-a schimbat ceva (pt a nu palpai)
    if (mood != ultimul_mood || ochi_inchisi != ultima_clipire || (mood == 2 && timer_serial > 10)) {
        if (mood != ultimul_mood) lcd.clear();

        if (mood == 2) { 
            // MODUL MARSARIER
            lcd.setCursor(0, 0); lcd.print(" [R] MARSARIER  ");
            lcd.setCursor(0, 1); lcd.print(" Dist: "); 
            if(ds == 999) lcd.print("Liber  "); else { lcd.print(ds); lcd.print("cm "); }
            
            // Animatie sageata
            if(frame_sageata % 2 == 0) { lcd.setCursor(14, 1); lcd.write(5); lcd.print(" "); }
            else { lcd.setCursor(14, 1); lcd.print(" "); lcd.write(5); }
            frame_sageata++;

        } else {
            // Desenam OCHII (pe randul 1, coloanele 3 si 12)
            lcd.setCursor(3, 1); lcd.write(ochi_inchisi ? 1 : 0);
            lcd.setCursor(12, 1); lcd.write(ochi_inchisi ? 1 : 0);

            if (mood == 0) { 
                // Sprancene Normale
                lcd.setCursor(2, 0); lcd.write(2); lcd.write(3); lcd.write(4); // Ochi stang
                lcd.setCursor(11, 0); lcd.write(2); lcd.write(3); lcd.write(4); // Ochi drept
                lcd.setCursor(6, 1); lcd.print("    "); // Sterge mesajul de crash
            } else if (mood == 1) { 
                // Sprancene Suparate (Crash)
                lcd.setCursor(2, 0); lcd.print(" "); lcd.write(3); lcd.write(4); // Fara varful exterior stang
                lcd.setCursor(11, 0); lcd.write(2); lcd.write(3); lcd.print(" "); // Fara varful exterior drept
                lcd.setCursor(6, 1); lcd.print("!!!!");
            }
        }

        ultimul_mood = mood;
        ultima_clipire = ochi_inchisi;
    }

    // 5. Debug Serial la fiecare secunda
    if (timer_serial++ > 20) {
        char buf[40];
        sprintf(buf, "[SYS] LDR:%d | DF:%ld | DS:%ld\n", lumina, df, ds);
        Serial.print(buf);
        timer_serial = 0;
    }

    // last_cmd = '0'; // Am comentat asta intentionat pt ca sa tina minte marsarierul.
    delay(50); // Bucla ruleaza f rapid, 20 cadre/sec
}