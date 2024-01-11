#include "../inc/i2c.h"

/* --------------- TYPEDEFS --------------- */

typedef struct {
    
    uint8_t* tx_buf;
	uint8_t* rx_buf;  
    
    uint8_t tx_pos;
	uint8_t rx_pos;   

    uint8_t rx_len;
    uint8_t tx_len;

	uint32_t virq;

} i2c_handler_t;

/* --------------- VARIABLES --------------- */

static void __iomem *i2c_ptr = NULL;
static void __iomem *clk_ptr = NULL;
static void __iomem *control_module_ptr = NULL;

DEFINE_MUTEX(lock_bus);
DECLARE_WAIT_QUEUE_HEAD(waitq);

static i2c_handler_t hm_i2c;
int waitq_cond;

/* --------------- PROTOTIPOS --------------- */

static int  i2c_is_locked(void);

static void i2c_clear_bufs(void);
static void i2c_set_rxbuf(char size);
static void i2c_set_txbuf(char* data, char size);

static void i2c_power_clock(void);
static void i2c_set_slave(uint8_t slv_addr);
static int  i2c_set_virq(struct platform_device *pdev);

/* --------------- FUNCIONES --------------- */

int i2c_init(struct platform_device *pdev) {
    
	uint32_t reg;
	struct device *i2c_dev = &pdev->dev;

    pr_info("BMP --> %s\n", __func__);

    if (i2c_dev->parent == NULL) {
        pr_err("BMP --> Error, i2c_dev no tiene un dev padre.\n");
        return -1;
    }

	/* ----- mapeo de direcciones base ----- */

    if((clk_ptr = ioremap(CM_PER_BASE, CM_PER_LEN)) == NULL){
        pr_err("BMP --> Error al mapear clk_ptr.\n");
        return -1;
    }

    if((control_module_ptr = ioremap(CTRL_MOD_BASE, CTRL_MOD_LEN)) == NULL){
        pr_err("BMP --> Error al mapear control_module_ptr.\n");
		iounmap(clk_ptr);
		return -1;
    }

    if((i2c_ptr = ioremap(I2C_BASE, I2C_LEN)) == NULL){
        pr_err("BMP --> Error al mapear i2c_ptr.\n");
        iounmap(clk_ptr);
		iounmap(control_module_ptr);
		return -1;
    }

    pr_info("BMP --> %s\n", __func__);
    
	/* ----- configuro pines SDA y SCL ----- */

    reg = ioread32(control_module_ptr + CTRL_MOD_SCL);
	reg &= CTRL_MOD_SCL_MSK;
	reg |= CTRL_MOD_SCL_MODE;
	iowrite32(reg, control_module_ptr + CTRL_MOD_SCL);

	reg = ioread32(control_module_ptr + CTRL_MOD_SDA);
	reg &= CTRL_MOD_SDA_MSK;
	reg |= CTRL_MOD_SDA_MODE;
	iowrite32(reg, control_module_ptr + CTRL_MOD_SDA);

	/* ----- configuro clock ----- */

    i2c_power_clock();

    // deshabilito i2c
    iowrite32(0x0, i2c_ptr + I2C_CON); 

    iowrite32(I2C_PSC_24MHZ, i2c_ptr + I2C_PSC);
    iowrite32(I2C_SCLL_400K, i2c_ptr + I2C_SCLL); 
    iowrite32(I2C_SCLH_400K, i2c_ptr + I2C_SCLH); 

	/* ----- configuro i2c_con ----- */
    
	iowrite32(0x00, i2c_ptr + I2C_SYSC);   
    iowrite32(I2C_CON_ENABLE | I2C_CON_MASTER | I2C_CON_TX, i2c_ptr + I2C_CON);
    
    /* ----- configuro irq ----- */

    if(i2c_set_virq(pdev) != 0){
		return -1;
	}

    /* ----- configuro buffers ----- */

    if ((hm_i2c.rx_buf = (char *) __get_free_page (GFP_KERNEL)) < 0){
        pr_alert("BMP --> Error, no se pudo obtener memoria para rx_buf.\n"); 
		iounmap(i2c_ptr);
		iounmap(clk_ptr);
		iounmap(control_module_ptr);
		free_irq(hm_i2c.virq, NULL);
		return -1;
    }

    if ((hm_i2c.tx_buf = (char *) __get_free_page (GFP_KERNEL)) < 0){
        pr_alert("BMP --> Error, no se pudo obtener memoria para tx_buf.\n");
        free_page((unsigned long)hm_i2c.rx_buf);
		iounmap(i2c_ptr);
		iounmap(clk_ptr);
		iounmap(control_module_ptr);
		free_irq(hm_i2c.virq, NULL);
		return -1;
    }

    pr_info("BMP --> I2C Configurado exitosamente.\n");
    return 0;
}

