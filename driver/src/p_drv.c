#include "../inc/p_drv.h"

static struct device* i2c_dev;
static struct device_node *node;

static int bmp_pdrv_probe(struct platform_device *pdev);
static int bmp_pdrv_remove(struct platform_device *pdev);
static int bmp_pdrv_check_properties(void);

// tabla a pasarle a struct platform_driver.
// busca en el DT dispositivos con la propiedad .compatible especificada.
// de encontrarla se llama a probe().
static const struct of_device_id bmp_of_id[] = {
	{.compatible = "liam,i2c"},
	{ },
};

MODULE_DEVICE_TABLE(of, bmp_of_id);

static struct platform_driver bmp_pdrv = {

	.probe =  bmp_pdrv_probe,	// implementacion requerida
	.remove = bmp_pdrv_remove,	// implementacion requerida
	.driver = {
		.owner = THIS_MODULE,
		.name = "liam_i2c",
		.of_match_table = bmp_of_id,
	},
};

/* --------------------------------------------- */

// llamada al "matchear" con un dispositivo en el DT.
static int bmp_pdrv_probe(struct platform_device *pdev) {

	pr_info("BMP --> Ingreso a funcion %s.\n", __func__);

	// la inicializacion del modulo i2c la hago en el open para no tenerlo
	// corriendo mientras no se esta usando, a costa de un arranque mas lento.

	i2c_dev = &pdev->dev;
	node = i2c_dev->of_node;		
	
	// verifico que levante el dispositivo correcto
	if(bmp_pdrv_check_properties() != 0){
		i2c_dev = NULL;
		pr_err("BMP --> Propiedades del DeviceTree incorrectas.\n");
		return -EINVAL;
	}

	if(char_dev_init(pdev) != 0){
		i2c_dev = NULL;
		pr_err("BMP --> Funcion %s tuvo un error.\n", "bmp_cdev_init");
		return -EINVAL;
	}

	/*if(i2c_init(pdev) != 0){
        pr_err("BMP --> No pudo inicializarse el modulo i2c.\n");
        return -EINVAL;
	}*/
	
	return 0;
}

static int bmp_pdrv_remove(struct platform_device *pdev) {
	char_dev_exit();
	pr_info("BMP --> %s.\n", __func__);
	return 0;
}

static int bmp_pdrv_check_properties(void){
	
	// las propiedades que son strings las verifique manualmente
	// en /proc/device-tree/ocp/i2c-liam@address
	uint32_t aux;
	uint32_t buf[2];
	
	device_property_read_u32(i2c_dev, "interrupts", &aux);
	pr_info("BMP --> interrupts: %i\n", aux);
	if(aux != DT_PROP_INTERRUPTS)
		return -EINVAL;
	
	device_property_read_u32(i2c_dev, "clock-frequency", &aux);
	pr_info("BMP --> clock-freq: %i\n", aux);
	if(aux != DT_PROP_CLK_FREQ)
		return -EINVAL;
	
	device_property_read_u32_array(i2c_dev, "reg", buf, 2);
	pr_info("BMP --> reg: %x \nBMP --> size: %x \n", buf[0], buf[1]);
	if( (buf[0] != DT_PROP_REG) && (buf[1] != DT_PROP_REG_SIZE))
		return -EINVAL;
	
	return 0;
}

// llamada al usar insmod
static int __init bmp_pdrv_init(void){
	
	// registro platform driver
	if( platform_driver_register(&bmp_pdrv) != 0){
		pr_err("BMP --> No se pudo inicializar el platform driver.\n");
		return -EINVAL;
	} 

	pr_info("BMP --> Platform driver inicializado correctamente.\n");
	return 0;
}

// llamada al usar rmmod
static void __exit bmp_pdrv_exit(void){

	platform_driver_unregister(&bmp_pdrv);
	pr_info("BMP --> Platform driver removido exitosamente.\n");
}

module_init(bmp_pdrv_init); // bind de funcion init
module_exit(bmp_pdrv_exit); // bind de funcion exit

MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");
MODULE_AUTHOR("Liam Duggan");
MODULE_DESCRIPTION("Driver para sensor de temperatura i2c bmp180");