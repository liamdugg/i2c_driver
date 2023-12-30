#include "../inc/i2c.h"

typedef enum {
	MODE_TX,
	MODE_RESTART, // repeated start
	MODE_RX,
	MODE_IDLE
} i2c_mode;

typedef struct i2c_handler_t {
	
	struct platform_device* pdev;
	
	void* __iomem instance;	// i2c register mapped based addr ptr
	uint32_t virq; 			// virtual irq
	
	uint8_t* tx_buf;
	uint8_t  tx_len; // cantidad a transmitir
	uint8_t  tx_pos; // cantidad transmitida 
	
	uint8_t* rx_buf;
	uint8_t  rx_len; // cantidad a recibir
	uint8_t  rx_pos; // cantidad recibida

	i2c_mode mode;

} i2c_handler_t;

//static i2c_handler_t* hm_i2c = NULL;
static i2c_handler_t hm_i2c;

//static void* __iomem i2c_base = NULL
static void* __iomem cm_per_base = NULL;
static void* __iomem control_module_base = NULL;
static void* __iomem instance = NULL;

DEFINE_MUTEX(lock_i2c);
DECLARE_WAIT_QUEUE_HEAD(wq_tx);
DECLARE_WAIT_QUEUE_HEAD(wq_rx);

static int wq_cond_tx;
static int wq_cond_rx;

/* ------------------------------ */

static int  i2c_is_busy(void);
static int  i2c_set_irq(void);
static int  i2c_set_txbuf(uint8_t* buf, uint8_t size);
static int  i2c_set_rxbuf(uint8_t* buf, uint8_t size);
static void i2c_set_slave(uint8_t slv_addr);

/* ---------------------------------------- */

