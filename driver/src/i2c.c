#include "../inc/i2c.h"

/* ------------------ TYPEDEFS ---------------------- */

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
	
	uint8_t  tx_buf[BUF_SIZE];
	uint8_t  tx_len; 		// cantidad a transmitir
	uint8_t  tx_pos; 		// cantidad transmitida 
	
	uint8_t  rx_buf[BUF_SIZE];
	uint8_t  rx_len; 		// cantidad a recibir
	uint8_t  rx_pos; 		// cantidad recibida

	i2c_mode mode;

} i2c_handler_t;

/* ------------------ VARIABLES ---------------------- */

static i2c_handler_t hm_i2c;

static void* __iomem cm_per_base = NULL;
static void* __iomem control_module_base = NULL;
static void* __iomem instance = NULL;

DEFINE_MUTEX(lock_i2c);
DECLARE_WAIT_QUEUE_HEAD(wq);
//DECLARE_WAIT_QUEUE_HEAD(wq);

static int wq_cond_tx;
static int wq_cond_rx;

/* ------------------ PROTOTIPOS ---------------------- */

static int  i2c_is_busy(void);
static int  i2c_set_irq(void);
static int  i2c_is_locked(void);
static int  i2c_set_txbuf(uint8_t* buf, uint8_t size);
static void i2c_set_rxbuf(uint8_t size);
static void i2c_set_slave(uint8_t slv_addr);

/* ------------------ FUNCIONES ---------------------- */

// INIT
int i2c_init(struct platform_device* pdev){

	uint32_t reg;
	uint32_t div;
	uint32_t prescaler = (SYSCLK/MODCLK) - 1;

	pr_info("BMP --> Ingreso a %s\n", __func__);
	
	hm_i2c.pdev = pdev;

	/* --------------- mapeos --------------- */
	
	pr_info("BMP --> Empiezo mapeo\n");
	
	cm_per_base = ioremap(CM_PER_BASE, 0x400);
	if(cm_per_base == NULL) {
		pr_err("BMP --> Error con mapeo de cm_per_base.\n");
		iounmap(instance);
		return -EINVAL;
	}

	control_module_base = ioremap(CTRL_MOD_BASE, 0x2000);
	if(control_module_base == NULL){
		pr_err("BMP --> Error con enable en cm_per.\n");
		iounmap(cm_per_base);
		iounmap(instance);
		return -EINVAL;
	}

	instance = ioremap(I2C_REG_BASE, I2C_REG_SIZE);
	if(instance == NULL){
		pr_err("BMP --> Error con mapeo de instance.\n");
		return -EINVAL;
	}

	/* --------------- clock init --------------- */
	
	reg = ioread32(cm_per_base + CM_PER_I2C2_CLKCTRL);
	reg |= CM_PER_I2C2_MODMODE_EN;
	iowrite32(reg, cm_per_base + CM_PER_I2C2_CLKCTRL);

	reg = ioread32(cm_per_base + CM_PER_I2C2_CLKCTRL);
	if( (reg & CM_PER_I2C2_MODMODE_MSK) != CM_PER_I2C2_MODMODE_EN){
		pr_err("BMP --> Error con enable en cm_per.\n");
		iounmap(cm_per_base);
		iounmap(control_module_base);
		iounmap(instance);
		return -EINVAL;
	}
	
	/* --------------- control module init --------------- */
	
	// scl
	reg = ioread32(control_module_base + CTRL_MOD_SCL);
	reg &= CTRL_MOD_SCL_MSK;
	reg |= CTRL_MOD_SCL_MODE;
	iowrite32(reg, control_module_base + CTRL_MOD_SCL);

	// sda (cambiar valores, dejo el prototipo)
	reg = ioread32(control_module_base + CTRL_MOD_SDA);
	reg &= CTRL_MOD_SDA_MSK;
	reg |= CTRL_MOD_SCL_MODE;
	iowrite32(reg, control_module_base + CTRL_MOD_SDA);

	/* --------------- i2c init ---------------*/
	
	// por algun motivo siempre queda en 0 por lo que no puedo salir del loop omito el reset.	
	// iowrite32(I2C_SYSC_SRST, instance + I2C_SYSC);
	// do{	
	// 	reg = ioread32(instance  + I2C_SYSS);
	// 	pr_info("BMP --> reg: %i\n", (reg & 0x1));
	// 	mdelay(10);
	//
	// } while( (reg&0x1) != 0x1); 

	// disable i2c2
	reg = ioread32(instance + I2C_CON);
	reg &= ~(I2C_CON_ENA);
	iowrite32(0x308, instance + I2C_SYSC);

	// set prescaler
	iowrite32(prescaler, instance + I2C_PSC);

	// set SCLL and SCLH
	div = MODCLK/(2*OUTCLK);
	iowrite32(div-7, instance + I2C_SCLL);
	iowrite32(div-5, instance + I2C_SCLH);

	// set OWN ADDRESS
	iowrite32(0x54, instance + I2C_OA);

	// set I2C_CON 7 bit addr, master mode
	iowrite32(0x84000, instance + I2C_CON);

	// configuro la irq
	if(i2c_set_irq() != 0){
		pr_err("BMP --> Error en la configuracion de irq.\n");
		iounmap(cm_per_base);
		iounmap(control_module_base);
		iounmap(instance);
		return -EINVAL;
	}

	// libero los punteros que no voy a volver a usar (en teoria)
	iounmap(control_module_base);
	iounmap(cm_per_base);

	reg = ioread32(instance + I2C_CON);
	reg |= I2C_CON_ENA;
	iowrite32(reg, instance + I2C_CON);

	pr_info("BMP --> Saliendo de %s", __func__);
	return 0;
}

