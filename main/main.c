/*
WIFI CASA - PITUBA
CONFIG_WIFI_SSID="LIVE TIM_5501_2G"
CONFIG_WIFI_PASSWORD="2px64kk44x"


WIFI CASA 

CONFIG_WIFI_SSID="NET_2G935B99"
CONFIG_WIFI_PASSWORD="B3935B99"


 * AWS IoT EduKit - Core2 for AWS IoT EduKit
 * Smart Thermostat v1.2.0
 * main.c
 * 
 * Copyright 2010-2015 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * Additions Copyright 2016 Espressif Systems (Shanghai) PTE LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 * A copy of the License is located at
 *
 *  http://aws.amazon.com/apache2.0
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */
/**
 * @file main.c
 * @brief simple MQTT publish, subscribe, and device shadows for use with AWS IoT EduKit reference hardware.
 *
 * This example takes the parameters from the build configuration and establishes a connection to AWS IoT Core over MQTT.
 *
 * Some configuration is required. Visit https://edukit.workshop.aws
 * 
 */


#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <time.h>
#include <math.h> 

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "esp_log.h"

#include "aws_iot_config.h"
#include "aws_iot_log.h"
#include "aws_iot_version.h"
#include "aws_iot_mqtt_client_interface.h"
#include "aws_iot_shadow_interface.h"
#include "I2c_device.h"
#include "core2forAWS.h"
#include "reg.h"


#include "wifi.h"

#include "fft.h"
#include "ds18b20.h" //Include library
#include "driver/gpio.h"

// Temp Sensors are on GPIO26
#define TEMP_BUS 26
#define LED 2
#define HIGH 1
#define LOW 0
#define digitalWrite gpio_set_level
DeviceAddress tempSensors[20];
float temps[20];



static const char *TAG = "MAIN";

#define I2CRELAYADDRESS 0x26
#define I2CHUBADDRESS 0x70
#define I2CADCADDRESS 0x48

#define I2C_NO_REG   ( 1 << 30 )
#define addr 0X26
#define mode_Reg 0X10
#define relay_Reg 0X11

#define HEATING "HEATING"
#define COOLING "COOLING"
#define STANDBY "STANDBY"

#define VOLUME_DETECT_PUMP 10

#define STARTING_HVACSTATUS STANDBY
#define STARTING_ROOMOCCUPANCY false

#define MAX_LENGTH_OF_UPDATE_JSON_BUFFER 2000

#define MAX_TEXTAREA_LENGTH 1024

/* CA Root certificate */
extern const uint8_t aws_root_ca_pem_start[] asm("_binary_aws_root_ca_pem_start");
extern const uint8_t aws_root_ca_pem_end[] asm("_binary_aws_root_ca_pem_end");

/* Default MQTT HOST URL is pulled from the aws_iot_config.h */
char HostAddress[255] = AWS_IOT_MQTT_HOST;

/* Default MQTT port is pulled from the aws_iot_config.h */
uint32_t port = AWS_IOT_MQTT_PORT;


 



/**********************
 *  STATIC VARIABLES
 **********************/

static lv_obj_t *out_txtarea;
static lv_obj_t *wifi_label;
static lv_obj_t *sw_Relay1;
static lv_obj_t *sw_Relay2;
static lv_obj_t *sw_Relay3;
static lv_obj_t *sw_Relay4;
static lv_obj_t *sw_Relay5;
static lv_obj_t *sw_Relay6;
static lv_obj_t *led_bomba;
static lv_obj_t *led_searching;

static lv_obj_t *sw_Relay1_label;
static lv_obj_t *sw_Relay2_label;
static lv_obj_t *sw_Relay3_label;
static lv_obj_t *sw_Relay4_label;
static lv_obj_t *sw_Relay5_label;
static lv_obj_t *sw_Relay6_label;

static lv_obj_t *sensorsearching;

bool Relay1On = false;
bool Relay2On = false;
bool Relay3On = false;
bool Relay4On = false;
bool Relay5On = false;
bool Relay6On = false; 
bool PumpRelay = false; 


static bool shadowUpdateInProgress;

uint8_t n_max_sensors = 6;
bool keepsearching = false;
unsigned int numberFound = 0;



void ui_textarea_prune(size_t new_text_length){
    const char * current_text = lv_textarea_get_text(out_txtarea);
    size_t current_text_len = strlen(current_text);
    if(current_text_len + new_text_length >= MAX_TEXTAREA_LENGTH){
        for(int i = 0; i < new_text_length; i++){
            lv_textarea_set_cursor_pos(out_txtarea, 0);
            lv_textarea_del_char_forward(out_txtarea);
        }
        lv_textarea_set_cursor_pos(out_txtarea, LV_TEXTAREA_CURSOR_LAST);
    }
}

void ui_textarea_add(char *baseTxt, char *param, size_t paramLen) {
    if( baseTxt != NULL ){
        xSemaphoreTake(xGuiSemaphore, portMAX_DELAY);
        if (param != NULL && paramLen != 0){
            size_t baseTxtLen = strlen(baseTxt);
            ui_textarea_prune(paramLen);
            size_t bufLen = baseTxtLen + paramLen;
            char buf[(int) bufLen];
            sprintf(buf, baseTxt, param);
            lv_textarea_add_text(out_txtarea, buf);
        } 
        else{
            lv_textarea_add_text(out_txtarea, baseTxt); 
        }
        xSemaphoreGive(xGuiSemaphore);
    } 
    else{
        ESP_LOGE(TAG, "Textarea baseTxt is NULL!");
    }
}

void iot_subscribe_callback_handler(AWS_IoT_Client *pClient, char *topicName, uint16_t topicNameLen,
                                    IoT_Publish_Message_Params *params, void *pData) {
    ESP_LOGI(TAG, "Subscribe callback");
    ESP_LOGI(TAG, "%.*s\t%.*s", topicNameLen, topicName, (int) params->payloadLen, (char *)params->payload);
}

void disconnect_callback_handler(AWS_IoT_Client *pClient, void *data) {
    ESP_LOGW(TAG, "MQTT Disconnect");
    ui_textarea_add("Disconnected from AWS IoT Core...", NULL, 0);

    IoT_Error_t rc = FAILURE;

    if(NULL == pClient) {
        return;
    }

    if(aws_iot_is_autoreconnect_enabled(pClient)) {
        ESP_LOGI(TAG, "Auto Reconnect is enabled, Reconnecting attempt will start now");
    } else {
        ESP_LOGW(TAG, "Auto Reconnect not enabled. Starting manual reconnect...");
        rc = aws_iot_mqtt_attempt_reconnect(pClient);
        if(NETWORK_RECONNECTED == rc) {
            ESP_LOGW(TAG, "Manual Reconnect Successful");
        } else {
            ESP_LOGW(TAG, "Manual Reconnect Failed - %d", rc);
        }
    }
}