// INIT
int i2c_init(struct platform_device* pdev){

	pr_info("BMP --> Ingreso a %s\n", __func__);

	uint32_t reg;
	uint32_t div;
	uint32_t prescaler = (SYSCLK/MODCLK) - 1;

	pr_info("BMP --> Empiezo mapeo\n");

	/* --------------- mapeos --------------- */

	// obtengo memoria para guardar una struct
	// cada device que entre a la funcion se le asigna una struct i2c_handler_t
	//hm_i2c = (i2c_handler_t*) devm_kzalloc(&(pdev->dev), sizeof(i2c_handler_t), GFP_KERNEL);
	
	/*pr_info("BMP --> 1\n");
	if(IS_ERR(hm_i2c)){
		pr_err("BMP --> Error en devm_kzalloc(). No pudo abrirse el dispositivo.\n");
		return -EINVAL;
	}*/

	hm_i2c.pdev = pdev;
	
	pr_info("BMP --> 3\n");
	cm_per_base = ioremap(CM_PER_BASE, 0x400);
	if(cm_per_base == NULL) {
		pr_err("BMP --> Error con mapeo de cm_per_base.\n");
		iounmap(instance);
		//kfree(hm_i2c);
		return -EINVAL;
	}

	pr_info("BMP --> 4\n");
	control_module_base = ioremap(CTRL_MOD_BASE, 0x2000);
	if(control_module_base == NULL){
		pr_err("BMP --> Error con enable en cm_per.\n");
		iounmap(cm_per_base);
		iounmap(instance);
		//kfree(hm_i2c);
		return -EINVAL;
	}

	instance = ioremap(I2C_REG_BASE, I2C_REG_SIZE);

	/* --------------- clock init --------------- */
	
	//reg &= CM_PER_I2C2_CLKCTRL_MSK;
	reg |= CM_PER_I2C2_MODMODE_EN;
	iowrite32(reg, cm_per_base + CM_PER_I2C2_CLKCTRL);

	reg = ioread32(cm_per_base + CM_PER_I2C2_CLKCTRL);
	//pr_info("BMP --> %x\n", instance);
	if(instance == NULL){
		pr_err("BMP --> Error con mapeo de instance.\n");
		return -EINVAL;
	}

	pr_info("BMP --> 5\n");
	//pr_info("BMP --> peripheral: %i\n", ioread32(cm_per_base));
	//pr_info("BMP --> peripheral: %i\n", ioread32(control_module_base));
	
	//mdelay(10);

	reg = ioread32(cm_per_base + CM_PER_I2C2_CLKCTRL);
	if( (reg & CM_PER_I2C2_MODMODE_MSK) != CM_PER_I2C2_MODMODE_EN){
		pr_err("BMP --> Error con enable en cm_per.\n");
		iounmap(cm_per_base);
		iounmap(control_module_base);
		iounmap(instance);
		//kfree(hm_i2c);
		return -EINVAL;
	}
	
	/* --------------- control module init --------------- */
	
	// scl
	pr_info("BMP --> 8\n");
	reg = ioread32(control_module_base + CTRL_MOD_SCL_OFF);
	reg &= CTRL_MOD_SCL_MSK;
	reg |= CTRL_MOD_SCL; // fast, receiver enabled, pullup, pull disabled, mode 3
	iowrite32(reg, control_module_base + CTRL_MOD_SCL_OFF);

	// sda (cambiar valores, dejo el prototipo)
	pr_info("BMP --> 10\n");
	reg = ioread32(control_module_base + CTRL_MOD_SDA_OFF);
	reg &= CTRL_MOD_SDA_MSK;
	reg |= CTRL_MOD_SCL;
	iowrite32(reg, control_module_base + CTRL_MOD_SDA_OFF);

	/* --------------- i2c init ---------------*/
	
	/* 
	por algun motivo siempre queda en 0 por lo que no puedo salir del loop
	omito el reset.
	
	iowrite32(I2C_SYSC_SRST, instance + I2C_SYSC);
	do{	
		pr_info("BMP --> Loop\n");
		reg = ioread32(instance  + I2C_SYSS);
		pr_info("BMP --> reg: %i\n", (reg & 0x1));
		mdelay(10);
	} while( (reg&0x1) != 0x1); 
	*/

	pr_info("BMP --> AA\n");

	// disable i2c2
	reg = ioread32(instance + I2C_CON);
	reg &= ~(I2C_CON_ENA);
	iowrite32(0x308, instance + I2C_SYSC);

	pr_info("BMP --> BB\n");

	// set prescaler
	iowrite32(prescaler, instance + I2C_PSC);

	pr_info("BMP --> CC\n");

	// set SCLL and SCLH
	div = MODCLK/(2*OUTCLK);
	iowrite32(div-7, instance + I2C_SCLL);
	iowrite32(div-5, instance + I2C_SCLH);

	pr_info("BMP --> DD\n");

	// set OWN ADDRESS
	iowrite32(0x54, instance + I2C_OA);

	pr_info("BMP --> EE\n");

	// set I2C_CON 7 bit addr, master mode
	iowrite32(0x84000, instance + I2C_CON);

	pr_info("BMP --> FF\n");

	// habilito interrupciones
	iowrite32(I2C_IRQ_XRDY | I2C_IRQ_ARDY | I2C_IRQ_RRDY, instance + I2C_IRQENA_SET);

	pr_info("BMP --> GG\n");

	// configuro la irq
	if(i2c_set_irq() != 0){
		pr_err("BMP --> Error en la configuracion de irq.\n");
		iounmap(cm_per_base);
		iounmap(control_module_base);
		iounmap(instance);
		//kfree(hm_i2c);
		return -EINVAL;
	}

	pr_info("BMP --> HH\n");

	// libero los punteros que no voy a volver a usar (en teoria)
	iounmap(control_module_base);
	iounmap(cm_per_base);

	pr_info("BMP --> II\n");

	reg = ioread32(instance + I2C_CON);
	reg |= I2C_CON_ENA;
	iowrite32(reg, instance + I2C_CON);

	pr_info("BMP --> Saliendo de %s", __func__);
	return 0;
}

// REMOVE
void i2c_remove(void){

	uint32_t reg;
	reg = ioread32(instance + I2C_CON);
	reg &= ~(I2C_CON_ENA);
	iowrite32(reg,instance + I2C_CON);

	iounmap(instance);

	free_irq(hm_i2c.virq, NULL);

	if(hm_i2c.rx_buf != NULL)
		kfree(hm_i2c.rx_buf);

	if(hm_i2c.tx_buf != NULL)
		kfree(hm_i2c.tx_buf);

	//kfree(hm_i2c);
}

