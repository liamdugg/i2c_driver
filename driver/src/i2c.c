#include "../inc/i2c.h"

/* --------------- VARIABLES ---------------*/

static void __iomem *i2c_ptr = NULL;
static void __iomem *clk_ptr = NULL;
static void __iomem *control_module_ptr = NULL;

DEFINE_MUTEX(lock_bus);
DECLARE_WAIT_QUEUE_HEAD(waitq);

// I2C Data structure
static struct i2c_buffers{
    uint8_t * buff_rx;  // Pointer to user data in kernel space
    uint8_t pos_rx;     // Data to be received. This will be updated by the ISR.
    uint8_t buff_rx_len;

    uint8_t * buff_tx;
    uint8_t pos_tx;
    uint8_t buff_tx_len;
} data_i2c;

int waitq_cond;  // Condition to handle the state of the processes.
static int g_irq;       // IRQ number, saved for deinitialization

/* --------------- PROTOTIPOS ---------------*/
static int  i2c_is_busy(void);
static int  i2c_is_locked(void);
static void i2c_set_slave(uint8_t slv_addr);
static void i2c_power_clock(void);
static void i2c_clear_bufs(void);


static void i2c_set_slave(uint8_t addr){
    iowrite32(addr, i2c_ptr + I2C_REG_SA);
}

static void i2c_power_clock(void){

    uint32_t aux;

    //aux = (uint32_t)clk_ptr + (unsigned int)(IDCM_PER_I2C2_CLKCTRL); 
    aux = ioread32(clk_ptr + IDCM_PER_I2C2_CLKCTRL); 
    aux |= 0x02;
    iowrite32(aux, clk_ptr + IDCM_PER_I2C2_CLKCTRL);
    	 while(ioread32(clk_ptr + IDCM_PER_I2C2_CLKCTRL) != CM_PER_I2C2_CLKCTRL_ENABLE);
}

static int i2c_is_locked(void) {
    
	uint8_t i = 0;

    while(mutex_is_locked(&lock_bus)) {
        msleep(1);
        if (i++ == 100) {
            pr_warn("%s: TIMEOUT ERROR: I2C bus is locked.\n", DRIVER_NAME);
            return -1;
        }
    }

    return 0;
}

static void i2c_clear_bufs(void){

    memset(data_i2c.buff_rx, 0, 4096);
    data_i2c.pos_rx = 0;
    data_i2c.buff_rx_len = 0;

    memset(data_i2c.buff_tx, 0, 4096);
    data_i2c.pos_tx = 0;
    data_i2c.buff_tx_len = 0;
}

static irqreturn_t i2c_isr(int irq_number, void *dev_id) {
    
	int irq = ioread32(i2c_ptr + I2C_REG_IRQSTATUS);

	// TX 
    if(irq & I2C_IRQ_XRDY){ 
        
        iowrite32(data_i2c.buff_tx[data_i2c.pos_tx++], i2c_ptr + I2C_REG_DATA);
    
        if(data_i2c.buff_tx_len == data_i2c.pos_tx){
            iowrite32(I2C_IRQENABLE_CLR_TX, i2c_ptr + I2C_REG_IRQENABLE_CLR);
            waitq_cond = 1;
            wake_up_interruptible(&waitq);
        }

        iowrite32(I2C_IRQSTATUS_CLR_ALL, i2c_ptr + I2C_REG_IRQSTATUS);
    }
	
	// RX
    if(irq & I2C_IRQ_RRDY){
        
        data_i2c.buff_rx[data_i2c.pos_rx++] = ioread32(i2c_ptr + I2C_REG_DATA);

        if(data_i2c.buff_rx_len == data_i2c.pos_rx){ 
            iowrite32(I2C_IRQENABLE_CLR_RX, i2c_ptr + I2C_REG_IRQENABLE_CLR);
            waitq_cond = 1;
            wake_up_interruptible(&waitq);
        }

        iowrite32(I2C_IRQSTATUS_CLR_ALL, i2c_ptr + I2C_REG_IRQSTATUS);
    }  

    return IRQ_HANDLED;
}

