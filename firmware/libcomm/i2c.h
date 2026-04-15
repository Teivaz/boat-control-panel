/*
 * File:   i2c.h
 * Author: Teivaz
 *
 * Created on March 25, 2026, 10:39 PM
 */

#ifndef I2C_H
#define I2C_H

#include <xc.h>

typedef enum
{
    I2C_CLIENT_ERROR_NONE = 0, /**< Indicates no error */
    I2C_CLIENT_ERROR_BUS_COLLISION, /**< Indicates an error caused due to bus collision */
    I2C_CLIENT_ERROR_WRITE_COLLISION, /**< Indicates an error caused due to write collision */
    I2C_CLIENT_ERROR_RECEIVE_OVERFLOW, /**< Indicates an error due to a receive buffer overflow */
    I2C_CLIENT_ERROR_TRANSMIT_UNDERFLOW, /**< Indicates an error caused due to transmit buffer underflow */
    I2C_CLIENT_ERROR_READ_UNDERFLOW, /**< Indicates an error caused due to receive buffer underflow */
} I2cClientError;

typedef enum
{
    I2C_CLIENT_EVENT_NONE = 0,          /**< Event indicating that the I2C bus is in an idle state */
    I2C_CLIENT_EVENT_ADDR_MATCH,        /**< Event indicating that the I2C client has received a matching address */
    I2C_CLIENT_EVENT_RX_READY ,         /**< Event indicating that the I2C client is prepared to receive data from the host */
    I2C_CLIENT_EVENT_TX_READY,          /**< Event indicating that the I2C client is ready to transmit data to the host */
    I2C_CLIENT_EVENT_STOP_BIT_RECEIVED, /**< Event indicating that the I2C client has received a stop bit */
    I2C_CLIENT_EVENT_ERROR,             /**< Event indicating an error occurred on the I2C bus */
} I2cClientEvent;

void i2c_init(void);

uint8_t i2c_write_bytes(uint16_t address, uint8_t* data, size_t data_length);

void _i2c_event_handler(void);
void _i2c_error_event_handler(void);
uint8_t _i2c_interrupt_handler(I2cClientEvent clientEvent);

#endif /* I2C_H */
