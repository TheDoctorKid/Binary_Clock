#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <avr/sleep.h>

volatile uint8_t seconds = 0;
volatile uint8_t minutes = 0;
volatile uint8_t hours = 0;
volatile uint8_t sleepMode = 0;
volatile uint16_t periodTime = 0; // Schaltsekunde Zusatz

const uint8_t hoursMapping [5]= {PD7, PD6, PD5, PD1, PD0};

void initPWM() 
{
    DDRB |= (1 << PB1) | (1 << PB2); // PB1 (OC1A), PB2 (OC1B) als Ausgang

    TCCR1A = (1 << WGM10) | (1 << WGM12) | (1 << COM1A1) | (1 << COM1B1) | (1 << COM1A0) | (1 << COM1B0); 
    TCCR1B = (1 << CS11); // Prescaler = 8

    OCR1A = 10; // Helligkeit der Stunden-LEDs
    OCR1B = 10; // Helligkeit Minuten-LEDs
}

void disablePWM() 
{
    // PWM-Funktionalität deaktivieren
    TCCR1A = 0;
    TCCR1B = 0;
    // Pins als normale Ausgänge setzen
    PORTB &= ~((1 << PB1) | (1 << PB2));
}

void initLEDmitTundPWM() 
{   
    // LED-, Taster-Pins und PWM setzen
    DDRB |= (1 << PB0); // Sekunden Ausgabe für Zeitmessung

    DDRC |= 0x3F; // PC0-PC5 als Ausgang für Minuten-LEDs
    DDRD |= (1 << PD0) | (1 << PD1) | (1 << PD5) | (1 << PD6) | (1 << PD7); // Stunden-LEDs
    PORTD |= (1 << PD2) | (1 << PD3) | (1 << PD4); // Pull-Ups für Taster aktivieren
    initPWM();
}


void initTimer2() 
{
    ASSR |= (1 << AS2); // Aktivierung asynchronen Modus 
    TCCR2B = (1 << CS22) | (1 << CS20); // Prescaler 128 → (32768 Hz / 128) = 256
    TIMSK2 = (1 << TOIE2); // Interrupt
    
}

void updateLEDs() 
{
    // Aktualisierung LED-Anzeige
    if(sleepMode) {return;}

    PORTC =(PORTC & ~0b00111111) | (minutes & 0x3F); // Minuten LED-Maske
    uint8_t hours_display = 0;

    for( uint8_t i = 0 ; i < 5; i++)
    {
        if (hours & (1 << i))
        {
            hours_display |= (1 << hoursMapping[i]);
        }
    }

    uint8_t mask =  (1 << PD7) | (1 << PD0) | (1<< PD1) | (1<< PD5) | (1<< PD6);
    
    PORTD = (PORTD & ~mask) | ((hours_display & mask)); // Änderung auf relevanter Stundenanzeige
}

void turnOffLEDs() {
    // Alle LEDs ausschalten
    PORTC &= ~0x3F;  // Minuten-LEDs aus
    uint8_t mask = (1 << PD7) | (1 << PD0) | (1 << PD1) | (1 << PD5) | (1 << PD6);
    PORTD &= ~mask;  // Stunden-LEDs aus
    PORTB &= ~(1 << PB0); // Sekunden-LED aus
}

// ISR für Timer2: Sekundenzähler
ISR(TIMER2_OVF_vect) 
{
    seconds++;
    periodTime++;
    
    // Schaltsekunde
    if (periodTime == 15105 & seconds >= 1)
    {
        seconds--;
        periodTime = 0;        
    } 
    
    PORTB ^= (1 << PB0);
    if (seconds >= 60) 
    {
        seconds = 0;
        minutes++;
        if (minutes >= 60) 
        {
            minutes = 0;
            hours = (hours + 1) % 24;
        }
    }
}

void CheckSleepMode(){
    if (sleepMode) 
    {   
        disablePWM();
        turnOffLEDs();
        set_sleep_mode(SLEEP_MODE_PWR_SAVE);
        sleep_enable();
        sleep_cpu();
    } 
    else 
    {
        initPWM();
        set_sleep_mode(SLEEP_MODE_IDLE);
        sleep_enable();   
    }
}

void toggleSleepMode() 
{
    sleepMode = !sleepMode;
}

//TASTER
void checkButtons() 
{
    static uint8_t taster2_gedrueckt = 0;
    static uint8_t taster3_gedrueckt = 0;
    static uint16_t simultaneous_press_timer = 0;
    
    // Zustände der Tasten speichern
    uint8_t pd2_gedrueckt = !(PIND & (1 << PD2)); // Stunden-Taster (Taster 3)
    uint8_t pd3_gedrueckt = !(PIND & (1 << PD3)); // Minuten-Taster (Taster 2)
    uint8_t pd4_gedrueckt = !(PIND & (1 << PD4)); // Sleep-Taster (Taster 1)
    
    // Sleep-Taster
    if (pd4_gedrueckt) { 
        _delay_ms(100); // Entprellen
        if (!(PIND & (1 << PD4))) 
        {
            toggleSleepMode();
            while (!(PIND & (1 << PD4))); // Warten bis losgelassen wird
        }
    }
    
    // (Taster 2 + Taster 3)
    if (pd2_gedrueckt && pd3_gedrueckt) 
    {
        simultaneous_press_timer++;
        
        // nach ca. 300ms Reset ausführen
        if (simultaneous_press_timer > 6) {
            _delay_ms(100);
            if (!(PIND & (1 << PD2)) && !(PIND & (1 << PD3))) {
                if (sleepMode) {
                    toggleSleepMode();
                } else {
                    // Reset durchführen
                    seconds = 0;
                    minutes = 0;
                    hours = 0;
                }
                while (!(PIND & (1 << PD2)) || !(PIND & (1 << PD3)));
                
                // Verhindern, dass die Einzelfunktionen ausgelöst werden
                taster2_gedrueckt = 0;
                taster3_gedrueckt = 0;
                simultaneous_press_timer = 0;
                return;
            }
        
        }
    } else {
        simultaneous_press_timer = 0; // Reset Timer
        
        // Minuten hochzählen (Taster 2)
        if (pd3_gedrueckt && !taster2_gedrueckt) 
        { 
            _delay_ms(100); 
            if (!(PIND & (1 << PD3))) 
            {
                taster2_gedrueckt = 1;
                if (sleepMode) {
                    toggleSleepMode();
                } else {
                    minutes = (minutes + 1) % 60;
                }
            }
        } 
        else if (!pd3_gedrueckt && taster2_gedrueckt) 
        {
            taster2_gedrueckt = 0;
        }

        // Stunden hochzählen (Taster 3)
        if (pd2_gedrueckt && !taster3_gedrueckt) 
        { 
            _delay_ms(100);
            if (!(PIND & (1 << PD2))) 
            {
                taster3_gedrueckt = 1;
                if (sleepMode) {
                    toggleSleepMode();
                } else 
                {
                    hours = (hours + 1) % 24;
                }
            }
        } else if (!pd2_gedrueckt && taster3_gedrueckt) 
        {
            taster3_gedrueckt = 0;
        }
    }
}



int main() 
{
    initLEDmitTundPWM();
    initTimer2();
    sei(); 

    while (1) 
    {
        checkButtons();
        CheckSleepMode();
        updateLEDs();
    }

    return 0;
}
