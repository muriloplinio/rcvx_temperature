/*
	Gravar e carregar struct do arquivo
*/
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include "reg.h"
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_spiffs.h"


static const char *TAG = "REG";
static const esp_vfs_spiffs_conf_t conf = {
      .base_path = "",
      .partition_label =  NULL,
      .max_files = 5,
      .format_if_mount_failed = true
    };

static const char *filename = "dados.bin";

bool existearquivo()
{
ESP_LOGI(TAG, "ler_arquivo - Initializing SPIFFS");
    // Use settings defined above to initialize and mount SPIFFS filesystem.
    // Note: esp_vfs_spiffs_register is an all-in-one convenience function.
    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if ((ret != ESP_OK) & (ret != ESP_ERR_INVALID_STATE)) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(conf.partition_label, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }
	
	// abre o arquivo para leitura
	FILE * arq = fopen(filename, "rb");
	if(arq != NULL)
	{
        return(true);
    }
    else
       { puts("Erro: ler_arquivo: abertura do arquivo");
	   return(false);
	   }

}

// função para escrever os elementos de uma struct no arquivo
void  escrever_arquivo(t_rcx_pipa rcx_pipas[])
{
    ESP_LOGI(TAG, "escrever_arquivo - Initializing SPIFFS");


    // Use settings defined above to initialize and mount SPIFFS filesystem.
    // Note: esp_vfs_spiffs_register is an all-in-one convenience function.
    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if ((ret != ESP_OK) & (ret != ESP_ERR_INVALID_STATE)) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret)); 
        }
        return;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(conf.partition_label, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }
	
	
	
	bool a = false;
	int i;
	FILE * arq;

	// abre o arquivo para escrita no modo append (adiciona ao final)
	arq = fopen(filename, "wb");
    if (arq != NULL) {
        i = fwrite(rcx_pipas, sizeof(t_rcx_pipa), MAX, arq);
        if (i == MAX)
            ESP_LOGI(TAG, "Gravacao %d registros com sucesso\n", i);
        else
            ESP_LOGI(TAG, "Foram gravados apenas %d elementos\n", i);
        fclose(arq);
    }
    else
        puts("Erro: escrever_arquivo: abertura do arquivo");

	// All done, unmount partition and disable SPIFFS
     esp_vfs_spiffs_unregister(conf.partition_label);
    ESP_LOGI(TAG, "SPIFFS unmounted");

}


// função para ler do arquivo
// recebe o vetor que ela irá preencher
// retorna a quantidade de elementos preenchidos
int ler_arquivo(t_rcx_pipa rcx_pipas[MAX])
{
	ESP_LOGI(TAG, "ler_arquivo - Initializing SPIFFS");
    // Use settings defined above to initialize and mount SPIFFS filesystem.
    // Note: esp_vfs_spiffs_register is an all-in-one convenience function.
    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if ((ret != ESP_OK) & (ret != ESP_ERR_INVALID_STATE)) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(conf.partition_label, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }
	
	// abre o arquivo para leitura
	FILE * arq = fopen(filename, "rb");
	if(arq != NULL)
	{
        t_rcx_pipa p;
		int ret = fread(rcx_pipas, sizeof(t_rcx_pipa), MAX, arq);
        if (ret == MAX) {
            ESP_LOGI(TAG,"Lidos %d registros com sucesso\n", ret);
        }
        else
            ESP_LOGI(TAG,"Foram lidos apenas %d elementos\n", ret);
        fclose(arq);
    }
    else
       { puts("Erro: ler_arquivo: abertura do arquivo");}
	return(MAX);



	// All done, unmount partition and disable SPIFFS
    esp_vfs_spiffs_unregister(conf.partition_label);
    ESP_LOGI(TAG, "SPIFFS unmounted");
}


void SPIFFS_File (void)
{
    ESP_LOGI(TAG, "Initializing SPIFFS");

    esp_vfs_spiffs_conf_t conf = {
      .base_path = "/spiffs",
      .partition_label = NULL,
      .max_files = 5,
      .format_if_mount_failed = true
    };

    // Use settings defined above to initialize and mount SPIFFS filesystem.
    // Note: esp_vfs_spiffs_register is an all-in-one convenience function.
    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(conf.partition_label, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }

    // Use POSIX and C standard library functions to work with files.
    // First create a file.
    ESP_LOGI(TAG, "Opening file");
    FILE* f = fopen("/spiffs/hello.txt", "w");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing");
        return;
    }
    fprintf(f, "Hello World!\n");
    fclose(f);
    ESP_LOGI(TAG, "File written");

    // Check if destination file exists before renaming
    struct stat st;
    if (stat("/spiffs/foo.txt", &st) == 0) {
        // Delete it if it exists
        unlink("/spiffs/foo.txt");
    }

    // Rename original file
    ESP_LOGI(TAG, "Renaming file");
    if (rename("/spiffs/hello.txt", "/spiffs/foo.txt") != 0) {
        ESP_LOGE(TAG, "Rename failed");
        return;
    }

    // Open renamed file for reading
    ESP_LOGI(TAG, "Reading file");
    f = fopen("/spiffs/foo.txt", "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for reading");
        return;
    }
    char line[64];
    fgets(line, sizeof(line), f);
    fclose(f);
    // strip newline
    char* pos = strchr(line, '\n');
    if (pos) {
        *pos = '\0';
    }
    ESP_LOGI(TAG, "Read from file: '%s'", line);

    // All done, unmount partition and disable SPIFFS
    esp_vfs_spiffs_unregister(conf.partition_label);
    ESP_LOGI(TAG, "SPIFFS unmounted");
}
 



int main2(int argc, char *argv[])
{  
  //variável do tipo time_t para armazenar o tempo em segundos  
  time_t segundos;
  
  //obtendo o tempo em segundos  
  time(&segundos);   
  
  //para converter de segundos para o tempo local  
  //utilizamos a função localtime  
  //ponteiro para struct que armazena data e hora  
  struct tm *data_hora; 
  data_hora = localtime(&segundos);  
    
    // vetor que será escrito no arquivo
	t_rcx_pipa rcx_pipas[] = {{1,0,0}, 
									{2,segundos,segundos}, 
									{3,segundos,segundos},
									{4,segundos,segundos}, 
									{5,segundos,segundos}, 
									{6,segundos,segundos}
									};

	int dd = sizeof(rcx_pipas) / sizeof(rcx_pipas[0]);
	escrever_arquivo(rcx_pipas);

	// vetor para onde serão carregados os dados
	// esse vetor foi criado para demonstrar que realmente funciona,
	// mas basta utilizar somente um vetor
	t_rcx_pipa aux_rcx_pipas[MAX];

	int len_vet = ler_arquivo(aux_rcx_pipas);
	int i;
	// mostrando os elementos do vetor aux_pessoas
	len_vet = 6;
	for(i = 0; i < len_vet; i++)
	{
		data_hora = localtime(&aux_rcx_pipas[i].datahora_on); 
		ESP_LOGI(TAG,"Id: %d\n", aux_rcx_pipas[i].id);
		data_hora = localtime(&aux_rcx_pipas[i].datahora_on); 
		ESP_LOGI(TAG,"Idatahora_on: %d/%d/%d\n", data_hora->tm_mday, data_hora->tm_mon+1, data_hora->tm_year+1900);
        //ESP_LOGI(TAG,"datahora_off: %x\n\n", aux_rcx_pipas[i].datahora_off);
	}
//
	return 0;
}