#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <string.h>
#include <fcntl.h>

#define INDEX_TEMP	0
#define INDEX_PRES	1

#define DEV_PATH	"/dev/bmp180"

int main(void){

	int devd;
	int ret;
	char data[2];

	printf("Abriendo.\n");
	if((devd = open(DEV_PATH, O_RDWR)) == -1){
		perror("No se pudo abrir el dispositivo.\n");
		return -1;
	}

	printf("Dispositivo abierto.\n");

	printf("Realizo primera lectura.\n");
	if(read(devd, data, sizeof(data)) == -1){
		perror("No se pudo realizar la lectura.\n");
		return -1;
	}

	printf("Temperatura: %i\n", data[INDEX_TEMP]);
	printf("Temperatura: %i\n", data[INDEX_PRES]);

	printf("Realizo segunda lectura.\n");
	if(read(devd, data, sizeof(data)) == 0){
		perror("No se pudo realizar la lectura.\n");
		return -1;
	}

	printf("Temperatura: %i\n", data[INDEX_TEMP]);
	printf("Temperatura: %i\n", data[INDEX_PRES]);

	// ioctl por ahora no hace nada
	// solo pruebo que se llame bien a la funcion
	printf("Realizo prueba de ioctl.\n");
	if((ret = ioctl(devd, 1)) != 0){
		perror("Error en ioctl.\n");
	}

	// write no hace nada
	// solo pruebo que se llame bien a la funcion
	printf("Realizo prueba de write.\n");
	if(write(devd, data, (size_t)2) == -1){
		perror("Error en write.\n");
	}

	printf("Realizo tercer lectura.\n");
	if( read(devd, data, sizeof(data)) == 0){
		perror("No se pudo realizar la lectura.\n");
		return -1;
	}
	
	printf("Temperatura: %i\n", data[INDEX_TEMP]);
	printf("Temperatura: %i\n", data[INDEX_PRES]);

	close(devd);
	printf("Saliendo.\n");
	return 0;
}