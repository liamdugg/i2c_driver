#include "../inc/c_dev.h"

/* --------------- PROTOTIPOS --------------- */

static int		char_dev_open(struct inode* inodep, struct file* file);
static int		char_dev_close(struct inode* inodep,struct file* file);
static ssize_t	char_dev_read(struct file* filep, char* usr_buf, size_t len, loff_t* offset);
static ssize_t	char_dev_write(struct file* filep, const char* buffer, size_t len, loff_t* offset);
static long int char_dev_ioctl(struct file* file, unsigned cmd, unsigned long __user arg);
//static char*    char_dev_devnode(struct device* dev, umode_t* mode);

/* --------------- VARIABLES --------------- */

static dev_t devt;
static struct cdev              bmp_cdev;
static struct class 	        *bmp_class;
static struct device 	        *bmp_dev;
static struct platform_device   *bmp_pdev;

static struct file_operations fops = {
    .owner 			= THIS_MODULE,
	.open 			= char_dev_open,
	.release		= char_dev_close,
	.write			= char_dev_write,
	.read			= char_dev_read,
	.unlocked_ioctl = char_dev_ioctl,
};

/* --------------- FUNCIONES --------------- */

int char_dev_init(struct platform_device* pdev) {

    // obtengo major y minor
    if( alloc_chrdev_region(&devt, MINOR_NUMBER, NUM_OF_DEVICES, "bmp180") != 0){
        pr_err("BMP --> Error alloc_chrdev_region.\n");
        return -EINVAL;
    }
    
    //pr_info("BMP --> Major number: %i\n", MAJOR(devt));

    // creo la clase
    if( (bmp_class = class_create(THIS_MODULE, "liam-temp-sensor")) == NULL){
        pr_err("BMP --> Error class_create.\n");
        unregister_chrdev(devt, "bmp180");
        return -EINVAL;
    }

    //pr_info("BMP --> Clase creada.\n");

    // creo el archivo del cdev
    if((bmp_dev = device_create(bmp_class, NULL, devt, NULL, "bmp180")) == NULL){
        pr_err("BMP --> Error device_create.\n");
        class_destroy(bmp_class);
        unregister_chrdev(devt, "bmp180");
        return -EINVAL;
    }

    //pr_info("BMP --> Device creado.\n");

    // inicializo y registro
    cdev_init(&bmp_cdev, &fops);
    if(cdev_add(&bmp_cdev, devt, NUM_OF_DEVICES) != 0){
        pr_err("BMP --> Error cdev_add.\n");
        device_destroy(bmp_class, devt);
        class_destroy(bmp_class);
        unregister_chrdev(devt, "bmp180");
        return -EINVAL;
    }
    
    // pr_info("BMP --> Device inicializado y registrado.\n");

    bmp_pdev = pdev; // en teoria ya no lo uso

    pr_info("BMP --> Char device creado corretamente.\n");
    return 0;
}

void char_dev_exit(void){
        
    device_destroy(bmp_class, devt);
    class_destroy(bmp_class);
    unregister_chrdev(devt, "bmp180");
    
    pr_info("BMP --> %s.\n", __func__);
}

static int char_dev_open(struct inode* inodep, struct file* filep){

    if(i2c_init(bmp_pdev) != 0){
        pr_err("BMP --> No pudo inicializarse el modulo i2c.\n");
        return -EINVAL;
    }
        
    if(bmp_init() != 0){
        pr_err("BMP --> No pudo inicializarse el sensor.\n");
        i2c_remove();
        return -EINVAL;
    }

    pr_info("BMP --> %s\n", __func__);
    return 0;
}

static int char_dev_close(struct inode* inodep, struct file* filep){

    i2c_remove();
    return 0;
}

static ssize_t char_dev_read(struct file* filep, char* usr_buf, size_t len, loff_t* offset){
    
    char data[6];
    
    int16_t temp;
    int32_t pres;

    if(len != 6){
        pr_err("BMP --> tamaño insuficiente para presion y temperatura (=/=6)\n");
        return 0;
    }
     
    // chequeo si el puntero es valido (perteneciente a user space)
    if(!access_ok(VERIFY_READ, usr_buf, len)){
        pr_err("BMP --> Puntero de usuario invalido.No se puede realizar la medicion\n");
        return 0;
    } 

    // OBTENER VALORES
    temp = bmp_get_temp();
    pres = bmp_get_pres();

    data[TEMP_BIT_1] = (temp >> 8)  & 0xFF; // MSB 
    data[TEMP_BIT_0] = temp & 0xFF;         // LSB

    data[PRES_BIT_3] = (pres >> 24) & 0xFF;
    data[PRES_BIT_2] = (pres >> 16) & 0xFF;
    data[PRES_BIT_1] = (pres >> 8)  & 0xFF;
    data[PRES_BIT_0] = pres & 0xFF;

    // envio el buffer al usuario
    if(copy_to_user(usr_buf, data, len) != 0){
        pr_err("BMP --> Error enviando valores al usuario.\n");
        return 0;
    }

    // pr_info("BMP --> Salida de funcion %s\n", __func__);
	return len;
}

static ssize_t char_dev_write(struct file* filep, const char* buffer, size_t len, loff_t* offset){
	
    pr_info("BMP --> %s\n", __func__);
    return len;
}

static long int char_dev_ioctl(struct file* file, unsigned cmd, unsigned long __user arg){

    //pr_info("BMP --> %s\n", __func__);
	return 0;
}

/*
static char* char_dev_devnode(struct device* dev, umode_t* mode){
    
    if(mode != NULL) *mode = 0666;
    
    return NULL;
}
*/