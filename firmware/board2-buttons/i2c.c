#include "i2c.h"

static I2cClientError _i2c_error_state = {0};

void i2c_init(void)
{
    I2C1CON0bits.EN = 0;
    I2C1CON0bits.MODE = 0b000; // four 7-bit addresses
    /* TXU No underflow; CSD Clock Stretching enabled; RXO No overflow; P Cleared by hardware after sending Stop; ACKDT Acknowledge; ACKCNT Acknowledge;  */
    I2C1CON1 = 0x0;
    I2C1CON1bits.CSD = 1;
    /* ABD enabled; GCEN disabled; ACNT disabled; SDAHT 300 ns hold time; BFRET 8 I2C Clock pulses; FME disabled;  */
    I2C1CON2 = 0x0;
    /* CNT 0x0;  */
    I2C1CNTL = 0x00;
    I2C1CNTH = 0x00;
    I2C1ADR0 = 0x22 << 1;
    I2C1ADR1 = 0x22;
    I2C1ADR2 = 0x22 << 1;
    I2C1ADR3 = 0x22;
    /* BAUD 127;  */
    I2C1BAUD = 0x7F;

    LATCbits.LATC3 = 1;
    LATCbits.LATC4 = 1;
    ANSELCbits.ANSELC3 = 0;
    ANSELCbits.ANSELC4 = 0;
    TRISCbits.TRISC3 = 0;
    TRISCbits.TRISC4 = 0;
    
    // Open drain
    ODCONCbits.ODCC3 = 1;
    ODCONCbits.ODCC4 = 1;

    // I2C specific port functions
    RC3I2Cbits.TH = 0b01; //I2c specific
    RC4I2Cbits.TH = 0b01; //I2c specific
    RC3I2Cbits.PU = 0b01; //2x current of standard weak pull-up 
    RC4I2Cbits.PU = 0b01; //2x current of standard weak pull-up 

    INT0PPS = 0x8; //RB0->INTERRUPT MANAGER:INT0;
    I2C1SCLPPS = 0x13;  //RC3->I2C1:SCL1;
    RC3PPS = 0x37;  //RC3->I2C1:SCL1;
    I2C1SDAPPS = 0x14;  //RC4->I2C1:SDA1;
    RC4PPS = 0x38;  //RC4->I2C1:SDA1;
    
    
    /* Enable Interrupts */
    PIE7bits.I2C1IE = 1;
    PIE7bits.I2C1EIE = 1;
    PIE7bits.I2C1RXIE = 1;
    PIE7bits.I2C1TXIE = 1;
    I2C1PIEbits.PCIE = 1;
    I2C1PIEbits.ADRIE = 1;
    I2C1ERRbits.NACKIE = 1;

    I2C1CON0bits.EN = 1;
}

void __interrupt(irq(I2C1TX, I2C1RX, I2C1), base(8)) I2C1_ISR(void){
    _i2c_event_handler();
}

void __interrupt(irq(I2C1E), base(8)) I2C1_ERROR_ISR(void)
{
    _i2c_error_event_handler();
}

void _i2c_event_handler(void){
    if (0U != I2C1PIRbits.ACKTIF)
    {
        I2C1PIRbits.ACKTIF = 0;
    }
    if (0U != I2C1PIRbits.PCIF)
    {
        I2C1PIRbits.PCIF = 0;
        I2C1STAT1bits.CLRBF = 1;
        I2C1CNTL = 0x00;
        I2C1CNTH = 0x00;
        // Clearing the ACKDT bit to ensure clock stretching remains enabled for subsequent write/read requests.
        I2C1CON1bits.ACKDT = 0;
        _i2c_interrupt_handler(I2C_CLIENT_EVENT_STOP_BIT_RECEIVED);
    }
    else if (0U != I2C1PIRbits.ADRIF)
    {
        I2C1PIRbits.ADRIF = 0; /* Clear Address interrupt */
        /* Clear Software Error State */
        _i2c_error_state = I2C_CLIENT_ERROR_NONE;
        /* Notify that a address match event has occurred */
        if (_i2c_interrupt_handler(I2C_CLIENT_EVENT_ADDR_MATCH))
        {
//            I2C1CON1bits.ACKDT = 0; /* Send ACK */
        }
        else
        {
//            I2C1CON1bits.ACKDT = 1; /* Send NACK */
        }
    }
    else
    {
        /* Host reads from client, client transmits */
        if (0U != I2C1STAT0bits.R)
        {
            if ((I2C1STAT1bits.TXBE) && (!I2C1CON1bits.ACKSTAT))
            {
                /* I2C host wants to read. In the callback, client must write to transmit register */
                if (_i2c_interrupt_handler(I2C_CLIENT_EVENT_TX_READY))
                {
//                    I2C1CON1bits.ACKDT = 1; /* Send NACK */
                }
                else
                {
                    I2C1CON1bits.ACKDT = 0; /* Send ACK */
                }
            }
        }
        else
        {
            if (0U != I2C1STAT1bits.RXBF)
            {
                /* I2C host wants to write. In the callback, client must read data by calling I2Cx_ReadByte()  */
                if (_i2c_interrupt_handler(I2C_CLIENT_EVENT_RX_READY))
                {
                    I2C1CON1bits.ACKDT = 0; /* Send ACK */
                }
                else
                {
//                    I2C1CON1bits.ACKDT = 1; /* Send NACK */
                }
                I2C1PIRbits.ACKTIF = 0; /* Clear Acknowledge Status Interrupt Flag */
            }            
        }
    }

    /* Data written by the application; release the clock stretch */
    I2C1CON0bits.CSTR = 0;
}

