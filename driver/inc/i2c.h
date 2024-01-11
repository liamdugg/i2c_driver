#include <linux/init.h>
#include <linux/module.h>
#include <asm/io.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/completion.h>
#include <linux/jiffies.h>
#include <linux/mutex.h>
#include <linux/mod_devicetable.h>
#include <linux/property.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_clk.h>
#include <linux/clk.h>

void i2c_remove(void);
int  i2c_init(struct platform_device *pdev);
int  i2c_write(char slave_address, char* data, char size);
int  i2c_read(char slave_address, char* read_buff, char size);

// I2C2
#define I2C_BASE			0x4819C000
#define I2C_LEN				0x1000

#define I2C_REVNB_LO		0x00
#define I2C_REVNB_HI		0x04
#define I2C_SYSC            0x10
#define I2C_IRQSTAT_RAW		0x24
#define I2C_IRQSTAT			0x28
#define I2C_IRQENA_SET		0x2C
#define I2C_IRQENA_CLR 		0x30
#define I2C_WE				0x34
#define I2C_SYSS			0x90
#define I2C_BUF				0x94
#define I2C_CNT				0x98
#define I2C_DATA			0x9C
#define I2C_CON				0xA4
#define I2C_OA				0xA8
#define I2C_SA				0xAC
#define I2C_PSC				0xB0
#define I2C_SCLL			0xB4
#define I2C_SCLH			0xB8

// CON
#define I2C_CON_START		(1 << 0)
#define I2C_CON_STOP		(1 << 1)
#define I2C_CON_TX			(1 << 9)
#define I2C_CON_RX			(1 << 9)
#define I2C_CON_MASTER		(1 << 10)
#define I2C_CON_ENABLE		(1 << 15)

// SYSC
#define I2C_SYSC_AUTOIDLE	(1 << 0)
#define I2C_SYSC_RESET		(1 << 1)
#define I2C_SYSC_WAKEUP		(1 << 2)
#define I2C_SYSC_NOIDLE		(1 << 3)
#define I2C_SYSC_CLKACT		(3 << 8)

// IRQSTATUS
#define I2C_IRQ_AL			(1 << 0)
#define I2C_IRQ_NACK		(1 << 1)
#define I2C_IRQ_ARDY		(1 << 2)
#define I2C_IRQ_RRDY		(1 << 3)
#define I2C_IRQ_XRDY		(1 << 4)
#define I2C_IRQ_BB			(1 << 12)

#define I2C_IRQSTAT_CLR_ALL	0x00006FFF

#define I2C_IRQENA_CLR_MASK	0x00006FFF
#define I2C_IRQENA_CLR_ACK	0x00000004
#define I2C_IRQENA_CLR_RX	0x00000008
#define I2C_IRQENA_CLR_TX	0x00000010

// PSC
#define I2C_PSC_MASK		0x000000FF
#define I2C_PSC_12MHZ		0x00000003 // div por 3, 48MHz/4 = 12MHz 
#define I2C_PSC_24MHZ		0x00000001 // div por 1, 48MHz/1 = 24MHz

// SCLL
#define I2C_SCLL_MASK		0x000000FF
#define I2C_SCLL_400K		0x00000017 // tLOW = 1,25 uS = (23+7)*(2/48MHz)
#define I2C_SCLL_100K		0x000000E9 // tLOW = 5,00 uS = (233+7)+(1/48MHz)		

// SCLH
#define I2C_SCLH_MASK		0x000000FF
#define I2C_SCLH_400K		0x00000019 // THIGH = 1,25 uS = (25+5)*(1/48MHz)
#define I2C_SCLH_100K		0x000000EB // THIGH = 5,00 uS = (235+5)*(1/48MHz)

// clock module peripheral register
#define CM_PER_BASE			0x44E00000
#define CM_PER_LEN			0x400

#define CM_PER_CLKCTRL		0x44
#define CM_PER_CLKCTRL_ENA	0x2
#define CM_PER_CLKCTRL_MASK	0x00030003

// control module
#define CTRL_MOD_BASE		0x44E10000
#define CTRL_MOD_LEN		0x2000 

#define CTRL_MOD_SCL		0x97C
#define CTRL_MOD_SDA		0x978

#define CTRL_MOD_SCL_MODE	0x33
#define CTRL_MOD_SDA_MODE	0x33

#define CTRL_MOD_SCL_MSK	0xFFFFFFC0
#define CTRL_MOD_SDA_MSK	0xFFFFFFC0