#include <asm-generic/errno.h>
#include <asm-generic/errno-base.h>

#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/ioctl.h>
#include <linux/kdev_t.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/version.h>

#include "i2c.h"
#include "bmp.h"

/* --------------- INCLUDES ---------------*/

#define NUM_OF_DEVICES	1
#define MINOR_NUMBER	0

/* --------------- PROTOTIPOS --------------- */

int	 char_dev_init(struct platform_device* pdev);
void char_dev_exit(void);