void ShadowUpdateStatusCallback(const char *pThingName, ShadowActions_t action, Shadow_Ack_Status_t status,
                                const char *pReceivedJsonDocument, void *pContextData) {
    IOT_UNUSED(pThingName);
    IOT_UNUSED(action);
    IOT_UNUSED(pReceivedJsonDocument);
    IOT_UNUSED(pContextData);

    shadowUpdateInProgress = false;

    if(SHADOW_ACK_TIMEOUT == status) {
        ESP_LOGE(TAG, "Update timed out");
    } else if(SHADOW_ACK_REJECTED == status) {
        ESP_LOGE(TAG, "Update rejected");
    } else if(SHADOW_ACK_ACCEPTED == status) {
        ESP_LOGI(TAG, "Update accepted");
    }
}

void hvac_Callback(const char *pJsonString, uint32_t JsonStringDataLen, jsonStruct_t *pContext) {
    IOT_UNUSED(pJsonString);
    IOT_UNUSED(JsonStringDataLen);

    char * status = (char *) (pContext->pData);

    if(pContext != NULL) {
        ESP_LOGI(TAG, "Delta - hvacStatus state changed to %s", status);
    }

    if(strcmp(status, HEATING) == 0) {
        ESP_LOGI(TAG, "setting side LEDs to red");
        Core2ForAWS_Sk6812_SetSideColor(SK6812_SIDE_LEFT, 0xFF0000);
        Core2ForAWS_Sk6812_SetSideColor(SK6812_SIDE_RIGHT, 0xFF0000);
        Core2ForAWS_Sk6812_Show();

    } else if(strcmp(status, COOLING) == 0) {
        ESP_LOGI(TAG, "setting side LEDs to blue");
        Core2ForAWS_Sk6812_SetSideColor(SK6812_SIDE_LEFT, 0x0000FF);
        Core2ForAWS_Sk6812_SetSideColor(SK6812_SIDE_RIGHT, 0x0000FF);
        Core2ForAWS_Sk6812_Show();
    } else {
        ESP_LOGI(TAG, "clearing side LEDs");
        Core2ForAWS_Sk6812_Clear();
        Core2ForAWS_Sk6812_Show();
    }
}

void occupancy_Callback(const char *pJsonString, uint32_t JsonStringDataLen, jsonStruct_t *pContext) {
    IOT_UNUSED(pJsonString);
    IOT_UNUSED(JsonStringDataLen);

    if(pContext != NULL) {
        ESP_LOGI(TAG, "Delta - roomOccupancy state changed to %d", *(bool *) (pContext->pData));
    }
}

void ui_recuperacontexto() 
{
    t_rcx_pipa aux_rcx_pipas[6];
    ler_arquivo(aux_rcx_pipas);
    Relay1On = aux_rcx_pipas[0].RelayOn;
    Relay2On = aux_rcx_pipas[1].RelayOn;
    Relay3On = aux_rcx_pipas[2].RelayOn;
    Relay4On = aux_rcx_pipas[3].RelayOn;
    Relay5On = aux_rcx_pipas[4].RelayOn;
    Relay6On = aux_rcx_pipas[5].RelayOn;
	
	// mostrando os elementos do vetor
    int i;
	int len_vet = 6;
	for(i = 0; i < len_vet; i++)
	{
        ESP_LOGE(TAG,"Id: %d", aux_rcx_pipas[i].id);
        ESP_LOGE(TAG,"RelayOn: %x\n", aux_rcx_pipas[i].RelayOn);
		//ESP_LOGE(TAG,"Idatahora_on: %x\n", aux_rcx_pipas[i].datahora_on);
        //ESP_LOGE(TAG,"datahora_off: %x\n\n", aux_rcx_pipas[i].datahora_off);
        ESP_LOGI(TAG,"SensorAddress %d: 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x \n", i+1, aux_rcx_pipas[i].SensorAddressb1,aux_rcx_pipas[i].SensorAddressb2,aux_rcx_pipas[i].SensorAddressb3,aux_rcx_pipas[i].SensorAddressb4,aux_rcx_pipas[i].SensorAddressb5,aux_rcx_pipas[i].SensorAddressb6,aux_rcx_pipas[i].SensorAddressb7,aux_rcx_pipas[i].SensorAddressb8);
        ESP_LOGI(TAG,"Temp. Sensor %d: %0.1fC\n", i,aux_rcx_pipas[i].temperature);
        tempSensors[i][0] = aux_rcx_pipas[i].SensorAddressb1;
        tempSensors[i][1] = aux_rcx_pipas[i].SensorAddressb2;
        tempSensors[i][2] = aux_rcx_pipas[i].SensorAddressb3;
        tempSensors[i][3] = aux_rcx_pipas[i].SensorAddressb4;
        tempSensors[i][4] = aux_rcx_pipas[i].SensorAddressb5;
        tempSensors[i][5] = aux_rcx_pipas[i].SensorAddressb6;
        tempSensors[i][6] = aux_rcx_pipas[i].SensorAddressb7;
        tempSensors[i][7] = aux_rcx_pipas[i].SensorAddressb8;
        temps[i] = aux_rcx_pipas[i].temperature;
	}
    numberFound = 5; 
    falta guardar o contexto de sensores encontrado
    transferir controle de botões em estado inicial de updatetemp para recupera contexto
    if (Relay1On){lv_btn_set_state(sw_Relay1, LV_BTN_STATE_CHECKED_PRESSED);} else {lv_btn_set_state(sw_Relay1, LV_BTN_STATE_RELEASED);};
    if (Relay2On){lv_btn_set_state(sw_Relay2, LV_BTN_STATE_CHECKED_PRESSED);} else {lv_btn_set_state(sw_Relay2, LV_BTN_STATE_RELEASED);};
    if (Relay3On){lv_btn_set_state(sw_Relay3, LV_BTN_STATE_CHECKED_PRESSED);} else {lv_btn_set_state(sw_Relay3, LV_BTN_STATE_RELEASED);};
    if (Relay4On){lv_btn_set_state(sw_Relay4, LV_BTN_STATE_CHECKED_PRESSED);} else {lv_btn_set_state(sw_Relay4, LV_BTN_STATE_RELEASED);};
    if (Relay5On){lv_btn_set_state(sw_Relay5, LV_BTN_STATE_CHECKED_PRESSED);} else {lv_btn_set_state(sw_Relay5, LV_BTN_STATE_RELEASED);};
    if (Relay6On){lv_btn_set_state(sw_Relay6, LV_BTN_STATE_CHECKED_PRESSED);} else {lv_btn_set_state(sw_Relay6, LV_BTN_STATE_RELEASED);};
    
}