int i2c_init(struct platform_device *pdev) {
    
	struct device *i2c_dev = &pdev->dev;
    int retval = -1;
	uint32_t reg;

    // Check that parent device exists (should be target-module@9c000)
    if (i2c_dev->parent == NULL) {
        pr_err("I2C device doesn't have a parent.\n");
        goto pdev_error;
    }

    // Mapeo registros

    if((clk_ptr = ioremap(CM_PER, CM_PER_LEN)) == NULL){
        pr_alert("%s: Could not assign memory for clk_ptr (CM_PER).\n", DRIVER_NAME);
        goto pdev_error;
    }

    pr_info("%s: clk_ptr: 0x%X\n", DRIVER_NAME, (unsigned int)clk_ptr);

    if((control_module_ptr = ioremap(CTRL_MODULE_BASE, CTRL_MODULE_LEN)) == NULL){
        pr_alert("%s: Could not assign memory for control_module_ptr (CTRL_MODULE_BASE).\n", DRIVER_NAME);
        goto clk_ptr_error;
    }

    pr_info("%s: control_module_ptr: 0x%X\n", DRIVER_NAME, (unsigned int)control_module_ptr);

    if((i2c_ptr = ioremap(I2C2, I2C2_LEN)) == NULL){
        pr_alert("%s: Could not assign memory for i2c_ptr (I2C2).\n", DRIVER_NAME);
        goto control_module_ptr_error;
    }

    pr_info("%s: i2c_ptr: 0x%X\n", DRIVER_NAME, (unsigned int)i2c_ptr);

    // Pinmux configuration
    reg = ioread32(control_module_ptr + CTRL_MOD_SCL);
	reg &= CTRL_MOD_SCL_MSK;
	reg |= CTRL_MOD_SCL_MODE;
	iowrite32(reg, control_module_ptr + CTRL_MOD_SCL);

	reg = ioread32(control_module_ptr + CTRL_MOD_SDA);
	reg &= CTRL_MOD_SDA_MSK;
	reg |= CTRL_MOD_SDA_MODE;
	iowrite32(reg, control_module_ptr + CTRL_MOD_SDA);

    // Turn ON I2C Clock
    i2c_power_clock();

    // Disable I2C while configuring..
    iowrite32(0x0, i2c_ptr + I2C_REG_CON); 

    // Clock Configuration
    iowrite32(I2C_PSC_24MHZ, i2c_ptr + I2C_REG_PSC);
    iowrite32(I2C_SCLL_400K, i2c_ptr + I2C_REG_SCLL); 
    iowrite32(I2C_SCLH_400K, i2c_ptr + I2C_REG_SCLH); 

    // Force Idle
    iowrite32(0x00, i2c_ptr + I2C_REG_SYSC);   
    
    // Enable I2C device
    iowrite32(I2C_BIT_ENABLE | I2C_BIT_MASTER_MODE | I2C_BIT_TX, // 0x8600
        i2c_ptr + I2C_REG_CON);
    
    // Virtual IRQ request
    if ((g_irq = platform_get_irq(pdev, 0)) < 0) {
        pr_err("%s: Couldn't get I2C IRQ number.\n", DRIVER_NAME);
        goto i2c_ptr_error;
    }

    if ((request_irq(g_irq, (irq_handler_t) i2c_isr, IRQF_TRIGGER_RISING, "lliano,i2c", NULL) < 0)){//pdev->name, NULL)) < 0) {
        pr_err("%s: Couldn't request I2C IRQ.\n", DRIVER_NAME);
        goto i2c_ptr_error;
    }

    // Internal struct init.

    // We ask the kernel for memory for the buffers (4kB)
    if ((data_i2c.buff_rx = (char *) __get_free_page (GFP_KERNEL)) < 0){
        pr_alert("%s: Error while asking por a free page.\n", DRIVER_NAME);
        goto virq_error;
    }

    if ((data_i2c.buff_tx = (char *) __get_free_page (GFP_KERNEL)) < 0){
        pr_alert("%s: Error while asking por a free page.\n", DRIVER_NAME);
        goto virq_error;
    }

    pr_info("I2C successfully configured.\n");
    return 0;

    // Error Handling
    virq_error: free_irq(g_irq, NULL);
    i2c_ptr_error: iounmap(i2c_ptr);
    control_module_ptr_error: iounmap(control_module_ptr);
    clk_ptr_error: iounmap(clk_ptr);
    pdev_error: retval = -1; i2c_ptr = NULL; clk_ptr = NULL; control_module_ptr = NULL;
    return retval;
}

