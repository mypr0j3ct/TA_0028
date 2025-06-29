#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

ISR(INT0_vect)
{
    PORTB |= (1<<7);
    _delay_ms(200);
    PORTB &= ~(1<<7);
}

int main (void)
{
    DDRA=0xff;
    DDRB |= (1<<PB7); // LED output enable
    PORTD |= _BV(PD0); // pin int0 PD0 pullup
    EIMSK |= (1<<INT0); // enable interrupt
    EICRA |= _BV(ISC01); // falling edge activate INT0
    sei();

    while (1)
    {
        PORTA=0x00;
        for (unsigned char i=0;i<8;i++)
        {
            PORTA = ~(1<<i);
            _delay_ms(500);
        }
    }
}