void ui_salvacontexto() 
{
    // if (Relay1On){lv_btn_set_state(sw_Relay1, LV_BTN_STATE_CHECKED_PRESSED);} else {lv_btn_set_state(sw_Relay1, LV_BTN_STATE_RELEASED);};
    // if (Relay2On){lv_btn_set_state(sw_Relay2, LV_BTN_STATE_CHECKED_PRESSED);} else {lv_btn_set_state(sw_Relay2,  LV_BTN_STATE_RELEASED);};
    // if (Relay3On){lv_btn_set_state(sw_Relay3, LV_BTN_STATE_CHECKED_PRESSED);} else {lv_btn_set_state(sw_Relay3, LV_BTN_STATE_RELEASED);};
    // if (Relay4On){lv_btn_set_state(sw_Relay4, LV_BTN_STATE_CHECKED_PRESSED);} else {lv_btn_set_state(sw_Relay4, LV_BTN_STATE_RELEASED);};
    // if (Relay5On){lv_btn_set_state(sw_Relay5, LV_BTN_STATE_CHECKED_PRESSED);} else {lv_btn_set_state(sw_Relay5, LV_BTN_STATE_RELEASED);};
    // if (Relay6On){lv_btn_set_state(sw_Relay6, LV_BTN_STATE_CHECKED_PRESSED);} else {lv_btn_set_state(sw_Relay6, LV_BTN_STATE_RELEASED);};
    // vetor que será escrito no arquivo
	t_rcx_pipa rcx_pipas[] = {  {1,Relay1On, 0,0, temps[0], tempSensors[0][0], tempSensors[0][1], tempSensors[0][2], tempSensors[0][3], tempSensors[0][4], tempSensors[0][5], tempSensors[0][6], tempSensors[0][7]}, 
                                {2,Relay2On, 0,0, temps[1], tempSensors[1][0], tempSensors[1][1], tempSensors[1][2], tempSensors[1][3], tempSensors[1][4], tempSensors[1][5], tempSensors[1][6], tempSensors[1][7]}, 
                                {3,Relay3On, 0,0, temps[2], tempSensors[2][0], tempSensors[2][1], tempSensors[2][2], tempSensors[2][3], tempSensors[2][4], tempSensors[2][5], tempSensors[2][6], tempSensors[2][7]},
                                {4,Relay4On, 0,0, temps[3], tempSensors[3][0], tempSensors[3][1], tempSensors[3][2], tempSensors[3][3], tempSensors[3][4], tempSensors[3][5], tempSensors[3][6], tempSensors[3][7]}, 
                                {5,Relay5On, 0,0, temps[4], tempSensors[4][0], tempSensors[4][1], tempSensors[4][2], tempSensors[4][3], tempSensors[4][4], tempSensors[4][5], tempSensors[4][6], tempSensors[4][7]}, 
                                {6,Relay6On, 0,0, temps[5], tempSensors[5][0], tempSensors[5][1], tempSensors[5][2], tempSensors[5][3], tempSensors[5][4], tempSensors[5][5], tempSensors[5][6], tempSensors[5][7]}
                            };
    escrever_arquivo(rcx_pipas);
}


void SendDAtaI2CHub(uint8_t hub_channel, uint8_t my_data)
{
    esp_err_t err;
    I2CDevice_t port_A_peripheral;
    port_A_peripheral = Core2ForAWS_Port_A_I2C_Begin(I2CHUBADDRESS, PORT_A_I2C_STANDARD_BAUD);
    err = Core2ForAWS_Port_A_I2C_Write(port_A_peripheral, I2C_NO_REG, &hub_channel, 1); 
    port_A_peripheral = Core2ForAWS_Port_A_I2C_Begin(I2CRELAYADDRESS, PORT_A_I2C_STANDARD_BAUD);
    err = Core2ForAWS_Port_A_I2C_Write(port_A_peripheral, 0x11, &my_data, 1); 
    ESP_LOGE(TAG,"COMANDO: %d", my_data);
    Core2ForAWS_Port_A_I2C_Close(port_A_peripheral);
}


void ComandarRelay(int numRelay, bool Status){
 uint8_t my_data;
 uint8_t my_data2;
    switch ( numRelay )
  {
    case 1 :
        Relay1On = Status;
        printf("Relay1On\n");
        break;

    case 2 :
        Relay2On = Status;
        printf("Relay2On\n");
        break;

    case 3 :
        Relay3On = Status;
        printf("Relay3On\n");
        break;

    case 4 :
        Relay4On = Status;
        printf("Relay4On\n");
        break;
    case 5 :
        Relay5On = Status;
        printf("Relay5On\n");
        break;
    case 6 :
        Relay6On = Status;
        printf("Relay6On\n");
        break;
    case 7 :
        PumpRelay = Status;
        printf("PumpRelay\n");
        break;
  }
 my_data = 0;
 my_data2 = 0;
 if (Relay1On){my_data = my_data +1;}
 if (Relay2On){my_data = my_data +2;}
 if (Relay3On){my_data = my_data +4;}
 if (Relay4On){my_data = my_data +8;}
 if (Relay5On){my_data2 = my_data2 +1;}
 if (Relay6On){my_data2 = my_data2 +2;}
 if (PumpRelay){my_data2 = my_data2 +4;}


 ui_salvacontexto();

 SendDAtaI2CHub(1,my_data);
 //my_data = my_data << 4;
 SendDAtaI2CHub(2,my_data2);
 
}
void PumpRelayOn_Callback(const char *pJsonString, uint32_t JsonStringDataLen, jsonStruct_t *pContext) {
    IOT_UNUSED(pJsonString);
    IOT_UNUSED(JsonStringDataLen);

    if(pContext != NULL) {
        ESP_LOGI(TAG, "Delta - PumpRelayOn state changed to %d", *(bool *) (pContext->pData));
        ComandarRelay(7, *(bool *) pContext->pData);

    }
}

void Relay1On_Callback(const char *pJsonString, uint32_t JsonStringDataLen, jsonStruct_t *pContext) {
    IOT_UNUSED(pJsonString);
    IOT_UNUSED(JsonStringDataLen);

    if(pContext != NULL) {
        ESP_LOGI(TAG, "Delta - Relay1On state changed to %d", *(bool *) (pContext->pData));
        ComandarRelay(1, *(bool *) pContext->pData);
        
    }
}

