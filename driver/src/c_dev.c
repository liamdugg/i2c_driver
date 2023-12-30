#include "../inc/c_dev.h"

/* --------------- PROTOTIPOS --------------- */

static int		char_dev_open(struct inode* inodep, struct file* file);
static int		char_dev_close(struct inode* inodep,struct file* file);
static ssize_t	char_dev_read(struct file* filep, char* usr_buf, size_t len, loff_t* offset);
static ssize_t	char_dev_write(struct file* filep, const char* buffer, size_t len, loff_t* offset);
static long int char_dev_ioctl(struct file* file, unsigned cmd, unsigned long __user arg);
static char*    char_dev_devnode(struct device* dev, umode_t* mode);

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
        pr_err("BMP --> Error alloc_chrdcev_region.\n");
        return -EINVAL;
    }
    pr_info("BMP --> Major number: %i\n", MAJOR(devt));

    // creo la clase
    if( (bmp_class = class_create(THIS_MODULE, "liam-temp-sensor")) == NULL){
        pr_err("BMP --> Error class_create.\n");
        unregister_chrdev(devt, "bmp180");
        return -EINVAL;
    }
    pr_info("BMP --> Clase creada.\n");

    // creo el archivo del cdev
    if((bmp_dev = device_create(bmp_class, NULL, devt, NULL, "bmp180")) == NULL){
        pr_err("BMP --> Error device_create.\n");
        class_destroy(bmp_class);
        unregister_chrdev(devt, "bmp180");
        return -EINVAL;
    }

    pr_info("BMP --> Device creado.\n");

    // inicializo y registro
    cdev_init(&bmp_cdev, &fops);
    if(cdev_add(&bmp_cdev, devt, NUM_OF_DEVICES) != 0){
        pr_err("BMP --> Error cdev_add.\n");
        device_destroy(bmp_class, devt);
        class_destroy(bmp_class);
        unregister_chrdev(devt, "bmp180");
        return -EINVAL;
    }
    
    pr_info("BMP --> Device inicializado y registrado.\n");

    bmp_pdev = pdev; // en teoria ya no lo uso

    pr_info("BMP --> Char device creado corretamente.\n");
    return 0;
}

void char_dev_exit(void){
    
    pr_info("BMP --> Ingreso a funcion %s.\n", __func__);
    
    device_destroy(bmp_class, devt);
    class_destroy(bmp_class);
    unregister_chrdev(devt, "bmp180");
    
    pr_info("BMP --> Salida de funcion %s.\n", __func__);
}

static int char_dev_open(struct inode* inodep, struct file* filep){

    pr_info("BMP --> Ingreso a funcion %s", __func__);

    /*if(i2c_init(bmp_pdev) != 0){
        pr_err("BMP --> No pudo inicializarse el modulo i2c.\n");
        return -EINVAL;
    }*/
        
    if(bmp_init() != 0){
        pr_err("BMP --> No pudo inicializarse el sensor.\n");
        i2c_remove();
        return -EINVAL;
    }

    pr_info("BMP --> Sensor inicializado.\n");
    return 0;
}

static int char_dev_close(struct inode* inodep, struct file* filep){

    pr_info("BMP --> Ingreso a funcion %s", __func__);
    i2c_remove();
    return 0;
}

static ssize_t char_dev_read(struct file* filep, char* usr_buf, size_t len, loff_t* offset){
    
    char aux_buf[len];

    pr_info("BMP --> Ingreso a funcion %s.\n", __func__);

    // si quiere leer un solo valor asumo que quiere el de temperatura
    // si quiere 2 envio temperatura y presion.
    if(len != 1 && len != 2){
        pr_err("BMP --> Error, ingrese un valor valido de datos requeridos.\n");
        return 0;
    }

    if(!access_ok(VERIFY_READ, usr_buf, len)){
        pr_err("BMP --> Puntero de usuario invalido.No se puede realizar la medicion\n");
        return 0;
    } 

    bmp_measure();

    aux_buf[0] = (char)bmp_get_temp();
    pr_info("BMP --> La temperatura es de %c °C.\n", aux_buf[0]);

    if(len == 2){
        aux_buf[1] = (char)(bmp_get_pres()/1000); // /1000 para obtener en kPa y que entre en un char
        pr_info("BMP --> La presion es de %c kPa.\n", usr_buf[1]);
    }

    // envio el buffer al usuario
    if(copy_to_user(usr_buf, aux_buf, len) != 0){
        pr_err("BMP --> Error enviando valores al usuario.\n");
        return 0;
    }

	return len;
}

static ssize_t char_dev_write(struct file* filep, const char* buffer, size_t len, loff_t* offset){
	
    pr_info("BMP --> Ingreso a funcion %s. Nada para hacer, saliendo...\n", __func__);
    return len;
}

static long int char_dev_ioctl(struct file* file, unsigned cmd, unsigned long __user arg){

    pr_info("BMP --> IOCTL por ahora no hace nada, a implementar.\n");
	return 0;
}

static char* char_dev_devnode(struct device* dev, umode_t* mode){
    
    if(mode != NULL) *mode = 0666;
    
    return NULL;
}





// int char_dev_init(struct platform_device* pdev) {

// 	pr_info("BMP --> Ingreso a funcion %s.\n", __func__);
    
//     /* --------------- reservo major y minor numbers --------------- */
    
//     if( alloc_chrdev_region(&devt, MINOR_NUMBER, NUM_OF_DEVICES, "bmp180") != 0){
//         pr_err("BMP --> Error al registrar major y minor.\n");
//         return -EINVAL;
//     }
//     pr_info("BMP --> Se reservo el major: %d", MAJOR(devt));

//     /* --------------- creo clase del dispositivo --------------- */

//     if((bmp_class = class_create(THIS_MODULE, "i2c-sensor")) == NULL){

//         unregister_chrdev_region(devt, NUM_OF_DEVICES);
//         pr_err("BMP --> Error creando clase");
//         return -EINVAL;
//     }

//     pr_info("BMP --> Clase registrada correctamente.\n");
    
//     /* --------------- creo char device --------------- */

//     if((bmp_cdev = cdev_alloc()) == NULL){
//         class_unregister(bmp_class);
//         class_destroy(bmp_class);
//         unregister_chrdev_region(devt, NUM_OF_DEVICES);
//     }

//     cdev_init(bmp_cdev, &fops);

//     if(cdev_add(bmp_cdev, devt, NUM_OF_DEVICES) < 0){
//         cdev_del(bmp_cdev);
//         class_unregister(bmp_class);
//         class_destroy(bmp_class);
//         unregister_chrdev_region(devt, NUM_OF_DEVICES);
//         return -EINVAL;
//     }
    
//     bmp_class->devnode = char_dev_devnode;
    
//     if((bmp_dev = device_create(bmp_class, NULL, devt, NULL, "bmp180")) == NULL){
//         cdev_del(bmp_cdev);
//         class_unregister(bmp_class);
//         class_destroy(bmp_class);
//         unregister_chrdev_region(devt, NUM_OF_DEVICES);
//         pr_err("BMP --> Error creando char device.\n");
//     }
//     pr_info("BMP --> Char device creado correctamente.\n");

//     bmp_pdev = pdev; // guardo puntero a pdev, luego lo uso en open()
//     return 0;
// }