// REMOVE
void i2c_remove(void){

	uint32_t reg;

	//pr_info("BMP --> Ingreso a funcion %s", __func__);
	
	//pr_info("BMP --> Deshabilito I2C\n");
	reg = ioread32(instance + I2C_CON);
	reg &= ~(I2C_CON_ENA);
	iowrite32(reg,instance + I2C_CON);

	//pr_info("BMP --> Unmap instance\n");
	iounmap(instance);
	pr_info("BMP --> free irq\n");
	free_irq(hm_i2c.virq, NULL);

	//pr_info("BMP --> Salida de funcion %s", __func__);
	return;
}

// WRITE
int i2c_write(uint8_t slv_addr, uint8_t* data_to_copy, uint8_t size){

	uint32_t reg;

	if(size == 0){
		pr_err("BMP --> Error, el tamaño no puede ser cero.\n");
		return -EINVAL;
	}

	reg = ioread32(cm_per_base + CM_PER_I2C2_CLKCTRL);
	reg |= CM_PER_I2C2_MODMODE_EN;
	iowrite32(reg, cm_per_base + CM_PER_I2C2_CLKCTRL);

	if(i2c_is_locked() != 0)
		return -ETIMEDOUT;
	
	mutex_lock(&lock_i2c);

	i2c_set_slave(slv_addr);
	if( i2c_set_txbuf(data_to_copy, size) != 0) return -EINVAL;
	
	// cargo cantidad de bits a escribir
	iowrite32(hm_i2c.tx_len, instance + I2C_CNT);

	// habilito XRDY y deshabilito RRDY y ARDY
	iowrite32(I2C_IRQ_RRDY | I2C_IRQ_ARDY, instance + I2C_IRQENA_CLR);
	iowrite32(I2C_IRQ_XRDY, instance + I2C_IRQENA_SET);

	wq_cond_tx = 0;
	
	if(i2c_is_busy() != 0)
		return -ETIMEDOUT;

	// modo tx, start condition, enable y master (las ultimas 2 por las dudas)
	reg = ioread32(instance + I2C_CON);
	reg &= ~(I2C_CON_TMOD);
	reg |= I2C_CON_ENA | I2C_CON_MST | I2C_CON_TMOD | I2C_CON_START;
	iowrite32(reg, instance + I2C_CON);

	wait_event_interruptible(wq, wq_cond_tx == 1);
	
	reg = ioread32(instance + I2C_CON);
	reg &= 0xFFFFFFFE;
	reg |= I2C_CON_STOP;
	iowrite32(reg, instance + I2C_CON);
	
	udelay(100);
	mutex_unlock(&lock_i2c);
	return 0;
}

// READ
int i2c_read(uint8_t slv_addr, uint8_t* reg_addr, uint8_t* store_buf, uint8_t size){

	uint32_t reg;

	if(size == 0){
		pr_err("BMP --> Error, el tamaño no puede ser cero.\n");
		return -EINVAL;
	}

	reg = ioread32(cm_per_base + CM_PER_I2C2_CLKCTRL);
	reg |= CM_PER_I2C2_MODMODE_EN;
	iowrite32(reg, cm_per_base + CM_PER_I2C2_CLKCTRL);

	if(i2c_is_locked() != 0)
		return -ETIMEDOUT;

	mutex_lock(&lock_i2c);

	i2c_set_slave(slv_addr);
	i2c_set_rxbuf(size);

	// habilito XRDY y deshabilito RRDY y ARDY
	iowrite32(I2C_IRQ_XRDY | I2C_IRQ_ARDY, instance + I2C_IRQENA_CLR);
	iowrite32(I2C_IRQ_RRDY, instance + I2C_IRQENA_SET);

	// cargo cantidad de bits a escribir
	iowrite32(size, instance + I2C_CNT);

	wq_cond_rx = 0;
	// hm_i2c.mode = MODE_RX;

	if(i2c_is_busy() != 0)
		return -ETIMEDOUT;

	// habilito, modo master, modo rx y start condition
	reg = ioread32(instance + I2C_CON);
	reg &= ~(I2C_CON_TMOD);
	reg |= I2C_CON_ENA | I2C_CON_MST | I2C_CON_START;
	iowrite32(reg, instance + I2C_CON);

	wait_event_interruptible(wq, wq_cond_rx == 1);
	
	reg = ioread32(instance + I2C_CON);
	reg &= 0xFFFFFFFE;
	reg |= I2C_CON_STOP;
	iowrite32(reg, instance + I2C_CON);
	
	udelay(100);

	pr_info("BMP --> RX: 0x%x\n", hm_i2c.rx_buf[0]);
	store_buf[0] = hm_i2c.rx_buf[0];
	store_buf[1] = hm_i2c.rx_buf[1];
	
	mutex_unlock(&lock_i2c);
	return 0;
}