/* ------------------kfree(hm_i2c.rx_buf);---------------------- */

// WRITE
int i2c_write(uint8_t slv_addr, uint8_t* data_to_copy, uint8_t size){

	uint32_t reg;

	if(size == 0){
		pr_err("BMP --> Error, el tamaño no puede ser cero.\n");
		return -EINVAL;
	}

	if(i2c_is_busy() != 0){
		pr_err("BMP --> Timeout, el dispositivo esta ocupado.\n");
		return -ETIMEDOUT;
	}

	mutex_lock(&lock_i2c);

	i2c_set_slave(slv_addr);
	if( i2c_set_txbuf(data_to_copy, size) != 0) return -EINVAL;

	// limpio flags irq
	reg = ioread32(instance + I2C_IRQSTAT);
	iowrite32(reg & ~(I2C_IRQ_RRDY | I2C_IRQ_XRDY | I2C_IRQ_ARDY), instance + I2C_IRQSTAT);

	// cargo cantidad de bits a escribir
	iowrite32(hm_i2c.tx_len, instance + I2C_CNT);
	hm_i2c.mode = MODE_RESTART;

	// modo tx, start condition, enable y master (las ultimas 2 por las dudas)
	iowrite32(I2C_CON_ENA | I2C_CON_MST | I2C_CON_TMOD | I2C_CON_START, instance + I2C_CON);

	wq_cond_tx = 0;
	wait_event_interruptible(wq_tx, wq_cond_tx == 1);

	kfree(hm_i2c.tx_buf);
	mutex_unlock(&lock_i2c);
	return 0;
}

// READ
int i2c_read(uint8_t slv_addr, uint8_t* reg_addr, uint8_t* store_buf, uint8_t size){

	if(size == 0){
		pr_err("BMP --> Error, el tamaño no puede ser cero.\n");
		return -EINVAL;
	}

	if(i2c_is_busy() != 0){
		pr_err("BMP --> Timeout, el dispositivo estas ocupado");
		return -ETIMEDOUT;
	}

	// datasheet bmp180:
	// 	- after start condition master sends the slave address + write, and register address.
	// 	- the register address selects the read register
	// 	- master sends a restart condition followed by slave address + read, acknowledged by BMP180.
	// 	- BMP180 sends first the 8 MSB, acknowledged by master,
	// 	- then the 8 LSB, master sends "not acknowledge" and finally a stop condition.

	mutex_lock(&lock_i2c);

	i2c_set_slave(slv_addr);
	if( i2c_set_rxbuf(store_buf, size) != 0) return -EINVAL;
	if( i2c_set_txbuf(reg_addr, 1) != 0) return -EINVAL;

	// le escribo 1 byte de la direccion de registro
	iowrite32(1, instance + I2C_CNT);

	// con esto se que tengo que hacer un repeated start
	hm_i2c.mode = MODE_RESTART;

	// habilito, modo master, modo tx y start condition
	iowrite32(I2C_CON_ENA | I2C_CON_MST | I2C_CON_TMOD | I2C_CON_START, instance + I2C_CON);

	wq_cond_rx = 0;
	wait_event_interruptible(wq_rx, wq_cond_rx == 1);

	if(memcpy(store_buf, hm_i2c.rx_buf, size) == NULL)
		return -EINVAL;

	kfree(hm_i2c.tx_buf);
	kfree(hm_i2c.rx_buf);
	mutex_unlock(&lock_i2c);

	return 0;
}