void _i2c_error_event_handler(void)
{
    if (0U != I2C1ERRbits.BCLIF)
    {
        _i2c_error_state = I2C_CLIENT_ERROR_BUS_COLLISION;
        _i2c_interrupt_handler(I2C_CLIENT_EVENT_ERROR);
        I2C1ERRbits.BCLIF = 0; /* Clear the Bus collision */
    }
    else if (0U != I2C1STAT1bits.TXWE)
    {
        _i2c_error_state = I2C_CLIENT_ERROR_WRITE_COLLISION;
        _i2c_interrupt_handler(I2C_CLIENT_EVENT_ERROR);
        I2C1STAT1bits.TXWE = 0; /* Clear the Write collision */
    }
    else if (0U != I2C1CON1bits.RXO)
    {
        _i2c_error_state = I2C_CLIENT_ERROR_RECEIVE_OVERFLOW;
        _i2c_interrupt_handler(I2C_CLIENT_EVENT_ERROR);
        I2C1CON1bits.RXO = 0; /* Clear the Rx Overflow */
    }
    else if (0U != I2C1CON1bits.TXU)
    {
        _i2c_error_state = I2C_CLIENT_ERROR_TRANSMIT_UNDERFLOW;
        _i2c_interrupt_handler(I2C_CLIENT_EVENT_ERROR);
        I2C1CON1bits.TXU = 0; /* Clear the Transmit underflow*/
    }
    else if (0U != I2C1STAT1bits.RXRE)
    {
        _i2c_error_state = I2C_CLIENT_ERROR_READ_UNDERFLOW;
        _i2c_interrupt_handler(I2C_CLIENT_EVENT_ERROR);
        I2C1STAT1bits.RXRE = 0; /* Clear the Receive underflow */
    }
    else
    {
        /* Since no error flags are set, it can be concluded that the call to the error event handler was 
        triggered by the error flag being set due to the host sending a NACK after all the expected data 
        bytes were received. */
        _i2c_error_state = I2C_CLIENT_ERROR_NONE;
    }
    
    I2C1ERRbits.NACKIF = 0; /* Clear the common NACKIF */
    I2C1CON0bits.CSTR = 0; /* I2C Clock stretch release */
}

extern uint8_t i2c_data;

uint8_t _i2c_interrupt_handler(I2cClientEvent event) {
    switch (event) {
        case I2C_CLIENT_EVENT_ADDR_MATCH:
            return 1;
            break;
            /**< Event indicating that the I2C client is prepared to receive data from the host */
        case I2C_CLIENT_EVENT_RX_READY:
            i2c_data = I2C1RXB; // Data is here
            break;
            /**< Event indicating that the I2C client is ready to transmit data to the host */
        case I2C_CLIENT_EVENT_TX_READY:
            return 0;
            break;
            /**< Event indicating that the I2C client has received a stop bit */
        case I2C_CLIENT_EVENT_STOP_BIT_RECEIVED:
            break;
            /**< Event indicating an error occurred on the I2C bus */
        case I2C_CLIENT_EVENT_ERROR:
            break;
        default:
            break;
    }
    return 1;
}