void i2c_deinit(void) {
    
	if(clk_ptr != NULL)
        iounmap(clk_ptr);
	
	if(control_module_ptr != NULL)
        iounmap(control_module_ptr); 
	
	if(i2c_ptr != NULL)
        iounmap(i2c_ptr);

    free_irq(g_irq, NULL);
    free_page((unsigned long)data_i2c.buff_rx); 
    free_page((unsigned long)data_i2c.buff_tx);
}

int i2c_write(char slave_address, char* data, char size){

    int retval = -1;
    int auxReg;

    if (size == 0){
        pr_warn("%s: Write Error: Size should be greater than 0.\n", DRIVER_NAME);
        return retval;
    }

    // Wait until no other process is using it
    if(i2c_is_locked() != 0) {
        return retval;
    }

    // Get control of the bus
    mutex_lock(&lock_bus);

    // Makes sure CLK is running
    i2c_power_clock();
    
    // Set slave address
    i2c_set_slave(slave_address);
    
    // Load the data structures and registers.
    i2c_clear_bufs();
    memcpy(data_i2c.buff_tx, data, size);
    data_i2c.buff_tx_len = size;

    // Load I2C CNT registers
    iowrite32(data_i2c.buff_tx_len, i2c_ptr + I2C_REG_CNT);

    // Sets I2C CONFIG register w/ Master TX (=0x8600)
    iowrite32(I2C_BIT_ENABLE | I2C_BIT_MASTER_MODE | I2C_BIT_TX, i2c_ptr + I2C_REG_CON);

    // Clear IRQ Flags
    iowrite32(I2C_IRQSTATUS_CLR_ALL, i2c_ptr + I2C_REG_IRQENABLE_CLR); 
    iowrite32(I2C_IRQSTATUS_CLR_ALL, i2c_ptr + I2C_REG_IRQSTATUS);

    // Enables ACK interrupt
    iowrite32(I2C_IRQ_XRDY, i2c_ptr + I2C_REG_IRQENABLE_SET);

    // Check irq status (occupied or free)
    while (ioread32(i2c_ptr + I2C_REG_IRQSTATUS_RAW) & I2C_IRQ_BB) {msleep(1);}

    // Sends START
    waitq_cond = 0;     // We need to ensure the condition before sending the start

    auxReg = ioread32(i2c_ptr + I2C_REG_CON); 
    auxReg |= I2C_BIT_START;
    iowrite32(auxReg, i2c_ptr + I2C_REG_CON);

    // Sends process to sleep
    wait_event_interruptible (waitq, waitq_cond != 0);

    // We clear the start bit and sets the stop
    auxReg = ioread32(i2c_ptr + I2C_REG_CON);
    auxReg &= 0xFFFFFFFE;
    auxReg |= I2C_BIT_STOP;
    iowrite32(auxReg, i2c_ptr + I2C_REG_CON);

    // Waits for the core to send the stop and frees the mutex.
    udelay(100); // 100 us
    mutex_unlock(&lock_bus);
    if (waitq_cond > 0)
        retval = 0;

    return retval;
}

