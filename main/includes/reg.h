#ifndef REG_H
#define REG_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>


#define MAX 6


// declaração da struct 
typedef struct rcx_pipa
{
	int id;
    bool RelayOn;
    time_t datahora_on;
    time_t datahora_off;
    float temperature;
    uint8_t SensorAddressb1;
    uint8_t SensorAddressb2;
    uint8_t SensorAddressb3;
    uint8_t SensorAddressb4;
    uint8_t SensorAddressb5;
    uint8_t SensorAddressb6;
    uint8_t SensorAddressb7;
    uint8_t SensorAddressb8;
} t_rcx_pipa;



void escrever_arquivo(t_rcx_pipa rcx_pipas[]);
int ler_arquivo(t_rcx_pipa rcx_pipas[MAX]);
void SPIFFS_File (void);

#endif