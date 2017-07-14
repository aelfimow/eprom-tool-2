#include <avr/io.h>
#include <util/delay.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include "data_types.h"
#include "UsartDriver_ifc.h"

#define MAX_DATA        (5u)

#define PACKET_START_BYTE   ('A')
#define PACKET_END_BYTE     ('E')

static void InitIO(void);
static void Wait_100us(void);
static void ScanHex(const INT8U Data, INT8U * const pOut);
static void ShiftRegSet(const INT8U Data);

int main(void)
{
    const INT8U MaskCP = _BV(PE2);
    const INT8U MaskData0 = _BV(PD6);
    const INT8U MaskData1 = _BV(PD5);
    INT8U Answer[4u] = { 0u };
    volatile INT8U Data = 0u;
    INT16U i = 0u;
    INT8U DataA = 0u;
    INT8U DataB = 0u;

    UsartInit();
    InitIO();

    cli();

    ShiftRegSet(0u); /* data bus byte [15...8] */
    ShiftRegSet(0u); /* data bus byte [7...0] */
    ShiftRegSet(0u); /* address bus byte [7...0] */
    ShiftRegSet(0u); /* address bus byte [15...8] */

    while (TRUE)
    {
        /* Generate clock impulse for data latches */
        PORTE &= (INT8U)(~MaskCP);
        PORTE |= (INT8U)(MaskCP);
        Wait_100us();
        PORTE &= (INT8U)(~MaskCP);

        /* Ensure, both latches outputs are disabled */
        PORTD |= (INT8U)(MaskData1);
        PORTD |= (INT8U)(MaskData0);

        /* Get high byte */
        PORTD &= (INT8U)(~MaskData1);
        Wait_100us();
        Data = PINC;
        ScanHex(Data, &Answer[0u]);
        PORTD |= (INT8U)(MaskData1);

        /* Get low byte */
        PORTD &= (INT8U)(~MaskData0);
        Wait_100us();
        Data = PINC;
        ScanHex(Data, &Answer[2u]);
        PORTD |= (INT8U)(MaskData0);

        /* Send answer */
        UsartTx(Answer[0]);
        UsartTx(Answer[1]);
        UsartTx(Answer[2]);
        UsartTx(Answer[3]);
        UsartTx(10);
        UsartTx(13);

        /* Generate a pause */
        for (i = 0u; i < 10000u; ++i)
        {
            Wait_100us();
        }

        ShiftRegSet(DataB); /* data bus byte [15...8] */
        ShiftRegSet(DataA); /* data bus byte [7...0] */
        ShiftRegSet(0u); /* address bus byte [7...0] */
        ShiftRegSet(0u); /* address bus byte [15...8] */
        ++DataA;
        --DataB;
    }

    return 0;
}

static void InitIO(void)
{
    /* port A as output */
    DDRA |= (INT8U)(_BV(PA0));
    DDRA |= (INT8U)(_BV(PA1));
    DDRA |= (INT8U)(_BV(PA2));
    DDRA |= (INT8U)(_BV(PA3));
    DDRA |= (INT8U)(_BV(PA4));
    DDRA |= (INT8U)(_BV(PA5));
    DDRA |= (INT8U)(_BV(PA6));
    DDRA |= (INT8U)(_BV(PA7));

    /* some pins of port D as output */
    DDRD |= (INT8U)(_BV(PD7));
    DDRD |= (INT8U)(_BV(PD6));
    DDRD |= (INT8U)(_BV(PD5));

    /* port C as input */
    DDRC = 0u;

    /* port E as output */
    DDRE |= (INT8U)(_BV(PE0));
    DDRE |= (INT8U)(_BV(PE1));
    DDRE |= (INT8U)(_BV(PE2));

    /* Set some default pin values */
    PORTA = 0u;
    PORTE = (INT8U)(_BV(PE0));
    PORTD |= (INT8U)(_BV(PD5) | _BV(PD6) | _BV(PD7));

    return;
}

static void Wait_100us(void)
{
    INT8U i = 0u;

    /* This loop generates 100 us delay. */
    /* System clock of 9.6 MHz assumed... */
    for (i = 0u; i < 72u; ++i)
    {
        asm volatile (
                "nop\n\t"
                "nop\n\t"
                "nop\n\t"
                "nop\n\t"
                "nop\n\t"
                "nop\n\t"
                "nop\n\t"
                "nop\n\t"
                "nop\n\t"
                "nop\n\t"
                ::);
    }
}

static void ScanHex(const INT8U Data, INT8U * const pOut)
{
    INT8U Nibble = 0u;

    /* High nibble */
    Nibble = ((Data / 16u) & 0x0Fu);

    if (Nibble < 10)
    {
        pOut[0u] = ('0' + Nibble);
    }
    else
    {
        pOut[0u] = ('A' + (Nibble - 10u));
    }

    /* Low nibble */
    Nibble = (Data & 0x0Fu);

    if (Nibble < 10)
    {
        pOut[1u] = ('0' + Nibble);
    }
    else
    {
        pOut[1u] = ('A' + (Nibble - 10u));
    }
}

static void ShiftRegSet(const INT8U Data)
{
    const INT8U mask = _BV(PE1);

    /* Output data */
    PORTA = Data;

    /* Enable data parts of the shift register */
    PORTD |= (INT8U)(_BV(PD7));

    /* Generate one positive clock */
    PORTE &= (INT8U)(~mask);    /* clock pin LOW */
    PORTE |= (INT8U)(mask);     /* clock pin HIGH */
    PORTE &= (INT8U)(~mask);    /* clock pin LOW */
}