void Relay2On_Callback(const char *pJsonString, uint32_t JsonStringDataLen, jsonStruct_t *pContext) {
    IOT_UNUSED(pJsonString);
    IOT_UNUSED(JsonStringDataLen);

    if(pContext != NULL) {
        ESP_LOGI(TAG, "Delta - Relay1On state changed to %d", *(bool *) (pContext->pData));
        ComandarRelay(2, *(bool *) pContext->pData);

    }
}

void Relay3On_Callback(const char *pJsonString, uint32_t JsonStringDataLen, jsonStruct_t *pContext) {
    IOT_UNUSED(pJsonString);
    IOT_UNUSED(JsonStringDataLen);

    if(pContext != NULL) {
        ESP_LOGI(TAG, "Delta - Relay1On state changed to %d", *(bool *) (pContext->pData));
        ComandarRelay(3, *(bool *) pContext->pData);

    }
}

void Relay4On_Callback(const char *pJsonString, uint32_t JsonStringDataLen, jsonStruct_t *pContext) {
    IOT_UNUSED(pJsonString);
    IOT_UNUSED(JsonStringDataLen);

    if(pContext != NULL) {
        ESP_LOGI(TAG, "Delta - Relay1On state changed to %d", *(bool *) (pContext->pData));
        ComandarRelay(4, *(bool *) pContext->pData);
    }
}


void Relay5On_Callback(const char *pJsonString, uint32_t JsonStringDataLen, jsonStruct_t *pContext) {
    IOT_UNUSED(pJsonString);
    IOT_UNUSED(JsonStringDataLen);

    if(pContext != NULL) {
        ESP_LOGI(TAG, "Delta - Relay1On state changed to %d", *(bool *) (pContext->pData));
        ComandarRelay(5, *(bool *) pContext->pData);
    }
}


void Relay6On_Callback(const char *pJsonString, uint32_t JsonStringDataLen, jsonStruct_t *pContext) {
    IOT_UNUSED(pJsonString);
    IOT_UNUSED(JsonStringDataLen);

    if(pContext != NULL) {
        ESP_LOGI(TAG, "Delta - Relay1On state changed to %d", *(bool *) (pContext->pData));
        ComandarRelay(6, *(bool *) pContext->pData);
    }
}
// helper function for working with audio data
long map(long x, long in_min, long in_max, long out_min, long out_max) {
    long divisor = (in_max - in_min);
    if(divisor == 0){
        return -1; //AVR returns -1, SAM returns 0
    }
    return (x - in_min) * (out_max - out_min) / divisor + out_min;
}




