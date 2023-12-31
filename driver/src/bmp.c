#include "../inc/bmp.h"
#include "../inc/i2c.h"

static bmp_t bmp;
static uint8_t data[2];

static int  bmp_check_chipid(void);
static void bmp_get_calib_values(void);
static int  bmp_write_reg(uint8_t reg_addr, uint8_t value);
static int  bmp_read_reg (uint8_t reg_addr, uint8_t* store);

int bmp_init(void){

	pr_info("BMP --> Ingreso a %s", __func__);

	// lo reinicio
	if( bmp_write_reg(REG_SOFT_RESET, POWER_ON_RESET)  != 0){
		pr_err("BMP --> Error, no se pudo inicializar el sensor.\n");
		return -EINVAL;
	}


	// chequeo el chip id
	if (bmp_check_chipid() != 0){
		pr_err("BMP --> Error, no se pudo inicializar el sensor.\n");
		return -EINVAL;
	}
	
	// levanto los valores de calibracion
	bmp_get_calib_values();*/
	
	return 0;
}

static int bmp_write_reg(uint8_t reg_addr, uint8_t value){

	data[0] = reg_addr;
	data[1] = value;

	if(i2c_write(SLAVE_ADDR, data, 2) != 0){
		pr_err("BMP--> Error, no se pudo escribir el registro");
		return -1;
	}

	return 0;
}

static int bmp_read_reg(uint8_t reg_addr, uint8_t* store){

	data[0] = reg_addr;

	if( i2c_write(SLAVE_ADDR, data, 1) != 0){
		pr_err("BMP --> Error, no se pudo leer el registro.\n");
		return -1;
	}

	if (i2c_read(SLAVE_ADDR, &reg_addr, store, REG_SIZE) != 0){
		pr_err("BMP --> Error, no se pudo leer el registro.\n");
		return -1; 
	}

	return 0;
}
 
void bmp_measure(void){

	long ut, up; 
	long x1, x2, x3;
	long b3, b4, b5, b6, b7;

	// leo la temperatura "raw"
	bmp_write_reg(REG_CTRL_MEAS, START_TEMP);
	bmp_read_reg(REG_OUT_MSB, &bmp.measures.t_msb);
	bmp_read_reg(REG_OUT_LSB, &bmp.measures.t_lsb);

	ut = GET_REG_VALUE(bmp.measures.t_msb, bmp.measures.t_lsb);

	// leo la presion "raw"
	bmp_write_reg(REG_CTRL_MEAS, START_PRES | (OSS_STD << 6));
	bmp_read_reg(REG_OUT_MSB, &bmp.measures.p_msb);
	bmp_read_reg(REG_OUT_LSB, &bmp.measures.p_lsb);
	bmp_read_reg(REG_OUT_XLSB, &bmp.measures.p_xsb);

	up = ((bmp.measures.p_msb << 16) | (bmp.measures.p_lsb << 8) | (bmp.measures.p_xsb)) >> (8-OSS_STD);
	
	x1 = (ut- bmp.calib.AC6)*(bmp.calib.AC5/32768);
	x2 = (bmp.calib.MC * 2048)/(x1 + bmp.calib.MD);
	b5 = x1+x2;

	// obtengo el valor en celsius
	bmp.temp = (b5+8)/16;

	b6 = b5-4000;
	x1 = (bmp.calib.B2*(b6*b6/4096)) /2048;
	x2 = bmp.calib.AC2*b6 / 2048;
	x3 = x1+x2;
	b3 = (((bmp.calib.AC1*4 + x3) << OSS_STD)+2)/4;
	x1 = bmp.calib.AC3 * b6/8192;
	x2 = (bmp.calib.B1 * (b6*b6/4096))/65536;
	x3 = ((x1+x2)+2)/4;
	b4 = bmp.calib.AC4 * (unsigned long)(x3 + 32768)/32768;
	b7 = ((unsigned long)up-b3) * (50000 >> OSS_STD);

	if(b7 < 0x80000000) bmp.pres = (b7*2)/b4;
	else bmp.pres = (b7/b4)*2;

	x1 = (bmp.pres/256)*(bmp.pres/256);
	x1 = (x1 *3038)/65536;
	x2 = (-7357*bmp.pres)/65536;

	bmp.pres += (x1+x2+3791)/16;

	return;
}

static void bmp_get_calib_values(void){

	uint8_t msb, lsb;

	bmp_read_reg(REG_MSB_AC1, &msb);
	bmp_read_reg(REG_LSB_AC1, &lsb);
	bmp.calib.AC1 = GET_REG_VALUE(msb,lsb);
	
	bmp_read_reg(REG_MSB_AC2, &msb);
	bmp_read_reg(REG_LSB_AC2, &lsb);
	bmp.calib.AC2 = GET_REG_VALUE(msb,lsb);
	
	bmp_read_reg(REG_MSB_AC3, &msb);
	bmp_read_reg(REG_LSB_AC3, &lsb);
	bmp.calib.AC3 = GET_REG_VALUE(msb,lsb);
	
	bmp_read_reg(REG_MSB_AC4, &msb);
	bmp_read_reg(REG_LSB_AC4, &lsb);
	bmp.calib.AC4 = GET_REG_VALUE(msb,lsb);
	
	bmp_read_reg(REG_MSB_AC5, &msb);
	bmp_read_reg(REG_LSB_AC5, &lsb);
	bmp.calib.AC5 = GET_REG_VALUE(msb,lsb);
	
	bmp_read_reg(REG_MSB_AC6, &msb);
	bmp_read_reg(REG_LSB_AC6, &lsb);
	bmp.calib.AC6 = GET_REG_VALUE(msb,lsb);
	
	bmp_read_reg(REG_MSB_B1, &msb);
	bmp_read_reg(REG_LSB_B1,  &lsb);
	bmp.calib.B1 = GET_REG_VALUE(msb,lsb);
	
	bmp_read_reg(REG_MSB_B2, &msb);
	bmp_read_reg(REG_LSB_B2,  &lsb);
	bmp.calib.B2 = GET_REG_VALUE(msb,lsb);
	
	bmp_read_reg(REG_MSB_MB, &msb);
	bmp_read_reg(REG_LSB_MB,  &lsb);
	bmp.calib.MB = GET_REG_VALUE(msb,lsb);

	bmp_read_reg(REG_MSB_MC, &msb);
	bmp_read_reg(REG_LSB_MC, &lsb);
	bmp.calib.MC = GET_REG_VALUE(msb,lsb);
	
	bmp_read_reg(REG_LSB_MD, &lsb);
	bmp_read_reg(REG_MSB_MD, &msb);
	bmp.calib.MD = GET_REG_VALUE(msb,lsb);
}

static int bmp_check_chipid(void){

	bmp_read_reg(REG_CHIP_ID, &bmp.chip_id);
	pr_info("BMP --> El chip id es: %x", bmp.chip_id);

	if(bmp.chip_id == CHIP_ID) 
		return 0;

	pr_err("BMP --> Error, chip id incorrecto. \n");
	return -EINVAL;
}

long bmp_get_temp(void){
	return bmp.temp;
}

long bmp_get_pres(void){
	return bmp.pres;
}