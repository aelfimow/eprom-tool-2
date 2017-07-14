#include "UsartDriver.h"
#include "UsartDriver_par.h"

static void UsartWriteData(INT8U data);

void UsartInit(void)
{
    UBRRH = (INT8U)((USART_DRIVER_BAUDRATE / 256u) & 0xFFu);
    UBRRL = (INT8U)(USART_DRIVER_BAUDRATE & 0xFFu);
    UCSRB = (INT8U)((INT8U)(1u << TXEN) | (INT8U)(1u << RXEN));
    UCSRC = (INT8U)((INT8U)(1u << URSEL) | (INT8U)(3u << UCSZ0));
    return;
}

void UsartTx(INT8U data)
{
    UsartWriteData(data);
    return;
}

void UsartTxStr(const char *data)
{
    INT8U i = 0u;
    const INT8U max = 64u;
    INT8U c = 0;

    for (i = 0u; i < max; ++i)
    {
        c = pgm_read_byte(&data[i]);
        if (0u != c)
        {
            UsartWriteData(c);
        }
        else
        {
            i = max;
        }
    }

    return;
}

static void UsartWriteData(INT8U data)
{
    while (0u == (UCSRA & (INT8U)(1u << UDRE)))
    {
        /* do nothing */
    }

    UDR = data;

    return;
}

INT8U UsartRx(void)
{
    BOOLEAN done = FALSE;

    while (FALSE == done)
    {
        if (0 == (UCSRA & (INT8U)(1u << RXC)))
        {
            done = FALSE;
        }
        else
        {
            done = TRUE;
        }
    }

    return UDR;
}

BOOLEAN UsartRxDataAvail(void)
{
    BOOLEAN avail = FALSE;

    if (0 == (UCSRA & (INT8U)(1u << RXC)))
    {
        avail = FALSE;
    }
    else
    {
        avail = TRUE;
    }

    return avail;
}

INT8U UsartRead(INT8U *data)
{
    INT8U cnt = 0;

    if (NULL == data)
    {
        cnt = 0;
    }
    else
    {
        data[0] = UDR;
        cnt = 1u;
    }

    return cnt;
}

