#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

// Define the PWM duty cycle (0-255)
#define PWM_BRIGHTNESS 64

// Time variables
volatile uint8_t seconds = 0;
volatile uint8_t minutes = 0;
volatile uint8_t hours = 0;
volatile uint8_t eco_mode = 0;

// Initialize Timer0 for PWM output
void pwm_init() {
    // Set PB1 and PB2 as outputs (PWM anodes)
    DDRB |= (1 << PB1) | (1 << PB2);
    
    // Configure Timer0 for Fast PWM mode
    TCCR0A |= (1 << COM0A1) | (1 << COM0B1) | (1 << WGM01) | (1 << WGM00); // Fast PWM for PB1 (OC0A)
    TCCR0B |= (1 << CS01);  // Prescaler 8
    
    // Set PWM brightness
    OCR0A = PWM_BRIGHTNESS;
}

// Initialize Timer1 for the real-time clock
void timer_init() {
    // Set Timer1 to CTC mode
    TCCR1B |= (1 << WGM12);
    
    // Set prescaler to 256
    TCCR1B |= (1 << CS12);  // CS12=1, CS11=0, CS10=0 gives prescaler of 256
    
    // Set compare match value for 1 second interrupt (assuming 32768Hz crystal)
    // 32768 / 256 = 128 ticks per second
    OCR1A = 128 - 1;
    
    // Enable Timer1 compare match interrupt
    TIMSK1 |= (1 << OCIE1A);
    
    // Enable global interrupts
    sei();
}

// Timer1 interrupt service routine
ISR(TIMER1_COMPA_vect) {
    seconds++;
    
    if (seconds >= 60) {
        seconds = 0;
        minutes++;
        
        if (minutes >= 60) {
            minutes = 0;
            hours++;
            
            if (hours >= 24) {
                hours = 0;
            }
        }
    }
    
    // Update the binary clock display
    update_display();
}

// Update the LED display
void update_display() {
    // For hours, we use PD0, PD1, PD5, PD6, PD7
    uint8_t hours_display = 0;
    
    // Convert hours (0-23) to binary pattern for our specific pins
    if (hours & 0x01) hours_display |= (1 << PD0);  // Bit 0
    if (hours & 0x02) hours_display |= (1 << PD1);  // Bit 1
    if (hours & 0x04) hours_display |= (1 << PD5);  // Bit 2
    if (hours & 0x08) hours_display |= (1 << PD6);  // Bit 3
    if (hours & 0x10) hours_display |= (1 << PD7);  // Bit 4
    
    // For minutes, we use PC0-PC5
    uint8_t minutes_display = 0;
    
    // Convert minutes (0-59) to binary pattern
    for (uint8_t i = 0; i < 6; i++) {
        if (minutes & (1 << i)) {
            minutes_display |= (1 << i);
        }
    }
    
    // Since we're using common anode, we need to invert the logic
    // (0 turns LED on, 1 turns LED off for the cathode pins)
    PORTD = (PORTD & ~((1 << PD0) | (1 << PD1) | (1 << PD5) | (1 << PD6) | (1 << PD7))) | 
            (~hours_display & ((1 << PD0) | (1 << PD1) | (1 << PD5) | (1 << PD6) | (1 << PD7)));
    
    PORTC = (PORTC & ~0x3F) | (~minutes_display & 0x3F);
}

// Configure the crystal oscillator
void crystal_init() {
    // Set the clock prescaler to 1
    CLKPR = (1 << CLKPCE);
    CLKPR = 0;
    
    // Configure for external crystal
    // This assumes jumpers are set correctly for external crystal
    // No need to configure PB6/PB7 as they're automatically used for the crystal
}

// Setup pins as outputs
void pin_init() {
    // Set PD0, PD1, PD5, PD6, PD7 as outputs for hours (cathodes)
    DDRD |= (1 << PD0) | (1 << PD1) | (1 << PD5) | (1 << PD6) | (1 << PD7);
    
    // Set PC0-PC5 as outputs for minutes (cathodes)
    DDRC |= (1 << PC0) | (1 << PC1) | (1 << PC2) | (1 << PC3) | (1 << PC4) | (1 << PC5);
    
    // Set PB1 and PB2 as outputs for PWM anodes
    DDRB |= (1 << PB1) | (1 << PB2);
}

// Multiplexing function to alternate between hours and minutes display
void multiplex() {
    // Turn on hours display (PB1 high, PB2 low)
    PORTB |= (1 << PB1);
    PORTB &= ~(1 << PB2);
    _delay_ms(5);
    
    // Turn on minutes display (PB1 low, PB2 high)
    PORTB &= ~(1 << PB1);
    PORTB |= (1 << PB2);
    _delay_ms(5);
}

int main() {
    // Initialize pins
    pin_init();
    
    // Initialize crystal oscillator
    crystal_init();
    
    // Initialize PWM
    pwm_init();
    
    // Initialize timer for clock
    timer_init();
    
    // Main loop
    while (1) {
        // Multiplex between hours and minutes display
        multiplex();
    }
    
    return 0;
}