// IRQ HANDLER
static irqreturn_t i2c_handler(int irq, void *dev_id, struct pt_regs* regs){

	uint32_t irq_stat = ioread32(instance + I2C_IRQSTAT);
	//uint32_t reg;

	iowrite32(irq_stat & (I2C_IRQ_RRDY | I2C_IRQ_XRDY | I2C_IRQ_ARDY), instance + I2C_IRQSTAT);

	// TX
	if(irq_stat & I2C_IRQ_XRDY){

		//pr_info("BMP --> TX_POS: %i\n", hm_i2c.tx_pos);

		pr_info("BMP --> TX: 0x%x\n", hm_i2c.tx_buf[hm_i2c.tx_pos]);
		iowrite32(hm_i2c.tx_buf[hm_i2c.tx_pos++], instance + I2C_DATA);

		if(hm_i2c.tx_pos >= hm_i2c.tx_len){
			iowrite32(I2C_IRQ_XRDY, instance + I2C_IRQENA_CLR);
			wq_cond_tx = 1;
			wake_up_interruptible(&wq);
		}
	}

	// RX
	if(irq_stat & I2C_IRQ_RRDY){

		//pr_info("BMP --> RX_POS: %i\n", hm_i2c.rx_pos);
		hm_i2c.rx_buf[hm_i2c.rx_pos++] = ioread32(instance + I2C_DATA);	

		if(hm_i2c.rx_pos >= hm_i2c.rx_len){
			iowrite32(I2C_IRQ_RRDY, instance + I2C_IRQENA_CLR);
			wq_cond_rx = 1;
			//pr_info("BMP --> RX_OUT\n");
			wake_up_interruptible(&wq);
		}	
	}

	if (irq_stat & I2C_IRQ_NACK)
		pr_warn("BMP --> NACK recibido.\n");

	return (irqreturn_t) IRQ_HANDLED;
}

/* ------------------ AUX ---------------------- */

static int i2c_is_busy(void){

	int i = 0;

	while( (ioread32(instance + I2C_IRQSTAT_RAW) & (1<<12)) != 0){

		mdelay(1);

		if(i++ == 100){
			pr_err("BMP --> Timeout, el bus i2c esta ocupado.\n");
			return -ETIMEDOUT;
		}
	}

	return 0;
}

static int i2c_is_locked(void){
	
	int i = 0;

	while(mutex_is_locked(&lock_i2c)){

		mdelay(1);

		if(i++ == 100){
			pr_err("BMP --> Timeout, el mutex esta bloqueado.\n");
			return -ETIMEDOUT;
		}
	}
	return 0;
}

static int i2c_set_irq(){

	//pr_info("BMP --> Entro a %s\n.", __func__);

	// obtengo numero de irq
	hm_i2c.virq = platform_get_irq(hm_i2c.pdev, 0);
	if(hm_i2c.virq < 0){
		pr_err("BMP --> No se pudo obtener numero de IRQ.\n");
		return -EINVAL;
	}

	//pr_info("BMP --> Entro a %s\n.", __func__);

	// seteo el handler de la irq del i2c
	if(request_irq(hm_i2c.virq, (irq_handler_t)i2c_handler, IRQF_TRIGGER_RISING, "liam,i2c", NULL) < 0){
		pr_err("BMP --> No pudo setearse la isr correctamente.\n");
		return -EINVAL;
	}
	
	//pr_info("BMP --> Salgo de %s\n.", __func__);
	return 0;
}

static int i2c_set_txbuf(uint8_t* buf, uint8_t size){

	// reservo memoria y luego copio el contenido de buffer
	hm_i2c.tx_buf[0] = buf[0];
	hm_i2c.tx_buf[1] = buf[1];

	hm_i2c.tx_len = size;	// seteo el tamano
	hm_i2c.tx_pos = 0;		// seteo la posicion actual a 0
	return 0;
}

static void i2c_set_rxbuf(uint8_t size){

	hm_i2c.rx_buf[0] = 0;
	hm_i2c.rx_buf[1] = 0;

	hm_i2c.rx_len = size;	// seteo el tamano
	hm_i2c.rx_pos = 0;		// seteo la posicion actual a 0
}

static void i2c_set_slave(uint8_t slv_addr){
	iowrite32(slv_addr, instance + I2C_SA);
}
