#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <string.h>
#include <fcntl.h>

#define TEMP_BIT_1 0
#define TEMP_BIT_0 1

#define PRES_BIT_3 2
#define PRES_BIT_2 3
#define PRES_BIT_1 4
#define PRES_BIT_0 5

#define DEV_PATH	"/dev/bmp180"

int main(void){

	int devd = 0;

	char data[6];
	
	int16_t temp = 0;
	int32_t pres = 0;
	
	printf("Abriendo.\n");
	if((devd = open(DEV_PATH, O_RDWR)) == -1){
		perror("No se pudo abrir el dispositivo.\n");
		return -1;
	}
	printf("Dispositivo abierto.\n");

	// PRIMERA LECTURA
	if(read(devd, data, sizeof(data)) == -1){
		perror("No se pudo realizar la lectura.\n");
		return -1;
	}

	temp = (data[TEMP_BIT_1] << 8) | data[TEMP_BIT_0];
	printf("Lectura 1 --> Temperatura: %i\n", temp);
	
	pres = (data[PRES_BIT_3] << 24) | (data[PRES_BIT_2] << 16) | (data[PRES_BIT_1] << 8) | data[PRES_BIT_0];
	printf("Lectura 1 --> Presion: %i\n", pres);
	
	// SEGUNDA LECTURA
	if(read(devd, data, sizeof(data)) == 0){
		perror("No se pudo realizar la lectura.\n");
		return -1;
	}

	temp = (data[TEMP_BIT_1] << 8) | data[TEMP_BIT_0];
	printf("Lectura 2 --> Temperatura: %i\n", temp);
	
	pres = (data[PRES_BIT_3] << 24) | (data[PRES_BIT_2] << 16) | (data[PRES_BIT_1] << 8) | data[PRES_BIT_0];
	printf("Lectura 2 --> Presion: %i\n", pres);

	// ioctl por ahora no hace nada, pruebo que se llame bien a la funcion
	if(ioctl(devd, 1) != 0){
		perror("Error en ioctl.\n");
	}

	// write no hace nada, pruebo que se llame bien a la funcion
	if(write(devd, data, (size_t)2) == -1){
		perror("Error en write.\n");
	}


	if( read(devd, data, sizeof(data)) == 0){
		perror("No se pudo realizar la lectura.\n");
		return -1;
	}

	temp = (data[TEMP_BIT_1] << 8) | data[TEMP_BIT_0];
	printf("Lectura 3 --> Temperatura: %i\n", temp);
	
	pres = (data[PRES_BIT_3] << 24) | (data[PRES_BIT_2] << 16) | (data[PRES_BIT_1] << 8) | data[PRES_BIT_0];
	printf("Lectura 3 --> Presion: %i\n", pres);
	
	close(devd);
	printf("Saliendo.\n");
	return 0;
}