// IRQ HANDLER
static irqreturn_t i2c_handler(int irq, void *dev_id, struct pt_regs* regs){

	uint32_t irq_stat = ioread32(instance + I2C_IRQSTAT);
	uint32_t reg;

	iowrite32(irq_stat & ~(I2C_IRQ_RRDY | I2C_IRQ_XRDY | I2C_IRQ_ARDY), instance + I2C_IRQSTAT);

	// TX
	if(irq_stat & I2C_IRQ_XRDY){
		if(hm_i2c.tx_len < hm_i2c.tx_pos)
			iowrite32(hm_i2c.tx_buf[hm_i2c.tx_pos++], instance + I2C_DATA);
	}

	// RX
	if(irq_stat & I2C_IRQ_RRDY){
		if(hm_i2c.rx_len < hm_i2c.rx_pos)
			hm_i2c.rx_buf[hm_i2c.rx_pos++] = ioread32(instance + I2C_DATA);
	}

	// ARDY
	if(irq_stat & I2C_IRQ_ARDY) {

		if(hm_i2c.mode == MODE_RESTART){

			wq_cond_tx = 1;
			wake_up_interruptible(&wq_tx);

			iowrite32(hm_i2c.rx_len, instance + I2C_CNT);
			iowrite32(I2C_CON_ENA | I2C_CON_MST | I2C_CON_START, instance + I2C_CON);
			hm_i2c.mode = MODE_RX;
		}

		else {

			if(hm_i2c.mode == MODE_TX){
				wq_cond_tx = 1;
				wake_up_interruptible(&wq_tx);
			}

			else if(hm_i2c.mode == MODE_RX){
				wq_cond_rx = 1;
				wake_up_interruptible(&wq_rx);
			}

			hm_i2c.mode = MODE_IDLE;

			// stop condition
			reg = ioread32(instance + I2C_CON);
			reg &= ~(I2C_CON_START);
			reg |= I2C_CON_STOP;
			iowrite32(reg, instance + I2C_CON);
		}
	}

	if (irq_stat & I2C_IRQ_NACK)
		pr_warn("BMP --> NACK recibido.\n");

	return (irqreturn_t) IRQ_HANDLED;
}

/* ------------------------------ */

static int i2c_is_busy(){

	int i = 0;

	while( (ioread32(instance + I2C_IRQSTAT) & 0x1000) != 0){

		mdelay(1);

		if(i++ == 50){
			pr_err("BMP --> Timeout, el bus i2c esta ocupado.\n");
			return -ETIMEDOUT;
		}
	}

	return 0;
}

static int i2c_set_irq(){

	// obtengo numero de irq
	hm_i2c.virq = platform_get_irq(hm_i2c.pdev, 0);
	if(hm_i2c.virq < 0){
		pr_err("BMP --> No se pudo obtener numero de IRQ.\n");
		return -EINVAL;
	}

	// seteo el handler de la irq del i2c
	if(devm_request_irq(&(hm_i2c.pdev->dev), hm_i2c.virq, (irq_handler_t)i2c_handler, IRQF_TRIGGER_RISING, "liam,i2c", 0) < 0){
		free_irq(hm_i2c.virq, NULL);
		pr_err("BMP --> No pudo setearse la isr correctamente.\n");
		return -EINVAL;
	}

	return 0;
}

static int i2c_set_txbuf(uint8_t* buf, uint8_t size){

	// reservo memoria y luego copio el contenido de buffer
	if( (hm_i2c.tx_buf = (uint8_t*)kmalloc(size, GFP_KERNEL)) == NULL)
		return -1;

	if(memcpy(hm_i2c.tx_buf, buf, size) == NULL){
		kfree(hm_i2c.tx_buf);
		return -1;
	}

	hm_i2c.tx_len = size;	// seteo el tamano
	hm_i2c.tx_pos = 0;		// seteo la posicion actual a 0
	return 0;
}

static int i2c_set_rxbuf(uint8_t* buf, uint8_t size){

	// reservo memoria
	if((hm_i2c.rx_buf = (uint8_t*)kmalloc(size, GFP_KERNEL)) == NULL)
		return -1;

	hm_i2c.rx_len = size;	// seteo el tamano
	hm_i2c.rx_pos = 0;		// seteo la posicion actual a 0
	return 0;
}

static void i2c_set_slave(uint8_t slv_addr){
	iowrite32(slv_addr, instance + I2C_SA);
}