void aws_iot_task(void *param) {
    IoT_Error_t rc = FAILURE;

    char JsonDocumentBuffer[MAX_LENGTH_OF_UPDATE_JSON_BUFFER];
    size_t sizeOfJsonDocumentBuffer = sizeof(JsonDocumentBuffer) / sizeof(JsonDocumentBuffer[0]);
    

    jsonStruct_t PumpRelayActuator;
    PumpRelayActuator.cb = PumpRelayOn_Callback;
    PumpRelayActuator.pKey = "PumpRelay";
    PumpRelayActuator.pData = &PumpRelayOn_Callback;
    PumpRelayActuator.type = SHADOW_JSON_BOOL;
    PumpRelayActuator.dataLength = sizeof(bool);

    jsonStruct_t Relay1OnActuator;
    Relay1OnActuator.cb = Relay1On_Callback;
    Relay1OnActuator.pKey = "Relay1On";
    Relay1OnActuator.pData = &Relay1On;
    Relay1OnActuator.type = SHADOW_JSON_BOOL;
    Relay1OnActuator.dataLength = sizeof(bool);
    

    jsonStruct_t Relay2OnActuator;
    Relay2OnActuator.cb = Relay2On_Callback;
    Relay2OnActuator.pKey = "Relay2On";
    Relay2OnActuator.pData = &Relay2On;
    Relay2OnActuator.type = SHADOW_JSON_BOOL;
    Relay2OnActuator.dataLength = sizeof(bool);

    jsonStruct_t Relay3OnActuator;
    Relay3OnActuator.cb = Relay3On_Callback;
    Relay3OnActuator.pKey = "Relay3On";
    Relay3OnActuator.pData = &Relay3On;
    Relay3OnActuator.type = SHADOW_JSON_BOOL;
    Relay3OnActuator.dataLength = sizeof(bool);

    jsonStruct_t Relay4OnActuator;
    Relay4OnActuator.cb = Relay4On_Callback;
    Relay4OnActuator.pKey = "Relay4On";
    Relay4OnActuator.pData = &Relay4On;
    Relay4OnActuator.type = SHADOW_JSON_BOOL;
    Relay4OnActuator.dataLength = sizeof(bool);

    jsonStruct_t Relay5OnActuator;
    Relay5OnActuator.cb = Relay5On_Callback;
    Relay5OnActuator.pKey = "Relay5On";
    Relay5OnActuator.pData = &Relay5On;
    Relay5OnActuator.type = SHADOW_JSON_BOOL;
    Relay5OnActuator.dataLength = sizeof(bool);

    jsonStruct_t Relay6OnActuator;
    Relay6OnActuator.cb = Relay6On_Callback;
    Relay6OnActuator.pKey = "Relay6On";
    Relay6OnActuator.pData = &Relay6On;
    Relay6OnActuator.type = SHADOW_JSON_BOOL;
    Relay6OnActuator.dataLength = sizeof(bool);


    ESP_LOGI(TAG, "AWS IoT SDK Version %d.%d.%d-%s", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH, VERSION_TAG);

    // initialize the mqtt client
    AWS_IoT_Client iotCoreClient;

    ShadowInitParameters_t sp = ShadowInitParametersDefault;
    sp.pHost = HostAddress;
    sp.port = port;
    sp.enableAutoReconnect = false;
    sp.disconnectHandler = disconnect_callback_handler;

    sp.pRootCA = (const char *)aws_root_ca_pem_start;
    sp.pClientCRT = "#";
    sp.pClientKey = "#0";
    
    #define CLIENT_ID_LEN (ATCA_SERIAL_NUM_SIZE * 2)
    char *client_id = malloc(CLIENT_ID_LEN + 1);
    ATCA_STATUS ret = Atecc608_GetSerialString(client_id);
    if (ret != ATCA_SUCCESS){
        ESP_LOGE(TAG, "Failed to get device serial from secure element. Error: %i", ret);
        abort();
    }

    ui_textarea_add("\n\nDevice client Id:\n>> %s <<\n", client_id, CLIENT_ID_LEN);

    /* Wait for WiFI to show as connected */
    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT,
                        false, true, portMAX_DELAY);

    ESP_LOGI(TAG, "Shadow Init");

    rc = aws_iot_shadow_init(&iotCoreClient, &sp);
    ESP_LOGE(TAG, "aws_iot_shadow_init: %d", rc);
    if(SUCCESS != rc) {
        ESP_LOGE(TAG, "aws_iot_shadow_init returned error %d, aborting...", rc);
        abort();
    }

    ShadowConnectParameters_t scp = ShadowConnectParametersDefault;
    scp.pMyThingName = client_id;
    scp.pMqttClientId = client_id;
    scp.mqttClientIdLen = CLIENT_ID_LEN;


    ESP_LOGI(TAG, "Shadow Connect");
    rc = aws_iot_shadow_connect(&iotCoreClient, &scp);
    ESP_LOGE(TAG, "aws_iot_shadow_connect: %d", rc);
 
    if(SUCCESS != rc) {
        ESP_LOGE(TAG, "aws_iot_shadow_connect returned error %d, aborting...", rc);
        abort();
    }
    ui_textarea_add("Connected to AWS IoT Device Shadow service", NULL, 0);


    /*
     * Enable Auto Reconnect functionality. Minimum and Maximum time of Exponential backoff are set in aws_iot_config.h
     *  #AWS_IOT_MQTT_MIN_RECONNECT_WAIT_INTERVAL
     *  #AWS_IOT_MQTT_MAX_RECONNECT_WAIT_INTERVAL
     */
    
    rc = aws_iot_shadow_set_autoreconnect_status(&iotCoreClient, true);
    ESP_LOGE(TAG, "aws_iot_shadow_register_delta: %d", rc);
    if(SUCCESS != rc) {ESP_LOGE(TAG, "Shadow Register Delta Error");}
    // register delta callback for PumpRelay
    rc = aws_iot_shadow_register_delta(&iotCoreClient, &PumpRelayActuator);
    ESP_LOGE(TAG, "aws_iot_shadow_register_delta: %d", rc);
    if(SUCCESS != rc) {ESP_LOGE(TAG, "Shadow Register Delta Error");}
    // register delta callback for Relay1On
    rc = aws_iot_shadow_register_delta(&iotCoreClient, &Relay1OnActuator);
    ESP_LOGE(TAG, "aws_iot_shadow_register_delta: %d", rc);
    if(SUCCESS != rc) {ESP_LOGE(TAG, "Shadow Register Delta Error");}
    // register delta callback for Relay2On
    rc = aws_iot_shadow_register_delta(&iotCoreClient, &Relay2OnActuator);
    ESP_LOGE(TAG, "aws_iot_shadow_register_delta: %d", rc);
    if(SUCCESS != rc) {ESP_LOGE(TAG, "Shadow Register Delta Error");}
    // register delta callback for Relay3On
    rc = aws_iot_shadow_register_delta(&iotCoreClient, &Relay3OnActuator);
    ESP_LOGE(TAG, "aws_iot_shadow_register_delta: %d", rc);
    if(SUCCESS != rc) {ESP_LOGE(TAG, "Shadow Register Delta Error");}
    // register delta callback for Relay4On
    rc = aws_iot_shadow_register_delta(&iotCoreClient, &Relay4OnActuator);
    if(SUCCESS != rc) {ESP_LOGE(TAG, "Shadow Register Delta Error");}
    // register delta callback for Relay5On
    rc = aws_iot_shadow_register_delta(&iotCoreClient, &Relay5OnActuator);
    ESP_LOGE(TAG, "aws_iot_shadow_register_delta: %d", rc);
    if(SUCCESS != rc) {ESP_LOGE(TAG, "Shadow Register Delta Error");}
    // register delta callback for Relay6On
    rc = aws_iot_shadow_register_delta(&iotCoreClient, &Relay6OnActuator);
    ESP_LOGE(TAG, "aws_iot_shadow_register_delta: %d", rc);
    if(SUCCESS != rc) {ESP_LOGE(TAG, "Shadow Register Delta Error");}

    
   

  

    ESP_LOGI(TAG, "loop and publish changes");
    // loop and publish changes
    while(NETWORK_ATTEMPTING_RECONNECT == rc || NETWORK_RECONNECTED == rc || SUCCESS == rc) {


		//	erro em MQTT_RX_BUFFER_TOO_SHORT_ERROR = -32, tentei aumentar o buffer, mas entrou em loop de bug
            

        rc = aws_iot_shadow_yield(&iotCoreClient, 200);
        ESP_LOGE(TAG, "aws_iot_shadow_yield1: %d", rc);
        if(NETWORK_ATTEMPTING_RECONNECT == rc || shadowUpdateInProgress) {
            rc = aws_iot_shadow_yield(&iotCoreClient, 1000);
            ESP_LOGE(TAG, "aws_iot_shadow_yield2: %d", rc);
            // If the client is attempting to reconnect, or already waiting on a shadow update,
            // we will skip the rest of the loop.
            continue;
        }


        

        // END get sensor readings

        ESP_LOGI(TAG, "*****************************************************************************************");
        ESP_LOGI(TAG, "On Device: vave1On %d", Relay1On);
        ESP_LOGI(TAG, "On Device: vave2On %d", Relay2On);
        ESP_LOGI(TAG, "On Device: vave3On %d", Relay3On);
        ESP_LOGI(TAG, "On Device: vave4On %d", Relay4On);
        ESP_LOGI(TAG, "On Device: vave5On %d", Relay5On);
        ESP_LOGI(TAG, "On Device: vave6On %d", Relay6On);
        ESP_LOGI(TAG, "On Device: PumpRelay %d", PumpRelay);

        rc = aws_iot_shadow_init_json_document(JsonDocumentBuffer, sizeOfJsonDocumentBuffer);
        ESP_LOGE(TAG, "aws_iot_shadow_init_json_document: %d", rc);
        if(SUCCESS == rc) {
            rc = aws_iot_shadow_add_reported(JsonDocumentBuffer, sizeOfJsonDocumentBuffer, 10, &PumpRelayActuator,  &Relay1OnActuator, 
                                             &Relay2OnActuator, &Relay3OnActuator, &Relay4OnActuator, &Relay5OnActuator, &Relay6OnActuator);
            ESP_LOGE(TAG, "aws_iot_shadow_add_reported: %d", rc);
            if(SUCCESS == rc) {
                rc = aws_iot_finalize_json_document(JsonDocumentBuffer, sizeOfJsonDocumentBuffer);
                ESP_LOGI(TAG, "aws_iot_finalize_json_document: %d", rc);
                if(SUCCESS == rc) {
                    ESP_LOGI(TAG, "Update Shadow: %s", JsonDocumentBuffer);
                    rc = aws_iot_shadow_update(&iotCoreClient, client_id, JsonDocumentBuffer,
                                                ShadowUpdateStatusCallback, NULL, 4, true);
                    shadowUpdateInProgress = true;
                }
            }
        }
        ESP_LOGI(TAG, "*****************************************************************************************");
        ESP_LOGI(TAG, "Stack remaining for task '%s' is %d bytes", pcTaskGetTaskName(NULL), uxTaskGetStackHighWaterMark(NULL));

        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    if(SUCCESS != rc) {
        ESP_LOGE(TAG, "An error occurred in the loop %d", rc);
    }

    ESP_LOGI(TAG, "Disconnecting");
    rc = aws_iot_shadow_disconnect(&iotCoreClient);
    ESP_LOGI(TAG, "Disconnected");

    if(SUCCESS != rc) {
        ESP_LOGE(TAG, "Disconnect error %d", rc);
    }

    vTaskDelete(NULL);
}

