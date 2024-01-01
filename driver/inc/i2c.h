#include <asm-generic/errno.h>
#include <asm-generic/errno-base.h>

#include <linux/device.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kdev_t.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/delay.h>

/* --------------- FUNCTION PROTOTYPES --------------- */

int i2c_init (struct platform_device* pdev);
int i2c_read (uint8_t slv_addr, uint8_t* reg_addr, uint8_t* store_buf, uint8_t size);
int i2c_write(uint8_t slv_addr, uint8_t* data_to_copy, uint8_t size);
void i2c_remove(void);

/* --------------- DEFINES --------------- */

#define SYSCLK		48000000
#define MODCLK		12000000
#define OUTCLK		100000
#define BUF_SIZE	2

//#define 

// I2C 
#define I2C_REG_BASE 0x4819C000
#define I2C_REG_SIZE 0x1000

// I2C OFFSET FROM BASE
#define I2C_REVNB_LO	0x00
#define I2C_REVNB_HI	0x04
#define I2C_SYSC		0x10
#define I2C_IRQSTAT_RAW	0x24
#define I2C_IRQSTAT		0x28
#define I2C_IRQENA_SET	0x2C
#define I2C_IRQENA_CLR	0x30
// other registers...
#define I2C_SYSS		0x90
#define I2C_BUF			0x94
#define I2C_CNT			0x98
#define I2C_DATA		0x9C
#define I2C_CON			0xA4
#define I2C_OA			0xA8
#define I2C_SA			0xAC
#define I2C_PSC			0xB0
#define I2C_SCLL		0xB4
#define I2C_SCLH		0xB8

// I2C SYSC
#define I2C_SYSC_SRST	0x00000002

// I2C CON
#define I2C_CON_START	(1 << 0)
#define I2C_CON_STOP	(1 << 1)
#define I2C_CON_TMOD	(1 << 9)
#define I2C_CON_MST		(1 << 10)
#define I2C_CON_ENA		(1 << 15)

// I2C IRQ
#define I2C_IRQ_AL		(1 << 0)
#define I2C_IRQ_NACK	(1 << 1)
#define I2C_IRQ_ARDY	(1 << 2)
#define I2C_IRQ_RRDY	(1 << 3)
#define I2C_IRQ_XRDY	(1 << 4)
#define I2C_IRQ_BBUSY	(1 << 12)

// I2C IRQ STATUS
#define I2C_IRQSTAT_CLR_ALL	0x00006FFF

// I2C IRQ ENABLE CLEAR
#define I2C_IRQENA_CLR_MSK	0x00006FFF
#define I2C_IRQENA_CLR_ACK	0X00000004
#define I2C_IRQENA_CLR_RX	0X00000008
#define I2C_IRQENA_CLR_TX	0X00000010

// CLOCK MODULE PERIPHERAL
#define CM_PER_BASE			0x44E00000	// clock module peripheral
#define CM_PER_I2C2_CLKCTRL	0x44		// i2c2 clock control offset

#define CM_PER_I2C2_CLKCTRL_MSK 0xFFFFFFFC
#define CM_PER_I2C2_MODMODE_MSK	0x2
#define CM_PER_I2C2_MODMODE_EN	0x2

// CONTROL MODULE
#define CTRL_MOD_BASE			0x44E10000

#define CTRL_MOD_SCL			0x97C
#define CTRL_MOD_SDA			0x978

#define CTRL_MOD_SCL_MODE		0X33
#define CTRL_MOD_SDA_MODE		0x33

#define CTRL_MOD_SCL_MSK		0xFFFFFFC0
#define CTRL_MOD_SDA_MSK		0xFFFFFFC0


int i2c_init(struct platform_device* pdev);