/* ---------- INCLUDES ----------*/
#include <asm-generic/errno.h>
#include <asm-generic/errno-base.h>

#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/io.h>
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

#include "i2c.h"
#include "c_dev.h"

#define DT_PROP_INTERRUPTS	0x1E
#define DT_PROP_CLK_FREQ	0x186A0
#define DT_PROP_REG			0x4819C000
#define DT_PROP_REG_SIZE	0x1000
