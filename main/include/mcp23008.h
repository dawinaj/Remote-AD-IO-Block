/**
 * @file mcp23008.h
 *
 * ESP-IDF driver for MCP23008 I2C GPIO expander
 *
 * Copyright (c) 2018 Alan K. Duncan
 * MIT license
 */

#ifndef __MCP23008_H__
#define __MCP23008_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>
#include "driver/i2c.h"

/**
 * @brief MCP23008 device structure
 *
 * Structure to define the GPIO expander device
 */

typedef struct {
    i2c_port_t port;    /**< I2C_NUM_0 or I2C_NUM_1 */
    uint8_t address;    /**< Hardware address of the device */
    uint8_t current;    /**< Currently (known) value of the GPIO port */
} mcp23008_t;

esp_err_t mcp23008_read_reg(mcp23008_t *mcp, uint8_t reg, uint8_t *d);
esp_err_t mcp23008_write_reg(mcp23008_t *mcp, uint8_t reg, uint8_t d);

/**
 * @brief Initialize the MCP23008
 * @param mcp Pointer to the device structure
 * @return 'ESP_OK' on success
*/
esp_err_t mcp23008_init(mcp23008_t *mcp);

/**
 * @brief Reads the GPIO port
 * @param mcp Pointer to the device structure
 * @param d Pointer to a 8 bit integer to hold port value
 * @return 'ESP_OK' on success
*/
esp_err_t mcp23008_read_port(mcp23008_t *mcp, uint8_t *d);

/**
 * @brief Writes to the GPIO port
 * @param mcp Pointer to the device structure
 * @param d New port value
 * @return 'ESP_OK' on success
*/
esp_err_t mcp23008_write_port(mcp23008_t *mcp, uint8_t d);

/**
 * @brief Sets the GPIO port direction
 * A 1 bit is input, 0 is output. 
 * @param mcp Pointer to the device structure
 * @param d New port direction byte
 * @return 'ESP_OK' on success
*/
esp_err_t mcp23008_set_port_direction(mcp23008_t *mcp, uint8_t dirs);

/**
 * @brief Reads the GPIO port direction
 * A 1 bit is input, 0 is output. 
 * @param mcp Pointer to the device structure
 * @param d Pointer to 8 bit integer to hold direction values
 * @return 'ESP_OK' on success
*/
esp_err_t mcp23008_get_port_direction(mcp23008_t *mcp, uint8_t *dirs);

/**
 * @brief Sets the interrupt enable flags
 * A 1 bit is enabled for that GPIO, 0 is disabled. 
 * @param mcp Pointer to the device structure
 * @param d Interrupt enable flags
 * @return 'ESP_OK' on success
*/
esp_err_t mcp23008_set_interrupt_enable(mcp23008_t *mcp, uint8_t intr);

/**
 * @brief Reads the interrupt enable flags
 * A 1 bit is enabled for that GPIO, 0 is disabled. 
 * @param mcp Pointer to the device structure
 * @param d Pointer to 8 bit integer to hold interrupt enable flags
 * @return 'ESP_OK' on success
*/
esp_err_t mcp23008_read_interrupt_enable(mcp23008_t *mcp, uint8_t *intr);

/**
 * @brief Sets the default values
 * Used if interrupts configured to compare to default values 
 * @param mcp Pointer to the device structure
 * @param d Interrupt default compare values
 * @return 'ESP_OK' on success
*/
esp_err_t mcp23008_set_default_value(mcp23008_t *mcp, uint8_t defval);

/**
 * @brief Reads the default values
 * Used if interrupts configured to compare to default values 
 * @param mcp Pointer to the device structure
 * @param d Pointer to 8 bit integer to hold default values
 * @return 'ESP_OK' on success
*/
esp_err_t mcp23008_read_default_value(mcp23008_t *mcp, uint8_t *defval);

/**
 * @brief Sets the interrupt control flags
 * A 1 bit uses default value for comparison, 0 compares against previous value. 
 * @param mcp Pointer to the device structure
 * @param d Interrupt control flags
 * @return 'ESP_OK' on success
*/
esp_err_t mcp23008_set_interrupt_control(mcp23008_t *mcp, uint8_t intr);

/**
 * @brief Reads the interrupt flag register 
 * Flags the GPIO that caused the interrupt 
 * @param mcp Pointer to the device structure
 * @param d Pointer to 8 bit integer to hold interrupt GPIO flags
 * @return 'ESP_OK' on success
*/
esp_err_t mcp23008_read_interrupt_reg(mcp23008_t *mcp, uint8_t *intr);

/**
 * @brief Reads the interrupt capture register 
 * The state of the GPIO port at the time the interrupt fired
 * @param mcp Pointer to the device structure
 * @param d Pointer to 8 bit integer to hold GPIO state at interrupt
 * @return 'ESP_OK' on success
*/
esp_err_t mcp23008_read_interrupt_capture(mcp23008_t *mcp, uint8_t *intr);

/**
 * @brief Sets the output latches
 * @param mcp Pointer to the device structure
 * @param d GPIO output latch flags
 * @return 'ESP_OK' on success
*/
esp_err_t mcp23008_set_output_latch(mcp23008_t *mcp, uint8_t olat);

/**
 * @brief Sets the pull resistor status on the GPIO port
 * @param mcp Pointer to the device structure
 * @param d Pullup resistor flags
 * @return 'ESP_OK' on success
*/
esp_err_t mcp23008_set_pullups(mcp23008_t *mcp, uint8_t pu);

/**
 * @brief Reads the pull resistor flags on the GPIO port
 * @param mcp Pointer to the device structure
 * @param d Pointer to an 8 bit integer to hold pullup flags
 * @return 'ESP_OK' on success
*/
esp_err_t mcp23008_read_pullups(mcp23008_t *mcp, uint8_t *pu);

/**
 * @brief Sets a single bit on the GPIO port
 * @param mcp Pointer to the device structure
 * @param b Index of the bit to set
 * @return 'ESP_OK' on success
*/
esp_err_t mcp23008_port_set_bit(mcp23008_t *mcp, uint8_t b);

/**
 * @brief Clears a single bit on the GPIO port
 * @param mcp Pointer to the device structure
 * @param b Index of the bit to clear
 * @return 'ESP_OK' on success
*/
esp_err_t mcp23008_port_clear_bit(mcp23008_t *mcp, uint8_t b);

/**
 * @brief Toggles a single bit on the GPIO port
 * @param mcp Pointer to the device structure
 * @param b Index of the bit to toggle
 * @return 'ESP_OK' on success
*/
esp_err_t mcp23008_toggle_bit(mcp23008_t *mcp, uint8_t b);

#ifdef __cplusplus
}
#endif

#endif  // __MCP23008_H__