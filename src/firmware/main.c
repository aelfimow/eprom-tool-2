#include <avr/io.h>
#include <util/delay.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include "data_types.h"
#include "UsartDriver_ifc.h"

static void InitIO(void);
static INT8U ReceiveByte(void);
static void Wait_100us(void);
static void ExecuteCommand(const INT8U Command);

static void ShiftRegSet(const INT8U Data);
static void SetAddrData(const INT16U Addr, const INT16U Data);
static INT16U ReadAddr(const INT16U Addr);

int main(void)
{
    INT8U a = 0u;
    INT8U b = 0u;

    /* Initialization */
    UsartInit();
    InitIO();

    /* Disable interrupts */
    cli();

    while (TRUE)
    {
        a = ReceiveByte();
        b = ReceiveByte();

        while (a != b)
        {
            a = b;
            b = ReceiveByte();
        }

        /* Some command byte received */
        ExecuteCommand(a);
    }

    return 0;
}

static void ExecuteCommand(const INT8U Command)
{
    const INT8U pgm_mask = _BV(PE0);
    INT32U i = 0UL;
    INT16U Addr = 0u;
    INT16U Data16Bit = 0u;
    INT8U DataLow = 0u;
    INT8U DataHigh = 0u;

    switch (Command)
    {
        case 0xAAu:
            {
                /* Read operation */

                Addr = 0u;  /* Start address */

                /* This loop goes through all word addresses, reads data and */
                /* transmits data over UART */
                for (i = 0UL; i < 65536UL; ++i)
                {
                    Data16Bit = ReadAddr(Addr);

                    DataLow = (INT8U)(Data16Bit & 0x00FFu);
                    DataHigh = (INT8U)(Data16Bit / 256u);

                    UsartTx(DataLow);
                    UsartTx(DataHigh);

                    /* Next address */
                    ++Addr;
                }

                break;
            }

        case 0xBBu:
            {
                /* Write operation */

                Addr = 0u;  /* Start address */

                /* This loop goes through all word addresses, receives data over UART and */
                /* and programs it to the EPROM */
                for (i = 0UL; i < 65536UL; ++i)
                {
                    DataLow = ReceiveByte();
                    DataHigh = ReceiveByte();

                    Data16Bit = ((((INT16U)DataHigh) * 256u) + (((INT16U)DataLow) & 0x00FFu));

                    SetAddrData(Addr, Data16Bit);

                    /* Generate PGM pulse */
                    {
                        /* Set PGM pin to LOW */
                        PORTE &= (INT8U)(~pgm_mask);
                        Wait_100us();
                        /* Set PGM pin to HIGH */
                        PORTE |= (INT8U)(pgm_mask);
                    }

                    /* Next address */
                    ++Addr;
                }

                break;
            }

        default:
            {
                /* Unknown command, do nothing */
                break;
            }
    }
}

static INT8U ReceiveByte(void)
{
    BOOLEAN done = FALSE;
    INT8U DataByte = 0u;

    /* This loop runs as long as one received */
    while (FALSE == done)
    {
        /* This loop waits for one UART data byte */
        while (FALSE == UsartRxDataAvail())
        {
            /* Do nothing, just wait for UART data byte */
        }

        done = (0u != UsartRead(&DataByte));
    }

    return DataByte;
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
    PORTE |= (INT8U)(_BV(PE0));
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

static void SetAddrData(const INT16U Addr, const INT16U Data)
{
    ShiftRegSet(Data / 256u);       /* data bus byte [15...8] */
    ShiftRegSet(Data & 0x00FFu);    /* data bus byte [7...0] */
    ShiftRegSet(Addr & 0x00FFu);    /* address bus byte [7...0] */
    ShiftRegSet(Addr / 256u);       /* address bus byte [15...8] */
}

static INT16U ReadAddr(const INT16U Addr)
{
    const INT8U MaskOE = _BV(PD7);
    const INT8U MaskCP = _BV(PE2);
    const INT8U MaskData0 = _BV(PD6);
    const INT8U MaskData1 = _BV(PD5);
    INT8U DataHigh = 0u;
    INT8U DataLow = 0u;
    INT16U Result = 0u;

    SetAddrData(Addr, 0u);

    /* Ensure, both latches outputs are disabled */
    PORTD |= (INT8U)(MaskData1);
    PORTD |= (INT8U)(MaskData0);

    /* Set nOE of EPROM LOW */
    PORTD &= (INT8U)(~MaskOE);

    /* Generate clock pulse for data latches */
    PORTE &= (INT8U)(~MaskCP);
    PORTE |= (INT8U)(MaskCP);
    Wait_100us();
    PORTE &= (INT8U)(~MaskCP);

    /* Set nOE of EPROM HIGH */
    PORTD |= (INT8U)(MaskOE);

    /* Get high byte */
    PORTD &= (INT8U)(~MaskData1);
    Wait_100us();
    DataHigh = PINC;
    PORTD |= (INT8U)(MaskData1);

    /* Get low byte */
    PORTD &= (INT8U)(~MaskData0);
    Wait_100us();
    DataLow = PINC;
    PORTD |= (INT8U)(MaskData0);

    Result = ((((INT16U)DataHigh) * 256u) + (((INT16U)DataLow) & 0x00FFu));

    return Result;
}