void ui_wifi_label_update(bool state){
    xSemaphoreTake(xGuiSemaphore, portMAX_DELAY);
    if (state == false) {
        lv_label_set_text(wifi_label, LV_SYMBOL_WIFI);
    } 
    else{
        char buffer[25];
        sprintf (buffer, "#0000ff %s #", LV_SYMBOL_WIFI);
        lv_label_set_text(wifi_label, buffer);
    }
    xSemaphoreGive(xGuiSemaphore);
}



void sw_Relay1_cb(lv_obj_t * obj, lv_event_t event)
{
  if(event == LV_EVENT_CLICKED) {
         Relay1On = !Relay1On;
         ComandarRelay(1,Relay1On);

    }
}



void sw_Relay2_cb(lv_obj_t * obj, lv_event_t event)
{
  if(event == LV_EVENT_CLICKED) {
         Relay2On = !Relay2On;
         ComandarRelay(2,Relay2On);

    }
}

void sw_Relay3_cb(lv_obj_t * obj, lv_event_t event)
{
  if(event == LV_EVENT_CLICKED) {
         Relay3On = !Relay3On;
         ComandarRelay(3,Relay3On);
    }     
}
void sw_Relay4_cb(lv_obj_t * obj, lv_event_t event)
{
  if(event == LV_EVENT_CLICKED) {
         Relay4On = !Relay4On;
         ComandarRelay(4,Relay4On);
    }
}
void sw_Relay5_cb(lv_obj_t * obj, lv_event_t event)
{
  if(event == LV_EVENT_CLICKED) {
         Relay5On = !Relay5On;
         ComandarRelay(5,Relay5On);
    }
}
void sw_Relay6_cb(lv_obj_t * obj, lv_event_t event)
{
  if(event == LV_EVENT_CLICKED) {
         Relay6On = !Relay6On;
         ComandarRelay(6,Relay6On);
    }
}

static void event_handler(lv_obj_t * obj, lv_event_t event)
{
    if(event == LV_EVENT_VALUE_CHANGED) {
        printf("Button: %s\n", lv_msgbox_get_active_btn_text(obj));
    }
}

void updateTemp()
{
    int i = 0;
    for(i = 0; i < 6; i++){
        char str1[20]= {""};
        char str2[20]= {""};
        strcat(str1,"S"); 
        sprintf(str2, "%d", i+1);
        strcat(str1, str2);
        strcat(str1,":  ");
        sprintf(str2, "%-4.3g", temps[i]);
        strcat(str1, str2);
        strcat(str1,"°C");
        switch(i) {
            case 0:
                if (temps[i] > -100) 
                {
                    lv_label_set_text(sw_Relay1_label, str1);
                    if (Relay1On)
                        lv_btn_set_state(sw_Relay1,LV_BTN_STATE_CHECKED_PRESSED);
                    else
                        lv_btn_set_state(sw_Relay1,LV_BTN_STATE_RELEASED);
                }
                else 
                { 
                    lv_label_set_text(sw_Relay1_label, "NOK");
                    lv_btn_set_state(sw_Relay1,LV_BTN_STATE_CHECKED_DISABLED);
                }
                break;
            case 1:
               if (temps[i] > -100) 
                {
                    lv_label_set_text(sw_Relay2_label, str1);
                    if (Relay2On)
                        lv_btn_set_state(sw_Relay2,LV_BTN_STATE_CHECKED_PRESSED);
                    else
                        lv_btn_set_state(sw_Relay2,LV_BTN_STATE_RELEASED);
                }
                else 
                { 
                    lv_label_set_text(sw_Relay2_label, "NOK");
                    lv_btn_set_state(sw_Relay2,LV_BTN_STATE_CHECKED_DISABLED);
                }
                break;
            case 2:
                if (temps[i] > -100) 
                {
                    lv_label_set_text(sw_Relay3_label, str1);
                    if (Relay3On)
                        lv_btn_set_state(sw_Relay3,LV_BTN_STATE_CHECKED_PRESSED);
                    else
                        lv_btn_set_state(sw_Relay3,LV_BTN_STATE_RELEASED);
                }
                else 
                { 
                    lv_label_set_text(sw_Relay3_label, "NOK");
                    lv_btn_set_state(sw_Relay3,LV_BTN_STATE_CHECKED_DISABLED);
                }
                break;
            case 3:
                if (temps[i] > -100) 
                {
                    lv_label_set_text(sw_Relay4_label, str1);
                    if (Relay4On)
                        lv_btn_set_state(sw_Relay4,LV_BTN_STATE_CHECKED_PRESSED);
                    else
                        lv_btn_set_state(sw_Relay4,LV_BTN_STATE_RELEASED);
                }
                else 
                { 
                    lv_label_set_text(sw_Relay4_label, "NOK");
                    lv_btn_set_state(sw_Relay4,LV_BTN_STATE_CHECKED_DISABLED);
                }
                break;
            case 4:
                if (temps[i] > -100) 
                {
                    lv_label_set_text(sw_Relay5_label, str1);
                    if (Relay5On)
                        lv_btn_set_state(sw_Relay5,LV_BTN_STATE_CHECKED_PRESSED);
                    else
                        lv_btn_set_state(sw_Relay5,LV_BTN_STATE_RELEASED);
                }
                else 
                { 
                    lv_label_set_text(sw_Relay5_label, "NOK");
                    lv_btn_set_state(sw_Relay5,LV_BTN_STATE_CHECKED_DISABLED);
                }
                break;
            case 5:            
                if (temps[i] > -100) 
                {
                    lv_label_set_text(sw_Relay6_label, str1);
                    if (Relay6On)
                        lv_btn_set_state(sw_Relay6,LV_BTN_STATE_CHECKED_PRESSED);
                    else
                        lv_btn_set_state(sw_Relay6,LV_BTN_STATE_RELEASED);
                }
                else 
                { 
                    lv_label_set_text(sw_Relay6_label, "NOK");
                    lv_btn_set_state(sw_Relay6,LV_BTN_STATE_CHECKED_DISABLED);
                }
                break;
        }   
    }
}



