/* ---------- INCLUDES ----------*/

#include <asm-generic/errno.h>
#include <asm-generic/errno-base.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/ioctl.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kdev_t.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>
#include <linux/version.h>

int  	bmp_init(void);
void 	bmp_measure(void);
int16_t bmp_get_temp(void);
int32_t bmp_get_pres(void);

/* ---------- DEFINES ----------*/

#define NUM_OF_DEVICES	1
#define LSB_INDEX 	0
#define MSB_INDEX 	1
#define REG_SIZE	1

#define TEMP_BIT_1 0
#define TEMP_BIT_0 1

#define PRES_BIT_3 2
#define PRES_BIT_2 3
#define PRES_BIT_1 4
#define PRES_BIT_0 5

#define GET_REG_VALUE(msb,lsb) ((uint16_t)(((msb << 8) | lsb)))

/* ---------- TYPEDEFS ----------*/

typedef struct {
	int16_t  AC1;
	int16_t  AC2;
	int16_t  AC3;
	uint16_t AC4;
	uint16_t AC5;
	uint16_t AC6;	
	int16_t  B1;
	int16_t  B2;
	int16_t  MB;
	int16_t  MC;
	int16_t  MD;

	int32_t  B5; // este se calcula, no se lee
} bmp_calib_t;

typedef struct {

	uint8_t chip_id;
	uint8_t slv_addr;
	uint8_t mode;
	
	bmp_calib_t calib;

	int16_t temp;
	int32_t pres;
} bmp_t;

/* --------------- BMP --------------- */
/* https://cdn-shop.adafruit.com/datasheets/BST-BMP180-DS000-09.pdf */

#define SLAVE_ADDR	0x77

// CALIBRATION REGISTERS (read-only)
#define REG_MSB_AC1 	0xAA
#define REG_LSB_AC1 	0xAB
#define REG_MSB_AC2 	0xAC
#define REG_LSB_AC2 	0xAD
#define REG_MSB_AC3 	0xAE
#define REG_LSB_AC3 	0xAF
#define REG_MSB_AC4 	0xB0
#define REG_LSB_AC4 	0xB1
#define REG_MSB_AC5 	0xB2
#define REG_LSB_AC5 	0xB3
#define REG_MSB_AC6 	0xB4
#define REG_LSB_AC6 	0xB5

#define REG_MSB_B1 		0xB6
#define REG_LSB_B1 		0xB7
#define REG_MSB_B2 		0xB8
#define REG_LSB_B2 		0xB9

#define REG_MSB_MB 		0xBA
#define REG_LSB_MB 		0xBB
#define REG_MSB_MC 		0xBC
#define REG_LSB_MC 		0xBD
#define REG_MSB_MD 		0xBE
#define REG_LSB_MD 		0xBF

// DATA REGISTERS (read-only)
#define REG_OUT_XLSB 	0xF8
#define REG_OUT_LSB		0xF7
#define REG_OUT_MSB		0xF6

// CONTROL REGISTERS
#define REG_CTRL_MEAS	0xF4
#define REG_SOFT_RESET	0xE0
#define REG_CHIP_ID		0xD0

#define START_TEMP		0x2E
#define START_PRES		0x34
#define CHIP_ID			0x55
#define POWER_ON_RESET	0xB6

#define OSS_ULP			0x0 
#define OSS_STD			0x1 // solo voy a usar este en principio
#define OSS_HRES		0x2
#define OSS_UHRES		0x3