void i2c_remove(void) {
    
    uint32_t aux;

    // deshabilito I2C
    aux = ioread32(i2c_ptr + I2C_CON);
    aux &= ~(I2C_CON_ENABLE);
    iowrite32(aux, i2c_ptr + I2C_CON);

    // apago el clock
    aux = ioread32(clk_ptr + CM_PER_CLKCTRL); 
    aux &= ~(0x3);
	iowrite32(aux, clk_ptr + CM_PER_CLKCTRL);
    
	if(i2c_ptr != NULL)
        iounmap(i2c_ptr);
	
	if(control_module_ptr != NULL)
        iounmap(control_module_ptr); 
	
	if(clk_ptr != NULL)
        iounmap(clk_ptr);

    free_irq(hm_i2c.virq, NULL);
    free_page((unsigned long)hm_i2c.rx_buf); 
    free_page((unsigned long)hm_i2c.tx_buf);
}

int i2c_write(char slave_address, char* data, char size){

    int retval = -1;
    uint32_t reg;

    if (size == 0){
        pr_err("BMP --> Error, el tamaño no puede ser cero.\n");
        return -1;
    }

	// chequeo que no este lockeado el mutex
    if(i2c_is_locked() != 0) {
        return -1;
    }

    mutex_lock(&lock_bus);

	// prendo el clock, se suele trabar si no lo hago
    i2c_power_clock();
    
    i2c_set_slave(slave_address);
    i2c_set_txbuf(data, size);

    // cargo cantidad de bits a escribir
    iowrite32(hm_i2c.tx_len, i2c_ptr + I2C_CNT);

    // habilito, modo master y tx
    iowrite32(I2C_CON_ENABLE | I2C_CON_MASTER | I2C_CON_TX, i2c_ptr + I2C_CON);

    // limpio flags de IRQ y las deshabilito
    iowrite32(I2C_IRQSTAT_CLR_ALL, i2c_ptr + I2C_IRQENA_CLR); 
    iowrite32(I2C_IRQSTAT_CLR_ALL, i2c_ptr + I2C_IRQSTAT);

    // habilito IRQ de TX
    iowrite32(I2C_IRQ_XRDY, i2c_ptr + I2C_IRQENA_SET);

    // chequeo si el bus esta libre
    while (ioread32(i2c_ptr + I2C_IRQSTAT_RAW) & I2C_IRQ_BB) {
		mdelay(1);
	}

	waitq_cond = 0;

	// start condition
    reg = ioread32(i2c_ptr + I2C_CON); 
    reg |= I2C_CON_START;
    iowrite32(reg, i2c_ptr + I2C_CON);

    wait_event_interruptible (waitq, waitq_cond != 0);

    // stop condition
    reg = ioread32(i2c_ptr + I2C_CON);
    reg &= 0xFFFFFFFE;
    reg |= I2C_CON_STOP;
    iowrite32(reg, i2c_ptr + I2C_CON);

    udelay(100);
    mutex_unlock(&lock_bus);
    
	if (waitq_cond > 0)
        retval = 0;

    return retval;
}

int i2c_read(char slave_address, char* read_buff, char size){

    int retval = -1;
    uint32_t reg;

    if (size == 0){
        pr_err("BMP --> Error, el tamaño no puede ser cero.\n");
        return -1;
    }

    if(i2c_is_locked() != 0) {
        return -1;
    }

    mutex_lock(&lock_bus);

	// prendo el clock, se suele trabar si no lo hago
    i2c_power_clock();

    i2c_set_slave(slave_address);
	i2c_set_rxbuf(size);

	// cargo cantidad de bits a leer
    iowrite32(hm_i2c.rx_len, i2c_ptr + I2C_CNT);

	// habilito, modo master y rx
    iowrite32(I2C_CON_ENABLE | I2C_CON_MASTER, i2c_ptr + I2C_CON);

    // limpio flags de IRQ y las deshabilito
    iowrite32(I2C_IRQSTAT_CLR_ALL, i2c_ptr + I2C_IRQENA_CLR); 
    iowrite32(I2C_IRQSTAT_CLR_ALL, i2c_ptr + I2C_IRQSTAT);
    
    // habilito IRQ de RX
    iowrite32(I2C_IRQ_RRDY, i2c_ptr + I2C_IRQENA_SET);

	// chequeo si el bus esta libre
    while (ioread32(i2c_ptr + I2C_IRQSTAT_RAW) & I2C_IRQ_BB) {
		mdelay(1);
	}

	waitq_cond = 0;

	// start condition
    reg = ioread32(i2c_ptr + I2C_CON); 
    reg |= I2C_CON_START;
    iowrite32(reg, i2c_ptr + I2C_CON);

    wait_event_interruptible (waitq, waitq_cond != 0);

    // stop condition
    reg = ioread32(i2c_ptr + I2C_CON);
    reg &= 0xFFFFFFFE;
    reg |= I2C_CON_STOP;
    iowrite32(reg, i2c_ptr + I2C_CON);

    udelay(100);
    mutex_unlock(&lock_bus);
    
	if (waitq_cond > 0){
        memcpy(read_buff, hm_i2c.rx_buf, size);
        retval = 0;
    }
 
    return retval;
}