void getTempAddresses(DeviceAddress *tempSensorAddresses) 
{
    numberFound = 0;
    lv_btn_set_state(sensorsearching,LV_BTN_STATE_CHECKED_PRESSED);
    lv_led_on(led_searching);
    vTaskDelay(100 / portTICK_PERIOD_MS);
    lv_led_off(led_searching);
    vTaskDelay(100 / portTICK_PERIOD_MS);
    reset_search();
    while (search(tempSensorAddresses[numberFound],true)) 
    {
        numberFound++;
        // ESP_LOGI(TAG,"numberFound: %d",numberFound);
        if (numberFound == n_max_sensors) {
            keepsearching = false; // se já encontrou todos os n_max_sensors configurados, para a busca
            break;
        }
    }
    int j=0;
    for(j = 0; j < numberFound; j++){
        ESP_LOGI(TAG,"Address %d: 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x \n", j, tempSensors[j][0],tempSensors[j][1],tempSensors[j][2],tempSensors[j][3],tempSensors[j][4],tempSensors[j][5],tempSensors[j][6],tempSensors[j][7]);
    }
    ds18b20_setResolution(tempSensors,numberFound,10);
}

void TaskgetTempAddr(void *param)
//Tasks must be implemented to never return (i.e. continuous loop), or should be terminated using vTaskDelete function.
{
    for(;;){
        if (keepsearching) getTempAddresses(tempSensors);
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void sensorsearching_cb(lv_obj_t * obj, lv_event_t event)
{
  if(event == LV_EVENT_CLICKED) {
    keepsearching = !keepsearching;
  }

}

void tab_log_create(lv_obj_t *parent)
{
    out_txtarea = lv_textarea_create(parent, NULL);
    lv_obj_set_size(out_txtarea, 300, 180);//300
    lv_obj_align(out_txtarea, NULL, LV_ALIGN_IN_BOTTOM_MID, 0, -12);
    lv_textarea_set_max_length(out_txtarea, MAX_TEXTAREA_LENGTH);
    lv_textarea_set_text_sel(out_txtarea, false);
    lv_textarea_set_cursor_hidden(out_txtarea, true);
    lv_textarea_set_text(out_txtarea, "Starting Cloud Connected Blinky\n");

}


void tab_config_create(lv_obj_t *parent)
{
    int x = 10; // posição x dos botões
    int y = 10; //posição y dos botões
    int s = 14; //espaçamento entre botões
    int h = 30; //altura do botão
    int c = 145; //comprimento dos botões
    sensorsearching = lv_btn_create(parent, NULL);      /*Add a button to the current screen*/
    lv_obj_set_pos(sensorsearching, x, y);
	lv_obj_set_size(sensorsearching, c, h);
    //y = y + h + s;
    lv_obj_set_event_cb(sensorsearching, sensorsearching_cb); /*Assign a callback to the button*/
    lv_btn_set_checkable(sensorsearching, true);
    //lv_btn_toggle(sensorsearching);
    lv_btn_set_fit2(sensorsearching, LV_FIT_NONE, LV_FIT_TIGHT);
    lv_obj_t * label11;
    label11 = lv_label_create(sensorsearching, NULL);
    lv_label_set_text(label11, "Buscar Sensores");

    y = y - 7;
    x = x + c + 5;

    led_searching  = lv_led_create(parent, NULL);
    //lv_obj_align(led_searching, NULL, LV_ALIGN_CENTER, 40, 0);
    lv_obj_set_pos(led_searching,  x, y);
    lv_led_off(led_searching);   
}





void tab_cantina_create(lv_obj_t *parent)
{
    int x = 10; // posição x dos botões
    int y = 10; //posição y dos botões
    int s = 14; //espaçamento entre botões
    int h = 30; //altura do botão
    int c = 135; //comprimento dos botões
    sw_Relay1 = lv_btn_create(parent, NULL);      /*Add a button to the current screen*/
    lv_obj_set_pos(sw_Relay1, x, y);
	lv_obj_set_size(sw_Relay1, c, h);
    y = y + h + s;
    lv_obj_set_event_cb(sw_Relay1, sw_Relay1_cb); /*Assign a callback to the button*/
    lv_btn_set_checkable(sw_Relay1, true);
    lv_btn_toggle(sw_Relay1);
    lv_btn_set_fit2(sw_Relay1, LV_FIT_NONE, LV_FIT_TIGHT);
    sw_Relay1_label = lv_label_create(sw_Relay1, NULL);
    lv_label_set_text(sw_Relay1_label, "S1: 00°C");

     

    sw_Relay2 = lv_btn_create(parent, NULL);      /*Add a button to the current screen*/
    lv_obj_set_pos(sw_Relay2, x, y);
	lv_obj_set_size(sw_Relay2, c, h);
    y = y + h + s;
    lv_obj_set_event_cb(sw_Relay2, sw_Relay2_cb); /*Assign a callback to the button*/
    lv_btn_set_checkable(sw_Relay2, true);
    lv_btn_toggle(sw_Relay2);
    lv_btn_set_fit2(sw_Relay2, LV_FIT_NONE, LV_FIT_TIGHT);
    sw_Relay2_label = lv_label_create(sw_Relay2, NULL);
    lv_label_set_text(sw_Relay2_label, "S2: 00°C");

    sw_Relay3 = lv_btn_create(parent, NULL);      /*Add a button to the current screen*/
    lv_obj_set_pos(sw_Relay3, x, y);
	lv_obj_set_size(sw_Relay3, c, h);
    y = y + h + s;
    lv_obj_set_event_cb(sw_Relay3, sw_Relay3_cb); /*Assign a callback to the button*/
    lv_btn_set_checkable(sw_Relay3, true);
    lv_btn_toggle(sw_Relay3);
    lv_btn_set_fit2(sw_Relay3, LV_FIT_NONE, LV_FIT_TIGHT);
    sw_Relay3_label = lv_label_create(sw_Relay3, NULL);
    lv_label_set_text(sw_Relay3_label, "S3: 00°C");

    sw_Relay4 = lv_btn_create(parent, NULL);      /*Add a button to the current screen*/
    lv_obj_set_pos(sw_Relay4, x, y);
	lv_obj_set_size(sw_Relay4, c, h);
    y = y + h + s;
    lv_obj_set_event_cb(sw_Relay4, sw_Relay4_cb); /*Assign a callback to the button*/
    lv_btn_set_checkable(sw_Relay4, true);
    lv_btn_toggle(sw_Relay4);
    lv_btn_set_fit2(sw_Relay4, LV_FIT_NONE, LV_FIT_TIGHT);
    sw_Relay4_label = lv_label_create(sw_Relay4, NULL);
    lv_label_set_text(sw_Relay4_label, "S4 00°C");

    x = 160; // posição x dos botões
    y = 10; //posição y dos botões

    sw_Relay5 = lv_btn_create(parent, NULL);      /*Add a button to the current screen*/
    lv_obj_set_pos(sw_Relay5, x, y);
	lv_obj_set_size(sw_Relay5, c, h);
    y = y + h + s;
    lv_obj_set_event_cb(sw_Relay5, sw_Relay5_cb); /*Assign a callback to the button*/
    lv_btn_set_checkable(sw_Relay5, true);
    lv_btn_toggle(sw_Relay5);
    lv_btn_set_fit2(sw_Relay5, LV_FIT_NONE, LV_FIT_TIGHT);
    sw_Relay5_label = lv_label_create(sw_Relay5, NULL);
    lv_label_set_text(sw_Relay5_label, "S5: 00°C");

    sw_Relay6 = lv_btn_create(parent, NULL);      /*Add a button to the current screen*/
    lv_obj_set_pos(sw_Relay6, x, y);
	lv_obj_set_size(sw_Relay6, c, h);
    y = y + h + s;
    lv_obj_set_event_cb(sw_Relay6, sw_Relay6_cb); /*Assign a callback to the button*/
    lv_btn_set_checkable(sw_Relay6, true);
    lv_btn_toggle(sw_Relay6);
    lv_btn_set_fit2(sw_Relay6, LV_FIT_NONE, LV_FIT_TIGHT);
    sw_Relay6_label = lv_label_create(sw_Relay6, NULL);
    lv_label_set_text(sw_Relay6_label, "S6: 00°C");

    
    y = y + 5;
    x = x + 45;

    led_bomba  = lv_led_create(parent, NULL);
    //lv_obj_align(led_bomba, NULL, LV_ALIGN_CENTER, 40, 0);
    lv_obj_set_pos(led_bomba,  x, y);
    lv_led_off(led_bomba);
    lv_obj_t * label7;
    label7 = lv_label_create(parent, NULL);
    //lv_obj_align(label5, NULL, LV_ALIGN_CENTER, 40, -40);
    y = y + 55;
    x = x - 35+10;
    lv_obj_set_pos(label7,  x, y);
    lv_label_set_text(label7, "BOMBA"); 
    
}



void ui_init() {
    xSemaphoreTake(xGuiSemaphore, portMAX_DELAY);

    lv_obj_t *tabview;
    tabview = lv_tabview_create(lv_scr_act(), NULL);
    /*Add 3 tabs (the tabs are page (lv_page) and can be scrolled*/
    lv_obj_t *tab1 = lv_tabview_add_tab(tabview, "Cantina");
    lv_obj_t *tab2 = lv_tabview_add_tab(tabview, "Config");
    lv_obj_t *tab3 = lv_tabview_add_tab(tabview, "Log");
    /*Add content to the tabs*/
    tab_cantina_create(tab1);
    tab_config_create(tab2);
    tab_log_create(tab3);

    wifi_label = lv_label_create(tabview, NULL);
    lv_obj_align(wifi_label,NULL,LV_ALIGN_IN_TOP_RIGHT, 0, 6);
    lv_label_set_text(wifi_label, LV_SYMBOL_WIFI);
    lv_label_set_recolor(wifi_label, true);


    
    //lv_ex_btn_1();
   /*static const char * btns[] ={"Sim", "Cancelar", ""};

    lv_obj_t * mbox1 = lv_msgbox_create(lv_scr_act(), NULL);
    lv_msgbox_set_text(mbox1, "Tem certeza?");
    lv_msgbox_add_btns(mbox1, btns);
    lv_obj_set_width(mbox1, 200);
    lv_obj_set_event_cb(mbox1, event_handler);
    lv_obj_align(mbox1, NULL, LV_ALIGN_CENTER, 0, 0); */ /*Align to the corner*/
    
    xSemaphoreGive(xGuiSemaphore);
}


void ReadSensors(void *param)
{
    int i; //variável de controle do loop
    //getTempAddresses(tempSensors); // pega os endereços dos sensores
    //keepsearching =  (numberFound < 1); // se não encontrou nenhum sensor conectado, continua procurando
    for(;;){
        ds18b20_requestTemperatures();
        for(i = 0; i < n_max_sensors; i++){
            if (i <= numberFound) 
                temps[i] = ds18b20_getTempC((DeviceAddress *)tempSensors[i]);
            else 
                temps[i] = -100;

            ESP_LOGI(TAG,"Temp. Sensor %d: %0.1fC\n", i,temps[i]);
        }
        updateTemp();
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}


void app_main()
{   
    Core2ForAWS_Init();
    Core2ForAWS_Display_SetBrightness(80);
    Core2ForAWS_LED_Enable(1);
    
    ui_init();
    //ui_salvacontexto();
    
    SPIFFS_File();
    //ui_salvacontexto();
    ui_recuperacontexto();
    
    updateTemp();
    
    //initialise_wifi();

    //xTaskCreatePinnedToCore(&aws_iot_task, "aws_iot_task", 4096*2, NULL, 5, NULL, 1);
    //Core2ForAWS_Port_PinMode(PORT_B_ADC_PIN, ADC);

     gpio_reset_pin(LED);
    /* Set the GPIO as a push/pull output */
    gpio_set_direction(LED, GPIO_MODE_OUTPUT);
	ds18b20_init(TEMP_BUS);
    
    xTaskCreatePinnedToCore(&TaskgetTempAddr, "TaskgetTempAddr", 1024*4, NULL, 6, NULL, 1);
    xTaskCreatePinnedToCore(&ReadSensors,     "ReadSensors"    , 1024*4, NULL, 6, NULL, 1);
    
    //vTaskDelay(4000 / portTICK_PERIOD_MS);
    //vTaskDelete(NULL);
    
}



