#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// --- PINI ---
#define TRIG_FATA 2
#define ECHO_FATA 3
#define TRIG_SPATE 4
#define ECHO_SPATE 5
#define LED_FATA 7
#define LED_SPATE A3  // NOU: Pinul PC3 pentru stopurile rosii din spate

#define IN1 8   // PB0
#define IN2 9   // PB1
#define ENA 6   // PD6
#define PIN_ESC 10 // PB2

#define BUZZER A2
#define PIN_LDR A0
#define PIN_STATE A1 // NOU: Starea conexiunii HC-05

// --- GRAFICA CUSTOM LCD ---
byte eye[8]         = { B01000, B00100, B00110, B00110, B01111, B01111, B11111, B01110 };
byte blinked_eye[8] = { B00000, B00000, B00000, B00000, B00000, B11111, B00110, B00000 };
byte ebrow_1[8]     = { B00000, B00000, B00000, B00000, B00001, B00010, B00100, B00000 };
byte ebrow_2[8]     = { B00000, B00000, B01110, B10001, B00000, B00000, B00000, B00000 };
byte ebrow_3[8]     = { B00000, B00000, B00000, B00000, B10000, B01000, B00100, B00000 };
byte arrowAnim[8]   = { B00010, B00110, B01110, B11110, B01110, B00110, B00010, B00000 };

LiquidCrystal_I2C lcd(0x27, 16, 2);

// --- VARIABILE DE STARE ---
unsigned long last_cmd_millis = 0;
bool failsafe_active = true;

char motor_dir = 'c';   // w, x, c
char motor_steer = 'a'; // a, s, d
int gear = 1;           // 1, 2, 3, 4 (turbo)
int headlight_mode = 0; // 0=auto, 1=on, 2=off

unsigned long horn_end_time = 0;

int ultimul_mood = -1;
bool ultima_clipire = false;
char ultimul_steer = 'a';
unsigned long timp_urmat_clipire = 0;
bool ochi_inchisi = false;

uint16_t timer_serial = 0;
uint16_t frame_sageata = 0;
unsigned long last_marsarier_anim = 0;
unsigned long last_deconectat_anim = 0;

unsigned long last_buzzer_toggle = 0;
bool buzzer_state = false;
unsigned long last_telemetry_time = 0;

// --- FUNCTII ---
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
    // Fast PWM 10-bit pentru Timer 1
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
    pinMode(LED_SPATE, OUTPUT); // Initializare stopuri
    pinMode(PIN_STATE, INPUT);
    pinMode(PIN_LDR, INPUT);
    
    pwm_init();

    lcd.init();
    lcd.backlight();
    
    lcd.createChar(0, eye);
    lcd.createChar(1, blinked_eye);
    lcd.createChar(2, ebrow_1);
    lcd.createChar(3, ebrow_2);
    lcd.createChar(4, ebrow_3);
    lcd.createChar(5, arrowAnim);

    lcd.setCursor(0, 0); lcd.print(" Initializare...");
    delay(5000); // Wait for ESC arming
    lcd.clear();
}

