/****************************************************************************
 * aura_hw/lsm6ds3.c
 ****************************************************************************/

#include <nuttx/config.h>

#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

#include <nuttx/i2c/i2c_master.h>

#include "util.h"
#include "lsm6ds3.h"

/* ---- 寄存器表（LSM6DS3TR-C 数据手册） ---- */

#define REG_WHO_AM_I      0x0f
#define WHO_AM_I_VAL      0x6a
#define REG_CTRL1_XL      0x10
#define REG_CTRL2_G       0x11
#define REG_CTRL3_C       0x12
#define REG_OUTX_L_G      0x22  /* 0x22~0x2D 连续 12 字节：陀螺仪 XYZ + 加速度 XYZ */

/* ODR 编码（CTRL1_XL / CTRL2_G 的 bit7~4） */
#define ODR_104HZ         (0x04 << 4)
#define ODR_208HZ         (0x05 << 4)

/* 满量程：陀螺仪 ±2000 dps（FS_G=11），加速度 ±2 g（FS_XL=00） */
#define FS_G_2000DPS      (0x03 << 2)

#define GYRO_DPS_PER_LSB  0.070f      /* ±2000 dps → 70 mdps/LSB */
#define ACCEL_G_PER_LSB   0.000061f   /* ±2 g     → 0.061 mg/LSB */

#define CTRL3_BDU_IFINC   0x44        /* BDU=1（数据成对更新）+ IF_INC=1（突发读） */
#define CTRL3_SW_RESET    0x01

/* LSM6DS3TR-C 支持 I2C 快速模式 400kHz；
 * 本分支 i2c_msg_s 带 frequency 字段（旧版布局），需显式赋值
 */
#define I2C_BUS_FREQ_HZ  400000

static int i2c_write_reg(lsm6ds3_dev_t *dev, uint8_t reg, uint8_t value)
{
  uint8_t buf[2];
  struct i2c_msg_s msg;
  struct i2c_transfer_s xfer;

  buf[0] = reg;
  buf[1] = value;

  memset(&msg, 0, sizeof(msg));
  msg.frequency = I2C_BUS_FREQ_HZ;
  msg.addr      = dev->addr;
  msg.flags     = 0;
  msg.length    = 2;
  msg.buffer    = buf;

  xfer.msgv = &msg;
  xfer.msgc = 1;

  return ioctl(dev->fd, I2CIOC_TRANSFER, (unsigned long)(uintptr_t)&xfer);
}

static int i2c_read_regs(lsm6ds3_dev_t *dev, uint8_t reg,
                         uint8_t *buf, uint16_t len)
{
  struct i2c_msg_s msg[2];
  struct i2c_transfer_s xfer;

  memset(msg, 0, sizeof(msg));

  msg[0].frequency = I2C_BUS_FREQ_HZ;   /* 第一段：写寄存器起始地址 */
  msg[0].addr      = dev->addr;
  msg[0].flags     = 0;
  msg[0].length    = 1;
  msg[0].buffer    = &reg;

  msg[1].frequency = I2C_BUS_FREQ_HZ;   /* 第二段：重复起始 + 读 */
  msg[1].addr      = dev->addr;
  msg[1].flags     = I2C_M_READ;
  msg[1].length    = len;
  msg[1].buffer    = buf;

  xfer.msgv = msg;
  xfer.msgc = 2;

  return ioctl(dev->fd, I2CIOC_TRANSFER, (unsigned long)(uintptr_t)&xfer);
}

int lsm6ds3_init(lsm6ds3_dev_t *dev, const char *i2c_path, uint8_t addr)
{
  uint8_t whoami = 0;

  memset(dev, 0, sizeof(*dev));
  dev->addr = addr;
  dev->fd   = open(i2c_path, O_RDWR);
  if (dev->fd < 0)
    {
      printf("[lsm6ds3] open %s failed\n", i2c_path);
      return -1;
    }

  /* 软复位，等待数据手册要求的 50ms */
  if (i2c_write_reg(dev, REG_CTRL3_C, CTRL3_BDU_IFINC | CTRL3_SW_RESET) < 0)
    {
      goto errout;
    }

  usleep(50000);

  if (i2c_read_regs(dev, REG_WHO_AM_I, &whoami, 1) < 0 ||
      whoami != WHO_AM_I_VAL)
    {
      printf("[lsm6ds3] WHO_AM_I=0x%02x, expect 0x6a (检查总线号/地址)\n",
             whoami);
      goto errout;
    }

  if (lsm6ds3_set_odr_200hz(dev) < 0)
    {
      goto errout;
    }

  dev->ready = true;
  printf("[lsm6ds3] ready on %s @0x%02x, ODR=208Hz\n", i2c_path, addr);
  return 0;

errout:
  close(dev->fd);
  dev->fd = -1;
  return -1;
}

int lsm6ds3_set_odr_200hz(lsm6ds3_dev_t *dev)
{
  if (i2c_write_reg(dev, REG_CTRL1_XL, ODR_208HZ) < 0)               /* ±2g */
    {
      return -1;
    }

  if (i2c_write_reg(dev, REG_CTRL2_G, ODR_208HZ | FS_G_2000DPS) < 0)
    {
      return -1;
    }

  return 0;
}

int lsm6ds3_set_odr_100hz(lsm6ds3_dev_t *dev)
{
  if (i2c_write_reg(dev, REG_CTRL1_XL, ODR_104HZ) < 0)
    {
      return -1;
    }

  if (i2c_write_reg(dev, REG_CTRL2_G, ODR_104HZ | FS_G_2000DPS) < 0)
    {
      return -1;
    }

  return 0;
}

int lsm6ds3_read_frame(lsm6ds3_dev_t *dev, imu_raw_frame_t *frame)
{
  uint8_t raw[12];
  int16_t v;

  /* BDU=1 保证突发读 12 字节内陀螺仪/加速度数据不自更新 */
  if (i2c_read_regs(dev, REG_OUTX_L_G, raw, sizeof(raw)) < 0)
    {
      return -1;
    }

  v = (int16_t)((uint16_t)raw[1] << 8 | raw[0]);
  frame->gyro_dps[0] = v * GYRO_DPS_PER_LSB;
  v = (int16_t)((uint16_t)raw[3] << 8 | raw[2]);
  frame->gyro_dps[1] = v * GYRO_DPS_PER_LSB;
  v = (int16_t)((uint16_t)raw[5] << 8 | raw[4]);
  frame->gyro_dps[2] = v * GYRO_DPS_PER_LSB;

  v = (int16_t)((uint16_t)raw[7] << 8 | raw[6]);
  frame->accel_g[0] = v * ACCEL_G_PER_LSB;
  v = (int16_t)((uint16_t)raw[9] << 8 | raw[8]);
  frame->accel_g[1] = v * ACCEL_G_PER_LSB;
  v = (int16_t)((uint16_t)raw[11] << 8 | raw[10]);
  frame->accel_g[2] = v * ACCEL_G_PER_LSB;

  frame->timestamp_ms = aura_millis();
  return 0;
}

void lsm6ds3_deinit(lsm6ds3_dev_t *dev)
{
  if (dev->fd >= 0)
    {
      close(dev->fd);
      dev->fd    = -1;
      dev->ready = false;
    }
}