static irqreturn_t i2c_handler(int irq_number, void *dev_id) {
    
	int irq = ioread32(i2c_ptr + I2C_IRQSTAT);

	// TX 
    if(irq & I2C_IRQ_XRDY){ 
        
        //pr_info("BMP --> TX: 0x%x\n", hm_i2c.tx_buf[hm_i2c.tx_pos]);
        iowrite32(hm_i2c.tx_buf[hm_i2c.tx_pos++], i2c_ptr + I2C_DATA);
    
        if(hm_i2c.tx_len == hm_i2c.tx_pos){
            iowrite32(I2C_IRQENA_CLR_TX, i2c_ptr + I2C_IRQENA_CLR);
            waitq_cond = 1;
            wake_up_interruptible(&waitq);
        }

        iowrite32(I2C_IRQSTAT_CLR_ALL, i2c_ptr + I2C_IRQSTAT);
    }
	
	// RX
    if(irq & I2C_IRQ_RRDY){
        
        hm_i2c.rx_buf[hm_i2c.rx_pos++] = ioread32(i2c_ptr + I2C_DATA);
        //pr_info("BMP --> RX: 0x%x\n", hm_i2c.rx_buf[hm_i2c.rx_pos -1]);

        if(hm_i2c.rx_len == hm_i2c.rx_pos){ 
            iowrite32(I2C_IRQENA_CLR_RX, i2c_ptr + I2C_IRQENA_CLR);
            waitq_cond = 1;
            wake_up_interruptible(&waitq);
        }

        iowrite32(I2C_IRQSTAT_CLR_ALL, i2c_ptr + I2C_IRQSTAT);
    }  

    return IRQ_HANDLED;
}

/* --------------- FUNCIONES AUXILIARES ---------------*/

static int i2c_set_virq(struct platform_device *pdev){
	
	if ((hm_i2c.virq = platform_get_irq(pdev, 0)) < 0) {
        pr_err("BMP --> Error, no pudo obtenerse numero de IRQ.\n");
        iounmap(i2c_ptr);
		iounmap(clk_ptr);
		iounmap(control_module_ptr);
		return -1;
    }

    if ((request_irq(hm_i2c.virq, (irq_handler_t) i2c_handler, IRQF_TRIGGER_RISING, "liam,i2c", NULL) < 0)){
        pr_err("BMP --> Error, no pudo setearse la IRQ correctamente.\n");
        iounmap(i2c_ptr);
		iounmap(clk_ptr);
		iounmap(control_module_ptr);
		return -1;
    }

	return 0;
}

static void i2c_set_slave(uint8_t addr){
    iowrite32(addr, i2c_ptr + I2C_SA);
}

static void i2c_set_rxbuf(char size){

    i2c_clear_bufs();
    hm_i2c.rx_len = size;
}

static void i2c_set_txbuf(char* data, char size){
	
	i2c_clear_bufs();
    memcpy(hm_i2c.tx_buf, data, size);
    hm_i2c.tx_len = size;
}

static void i2c_power_clock(void){

    uint32_t aux;

    aux = ioread32(clk_ptr + CM_PER_CLKCTRL); 
    aux |= 0x02;
	iowrite32(aux, clk_ptr + CM_PER_CLKCTRL);
    
	while(ioread32(clk_ptr + CM_PER_CLKCTRL) != CM_PER_CLKCTRL_ENA);
}

static int i2c_is_locked(void) {
    
	uint8_t i = 0;

    while(mutex_is_locked(&lock_bus)) {
    
		mdelay(1);
        if (i++ == 100){
            pr_err("BMP --> Timeout, el mutex esta bloqueado.\n");
            return -1;
        }
    }

    return 0;
}

static void i2c_clear_bufs(void){

    memset(hm_i2c.rx_buf, 0, 4096);
    hm_i2c.rx_pos = 0;
    hm_i2c.rx_len = 0;

    memset(hm_i2c.tx_buf, 0, 4096);
    hm_i2c.tx_pos = 0;
    hm_i2c.tx_len = 0;
}