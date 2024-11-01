#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_http_client.h"
#include "esp_ota_ops.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "freertos/event_groups.h"

// Configuración de Thingsboard
const char* mqttServer = "demo.thingsboard.io"; // Servidor de Thingsboard
const int mqttPort = 1883;                       // Puerto de MQTT
const char* mqttUsername = "OhLePMiP1VhGU3QsZWNg"; // Token de acceso
const char* otaTopic = "v1/devices/me/telemetry"; // Tema para OTA

static const char *TAG = "ota_example";
#define WIFI_SSID "MiFibra-D96B"           // Reemplaza con tu SSID
#define WIFI_PASS "PCTXV2vr"               // Reemplaza con tu contraseña
#define OTA_URL "http://192.168.1.150:8000/app-template.bin" // URL del firmware

static EventGroupHandle_t wifi_event_group;
const int CONNECTED_BIT = BIT0;

void wifi_init(const char* ssid, const char* password);
static void check_for_updates(void);
static void publish_telemetry(float temperature);

void app_main(void) {
    // Inicializar Wi-Fi
    wifi_init(WIFI_SSID, WIFI_PASS);

    // Comprobar actualizaciones
    check_for_updates();

    // Publicar telemetría como ejemplo
    publish_telemetry(25.0); // Publicar una temperatura de ejemplo
}

void wifi_init(const char* ssid, const char* password) {
    // Inicializar NVS (NAND Flash Storage)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Inicializar Wi-Fi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Configurar la conexión Wi-Fi
    esp_wifi_set_mode(WIFI_MODE_STA); // Establecer modo estación
    wifi_config_t wifi_config = {};
    strncpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char*)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);

    // Asegurarse de que la cadena esté terminada en nulo
    wifi_config.sta.ssid[sizeof(wifi_config.sta.ssid) - 1] = '\0';
    wifi_config.sta.password[sizeof(wifi_config.sta.password) - 1] = '\0';

    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));

    // Iniciar el Wi-Fi
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "Wi-Fi iniciado. Conectando a %s...", ssid);

    // Conectar
    ESP_ERROR_CHECK(esp_wifi_connect());

    // Esperar hasta conectarse
    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, portMAX_DELAY);
}

static void publish_telemetry(float temperature) {
    // Publicar datos de telemetría en Thingsboard
    char payload[50];
    sprintf(payload, "{\"temperature\":%.1f}", temperature);
    
    esp_http_client_config_t config = {
        .url = "http://demo.thingsboard.io/api/v1/OhLePMiP1VhGU3QsZWNg/telemetry", // Cambia el token aquí
        .method = HTTP_METHOD_POST,
        .timeout_ms = 10000, // 10 segundos
        .event_handler = NULL,
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_post_field(client, payload, strlen(payload));

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Telemetry published: %s", payload);
    } else {
        ESP_LOGE(TAG, "Failed to publish telemetry: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
}

static void check_for_updates(void) {
    esp_http_client_config_t config = {
        .url = OTA_URL,
        .cert_pem = NULL, // Si usas HTTPS, añade tu certificado aquí
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    
    // Realizar la solicitud HTTP
    esp_err_t err = esp_http_client_perform(client);
    
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTP Status = %d, content_length = %lld",
                 esp_http_client_get_status_code(client),
                 esp_http_client_get_content_length(client));
    } else {
        ESP_LOGE(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return;
    }

    // Iniciar OTA
    const esp_partition_t *partition = esp_ota_get_next_update_partition(NULL);
    esp_ota_handle_t update_handle = 0;
    ESP_ERROR_CHECK(esp_ota_begin(partition, OTA_SIZE_UNKNOWN, &update_handle));

    // Leer datos y escribir en la nueva partición
    while (true) {
        char buffer[1024] = { 0 };
        int data_read = esp_http_client_read(client, buffer, sizeof(buffer));
        if (data_read <= 0) {
            break; // Fin de los datos
        }
        esp_ota_write(update_handle, buffer, data_read);
    }

    // Finalizar la actualización
    err = esp_ota_end(update_handle);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "OTA completed successfully. Rebooting...");
        esp_restart();
    } else {
        ESP_LOGE(TAG, "OTA end failed, error=%d", err);
    }

    esp_http_client_cleanup(client);
}