int i2c_read(char slave_address, char* read_buff, char size){

    int retval = -1;
    int auxReg;

    if (size == 0){
        pr_warn("%s: Write Error: Size should be greater than 0.\n", DRIVER_NAME);
        return retval;
    }

    // Wait until no other process is using it
    if(i2c_is_locked() != 0) {
        return retval;
    }

    // Get control of the bus
    mutex_lock(&lock_bus);

    // Makes sure CLK is running
    i2c_power_clock();

    // Clear IRQ Flags
    iowrite32(I2C_IRQSTATUS_CLR_ALL, i2c_ptr + I2C_REG_IRQENABLE_CLR); 
    iowrite32(I2C_IRQSTATUS_CLR_ALL, i2c_ptr + I2C_REG_IRQSTATUS);
    
    // Set slave address
    i2c_set_slave(slave_address);
    
    // Load the data structures and registers.
    i2c_clear_bufs();
    data_i2c.buff_rx_len = size;

    // Load I2C DATA & CNT registers
    iowrite32(data_i2c.buff_rx_len, i2c_ptr + I2C_REG_CNT);

    // Sets I2C CONFIG register w/ Master RX (=0x8400)
    iowrite32(I2C_BIT_ENABLE | I2C_BIT_MASTER_MODE, i2c_ptr + I2C_REG_CON); // (RX is enable with 0 at I2C_BIT_TX)

    // Enables ACK interrupt
    iowrite32(I2C_IRQ_RRDY, i2c_ptr + I2C_REG_IRQENABLE_SET);

    // Check irq status (occupied or free)
    while (ioread32(i2c_ptr + I2C_REG_IRQSTATUS_RAW) & I2C_IRQ_BB) {msleep(1);}

    // Sends START
    waitq_cond = 0;

    auxReg = ioread32(i2c_ptr + I2C_REG_CON); 
    auxReg |= I2C_BIT_START;
    iowrite32(auxReg, i2c_ptr + I2C_REG_CON);

    // Sends process to sleep
    wait_event_interruptible (waitq, waitq_cond != 0);

    // We clear the start bit and sets the stop
    auxReg = ioread32(i2c_ptr + I2C_REG_CON);
    auxReg &= 0xFFFFFFFE;
    auxReg |= I2C_BIT_STOP;
    iowrite32(auxReg, i2c_ptr + I2C_REG_CON);

    // Waits for the core to send the stop and frees the mutex.
    udelay(100); // 100 us
    mutex_unlock(&lock_bus);
    
	if (waitq_cond > 0){
        memcpy(read_buff, data_i2c.buff_rx, size);
        retval = 0;
    }
 
    return retval;
}

int i2c_read_reg(char slave_address, char reg_address, char* read_buff){
	
    int retval = -1;
    int auxReg;

    // Wait until no other process is using it
    if(i2c_is_locked() != 0) {
        return retval;
    }

    // Get control of the bus
    mutex_lock(&lock_bus);

    // Makes sure CLK is running
    i2c_power_clock();
    
    // Seteo la direccion del slave
    i2c_set_slave(slave_address);
    
    // Load the data structures and registers.
    i2c_clear_bufs();
    data_i2c.buff_rx_len = 1;

    // Load I2C DATA & CNT registers
    iowrite32(reg_address, i2c_ptr + I2C_REG_DATA);
    iowrite32(1, i2c_ptr + I2C_REG_CNT);

    // Sets I2C CONFIG register w/ Master TX (=0x8600)
    iowrite32(I2C_BIT_ENABLE | I2C_BIT_MASTER_MODE | I2C_BIT_TX, i2c_ptr + I2C_REG_CON);

    // Clear IRQ Flags
    iowrite32(I2C_IRQSTATUS_CLR_ALL, i2c_ptr + I2C_REG_IRQENABLE_CLR); 
    iowrite32(I2C_IRQSTATUS_CLR_ALL, i2c_ptr + I2C_REG_IRQSTATUS);

    // Enables ACK interrupt
    iowrite32(I2C_IRQ_ARDY, i2c_ptr + I2C_REG_IRQENABLE_SET);

    // Check irq status (occupied or free)
    while (ioread32(i2c_ptr + I2C_REG_IRQSTATUS_RAW) & I2C_IRQ_BB) {msleep(1);}

    // Sends START
    waitq_cond = 0; 

    auxReg = ioread32(i2c_ptr + I2C_REG_CON); 
    auxReg |= I2C_BIT_START;
    iowrite32(auxReg, i2c_ptr + I2C_REG_CON);

    // Sends process to sleep
    wait_event_interruptible (waitq, waitq_cond != 0);

    // We clear the start bit and sets the stop
    auxReg = ioread32(i2c_ptr + I2C_REG_CON);
    auxReg &= 0xFFFFFFFE;
    auxReg |= I2C_BIT_STOP;
    iowrite32(auxReg, i2c_ptr + I2C_REG_CON);

    // Waits for the core to send the stop and frees the mutex.
    udelay(100); // 100 us
    mutex_unlock(&lock_bus);
    
	if (waitq_cond > 0){
        memcpy(read_buff, data_i2c.buff_rx, 1);
        retval = 0;
    }

    return retval;
}