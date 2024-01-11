#include "../inc/bmp.h"
#include "../inc/i2c.h"

static bmp_t bmp;
//static char data[3];

static int		bmp_check_chipid(void);
static uint16_t	bmp_get_uncomp_temp(void);
static uint32_t	bmp_get_uncomp_pres(void);
static void		bmp_get_calib_values(void);
static int		bmp_write_reg(char* send_buf, char size);
static int		bmp_read_reg(char reg_addr, char* store);


int bmp_init(void){

	// lo reinicio
	// if( bmp_write_reg(REG_SOFT_RESET, POWER_ON_RESET, 2)  != 0){
	// 	pr_err("BMP --> Error, no se pudo inicializar el sensor.\n");
	// 	return -EINVAL;
	// }
	
	// chequeo el chip id
	if (bmp_check_chipid() != 0){
		pr_err("BMP --> Error, no se pudo inicializar el sensor.\n");
		return -EINVAL;
	}
	
	//levanto los valores de calibracion
	bmp_get_calib_values();
	return 0;
}

static int bmp_write_reg(char* send_buf, char size){

	// data[0] = reg_addr;
	// data[1] = value;

	if(i2c_write(SLAVE_ADDR, send_buf, size) != 0){
		pr_err("BMP--> Error, no se pudo escribir el registro");
		return -1;
	}

	return 0;
}

static int bmp_read_reg(char reg_addr, char* store){

	// data[0] = reg_addr;
	// data[1] = 0;
	// data[2] = 0;

	char reg = reg_addr;

	if( i2c_write(SLAVE_ADDR, &reg, 1) != 0){
		pr_err("BMP --> Error, no se pudo escribir el registro.\n");
		return -1;
	}

	if (i2c_read(SLAVE_ADDR, store, 1) != 0){
		pr_err("BMP --> Error, no se pudo leer el registro.\n");
		return -1; 
	}

	return 0;
}
 
static void bmp_get_calib_values(void){

	char msb, lsb;

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
	
	bmp_read_reg(REG_MSB_MD, &msb);
	bmp_read_reg(REG_LSB_MD, &lsb);
	bmp.calib.MD = GET_REG_VALUE(msb,lsb);	

	// pr_info("BMP --> Calib AC1: %i\n", bmp.calib.AC1);
	// pr_info("BMP --> Calib AC2: %i\n", bmp.calib.AC2);
	// pr_info("BMP --> Calib AC3: %i\n", bmp.calib.AC3);
	// pr_info("BMP --> Calib AC4: %i\n", bmp.calib.AC4);
	// pr_info("BMP --> Calib AC5: %i\n", bmp.calib.AC5);	
	// pr_info("BMP --> Calib AC6: %i\n", bmp.calib.AC6);
	// pr_info("BMP --> Calib B1:  %i\n", bmp.calib.B1);
	// pr_info("BMP --> Calib B2:  %i\n", bmp.calib.B2);
	// pr_info("BMP --> Calib MC:  %i\n", bmp.calib.MC);
	// pr_info("BMP --> Calib MB:  %i\n", bmp.calib.MB);
	// pr_info("BMP --> Calib MD:  %i\n", bmp.calib.MD);
}

static int bmp_check_chipid(void){

	bmp_read_reg(REG_CHIP_ID, &bmp.chip_id);

	if(bmp.chip_id == CHIP_ID) 
		return 0;

	pr_err("BMP --> Error, chip id incorrecto. \n");
	return -EINVAL;
}

static uint16_t bmp_get_uncomp_temp(void){

	uint8_t ut_msb = 0;
	uint8_t ut_lsb = 0;

	char start[] = {REG_CTRL_MEAS, START_TEMP};

	bmp_write_reg(start, 2); // start de medicion de temp

	mdelay(5);

	bmp_read_reg(REG_OUT_MSB, &ut_msb);
	bmp_read_reg(REG_OUT_LSB, &ut_lsb);

	return GET_REG_VALUE(ut_msb, ut_lsb);
}

int16_t bmp_get_temp(){

	int32_t uncomp_temp = (int32_t)bmp_get_uncomp_temp();
	int32_t x1=0, x2=0;

	x1 = (( uncomp_temp - (int32_t)bmp.calib.AC6 ) * (int32_t)bmp.calib.AC5) >> 15;

	if(x1 == 0 || bmp.calib.MD == 0){
		pr_err("BMP --> Dato invalido del sensor \n");
		return -1;
	}

	x2 = ((int32_t)bmp.calib.MC << 11) / (x1 + bmp.calib.MD);

	bmp.calib.B5 = x1 + x2;
	
	// temp en celsius
	// TODO: parametro para convertir a unidad pedida (hacerlo con ioctl?)
	bmp.temp = ((bmp.calib.B5 + 8) >> 4); 
	
	pr_info("BMP --> La temperatura es %i °C\n", bmp.temp);
	return bmp.temp;
}

static uint32_t bmp_get_uncomp_pres(void){

	uint8_t up_msb = 0; 
	uint8_t up_lsb = 0; 
	uint8_t up_xsb = 0;
	uint32_t up;

	char start[] = {REG_CTRL_MEAS, START_PRES + (OSS_STD << 6)};

	
	// start de medicion de temp (oversampling setting --> standard)
	// TODO: hacer variable el registro oversampling setting
	bmp_write_reg(start, 2);

	mdelay(8); // delay pedido para poder hacer la medicion

	bmp_read_reg(REG_OUT_MSB, &up_msb);
	bmp_read_reg(REG_OUT_LSB, &up_lsb);
	bmp_read_reg(REG_OUT_XLSB, &up_xsb);

	up = (uint32_t)(((uint32_t)up_msb) << 16) | 
	     (uint32_t)(up_lsb<<8) | 
		 (uint32_t)((uint32_t)up_xsb >> (8-OSS_STD));

	// TODO: hacerlo legible
	return up;
}

int32_t bmp_get_pres(void){

	uint32_t uncomp_pres;
	uint32_t b4=0, b7=0;
	int32_t  x1=0, x2=0, x3=0, b3=0, b6=0;

	uncomp_pres = bmp_get_uncomp_pres();

	b6 = bmp.calib.B5 - 4000;

	x1 = (b6*b6) >> 12;
	x1 *= bmp.calib.B2;
	x1 >>= 11;
	x2 = bmp.calib.AC2 * b6;
	x2 >>= 11;
	x3 = x1 + x2;
	b3 = (( (((int32_t)bmp.calib.AC1) * 4 + x3) << OSS_STD ) + 2) >> 2;

	x1 = (bmp.calib.AC3 * b6) >> 13;
	x2 = (bmp.calib.B1 * ((b6*b6) >> 12)) >> 16;
	x3 = ((x1+x2) + 2) >> 2;
	b4 = (bmp.calib.AC4 * (uint32_t)(x3+32768)) >> 15;
	b7 = ((uint32_t)(uncomp_pres - b3) * (50000 >> OSS_STD));

	if (b7 < 0x80000000){

		if (b4 != 0)
			bmp.pres = (b7 << 1) / b4;

		else 
			return -1;
	}

	else {

		if (b4 != 0)
			bmp.pres = (b7 / b4) << 1;

		else return -1;
	}

	x1 = bmp.pres >> 8;
	x1 *= x1;
	x1 = (x1 * 3038) >> 16;
	x2 = (bmp.pres * (-7357)) >> 16;

	// presion en Pa
	bmp.pres += (x1 + x2 + 3791) >> 04;
	pr_info("BMP --> La presion es %i Pa\n", bmp.pres);
	return bmp.pres;
}