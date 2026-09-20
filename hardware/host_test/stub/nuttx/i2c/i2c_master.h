#ifndef _STUB_I2C_H
#define _STUB_I2C_H
#include <stdint.h>
#include <sys/types.h>
struct i2c_msg_s { uint32_t frequency; uint16_t addr; uint16_t flags; uint8_t *buffer; ssize_t length; };
struct i2c_transfer_s { struct i2c_msg_s *msgv; size_t msgc; };
#define I2C_M_READ   0x0001
#define I2C_SLAVE    0x0703
#define I2CIOC_TRANSFER 0x0704
#endif