void loop() {
    unsigned long timp_curent = millis();
    bool bt_connected = digitalRead(PIN_STATE) == HIGH;

    // ==========================================
    // 1. BLUETOOTH & PARSER
    // ==========================================
    if (Serial.available()) {
        char c = Serial.read();
        if (c == 'w' || c == 's' || c == 'd' || c == 'a' || c == 'x' || c == 'c' || 
            c == '1' || c == '2' || c == '3' || c == 't' || c == 'l' || c == 'h') {
            
            last_cmd_millis = timp_curent;
            failsafe_active = false;
            
            switch(c) {
                case 'w': motor_dir = 'w'; break;
                case 'x': motor_dir = 'x'; break;
                case 'c': motor_dir = 'c'; break;
                case 's': motor_steer = 's'; break;
                case 'd': motor_steer = 'd'; break;
                case 'a': motor_steer = 'a'; break;
                case '1': gear = 1; break;
                case '2': gear = 2; break;
                case '3': gear = 3; break;
                case 't': gear = 4; break;
                case 'l': headlight_mode = (headlight_mode + 1) % 3; break;
                case 'h': horn_end_time = timp_curent + 500; break;
            }
        }
    }

    // ==========================================
    // 2. FAILSAFE (WATCHDOG)
    // ==========================================
    if (timp_curent - last_cmd_millis > 500) {
        motor_dir = 'c';
        motor_steer = 'a';
        failsafe_active = true;
    }

    // ==========================================
    // 3. SENZORI & FARURI (LDR)
    // ==========================================
    long df = get_dist(TRIG_FATA, ECHO_FATA);
    long ds = get_dist(TRIG_SPATE, ECHO_SPATE);
    int lumina = analogRead(PIN_LDR);

    if (headlight_mode == 1) {
        digitalWrite(LED_FATA, HIGH);
    } else if (headlight_mode == 2) {
        digitalWrite(LED_FATA, LOW);
    } else { // Auto
        digitalWrite(LED_FATA, lumina < 400 ? HIGH : LOW);
    }

    // ==========================================
    // 4. AEB (Autonomous Emergency Braking)
    // ==========================================
    bool aeb_fata = (df < 10);
    bool aeb_spate = (ds < 10);

    if (aeb_fata && motor_dir == 'w') motor_dir = 'c'; // Ignoram acceleratia spre obstacol fata
    if (aeb_spate && motor_dir == 'x') motor_dir = 'c'; // Ignoram acceleratia spre obstacol spate

    // ==========================================
    // 4.5 STOPURI (Sistem inteligent de frana / marsarier)
    // ==========================================
    // Stopurile se aprind DACA: masina e pe loc ('c'), da cu spatele ('x') SAU a intrat frana automata (AEB)
    if (motor_dir == 'c' || motor_dir == 'x' || aeb_fata || aeb_spate) {
        digitalWrite(LED_SPATE, HIGH);
    } else {
        // Se sting cand masina merge inainte normal ('w')
        digitalWrite(LED_SPATE, LOW);
    }

    // ==========================================
    // 5. BUZZER (Prioritate: Claxon > AEB > Parcare)
    // ==========================================
    long min_dist = min(df, ds);
    bool horn_active = (timp_curent < horn_end_time);

    if (horn_active) {
        digitalWrite(BUZZER, HIGH);
    } else if (aeb_fata || aeb_spate) {
        digitalWrite(BUZZER, HIGH); // Continuu pt urgenta
    } else {
        // Senzor parcare progresiv
        if (min_dist > 40) {
            digitalWrite(BUZZER, LOW);
        } else {
            int interval = 0;
            if (min_dist > 25) interval = 600; // Rar
            else if (min_dist >= 10) interval = 200; // Des
            
            if (timp_curent - last_buzzer_toggle >= interval) {
                buzzer_state = !buzzer_state;
                digitalWrite(BUZZER, buzzer_state ? HIGH : LOW);
                last_buzzer_toggle = timp_curent;
            }
        }
    }

    // ==========================================
    // 6. MOTOARE (Tractiune & Directie)
    // ==========================================
    // Directie (Timer 0)
    if (motor_steer == 'a') {
        digitalWrite(IN1, LOW); digitalWrite(IN2, LOW); OCR0A = 0;
    } else if (motor_steer == 's') {
        digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW); OCR0A = 255;
    } else if (motor_steer == 'd') {
        digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH); OCR0A = 255;
    }

    // Tractiune (Timer 1)
    if (motor_dir == 'c') {
        OCR1B = 3000;
    } else if (motor_dir == 'x') {
        OCR1B = 2600; // Marsarier fix
    } else if (motor_dir == 'w') {
        if (gear == 1) OCR1B = 3100;
        else if (gear == 2) OCR1B = 3200;
        else if (gear == 3) OCR1B = 3300;
        else if (gear == 4) OCR1B = 3400; // Turbo
    }

    // ==========================================
    // 7. ANIMATII I2C LCD (Non-blocking & Flicker-free)
    // ==========================================
    int mood = 0; // 0=Ochi normali, 1=Angry/Stop (AEB), 2=Marsarier (Senzor parcare), 3=Deconectat
    if (!bt_connected) mood = 3;
    else if (aeb_fata || aeb_spate) mood = 1;
    else if (motor_dir == 'x') mood = 2;

    // Logica de clipire aleatorie
    if (!ochi_inchisi && timp_curent > timp_urmat_clipire) {
        ochi_inchisi = true;
        timp_urmat_clipire = timp_curent + 150; 
    } else if (ochi_inchisi && timp_curent > timp_urmat_clipire) {
        ochi_inchisi = false;
        timp_urmat_clipire = timp_curent + random(2000, 5000); 
    }

    bool act_marsarier = (mood == 2 && timp_curent - last_marsarier_anim >= 250);
    bool act_deconectat = (mood == 3 && timp_curent - last_deconectat_anim >= 500);

    // Actualizare ecran
    if (mood != ultimul_mood || ochi_inchisi != ultima_clipire || motor_steer != ultimul_steer || act_marsarier || act_deconectat) {
        
        if (mood != ultimul_mood) {
            lcd.clear(); // Evitam flickerul, dam clear DOAR la tranzitia de stare
        }

        if (mood == 3) {
            // Deconectat
            if (mood != ultimul_mood || act_deconectat) {
                lcd.setCursor(0, 0); lcd.print("Caut conexiune  ");
                lcd.setCursor(0, 1); 
                int puncte = (timp_curent / 500) % 4; // Animatie 0-3 puncte
                for(int i=0; i<3; i++) {
                    if (i < puncte) lcd.print("."); else lcd.print(" ");
                }
                last_deconectat_anim = timp_curent;
            }
        } 
        else if (mood == 2) { 
            // Marsarier
            if (mood != ultimul_mood || act_marsarier) {
                lcd.setCursor(0, 0); lcd.print(" [R] MARSARIER  ");
                lcd.setCursor(0, 1); lcd.print(" Dist: "); 
                if(ds == 999) lcd.print("Liber  "); else { lcd.print(ds); lcd.print("cm   "); }
                
                // Animatie sageata
                if(frame_sageata % 2 == 0) { lcd.setCursor(14, 1); lcd.write(5); lcd.print(" "); }
                else { lcd.setCursor(14, 1); lcd.print(" "); lcd.write(5); }
                frame_sageata++;
                last_marsarier_anim = timp_curent;
            }
        } 
        else {
            // Ochi Animati
            if (mood != ultimul_mood || ochi_inchisi != ultima_clipire || motor_steer != ultimul_steer) {
                int pos_left_eye = 3;
                int pos_right_eye = 12;
                // Deplasare privire in functie de directie
                if (motor_steer == 's') { pos_left_eye = 2; pos_right_eye = 11; }
                else if (motor_steer == 'd') { pos_left_eye = 4; pos_right_eye = 13; }

                lcd.setCursor(0, 1); lcd.print("                "); // Curatam doar randul 1
                lcd.setCursor(pos_left_eye, 1); lcd.write(ochi_inchisi ? 1 : 0);
                lcd.setCursor(pos_right_eye, 1); lcd.write(ochi_inchisi ? 1 : 0);

                if (mood == 0) { 
                    lcd.setCursor(0, 0); lcd.print("                "); // Curatam randul 0 pentru sprancene normale
                    lcd.setCursor(pos_left_eye-1, 0); lcd.write(2); lcd.write(3); lcd.write(4);
                    lcd.setCursor(pos_right_eye-1, 0); lcd.write(2); lcd.write(3); lcd.write(4);
                } else if (mood == 1) { 
                    lcd.setCursor(0, 0); lcd.print("                "); // Curatam randul 0 pentru sprancene agresive
                    lcd.setCursor(pos_left_eye-1, 0); lcd.print(" "); lcd.write(3); lcd.write(4);
                    lcd.setCursor(pos_right_eye-1, 0); lcd.write(2); lcd.write(3); lcd.print(" ");
                    lcd.setCursor(6, 1); lcd.print("STOP");
                }
            }
        }

        ultimul_mood = mood;
        ultima_clipire = ochi_inchisi;
        ultimul_steer = motor_steer;
    }

    // ==========================================
    // 8. TELEMETRIE (Catre Android)
    // ==========================================
    if (timp_curent - last_telemetry_time >= 200) {
        char buf[30];
        sprintf(buf, "F:%ld S:%ld\n", df, ds);
        Serial.print(buf);
        last_telemetry_time = timp_curent;
    }

    delay(20); // Bucla ruleaza la max ~25 cadre/sec
}