#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_http_client.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "freertos/event_groups.h"
#include "time.h" // Para manejo de tiempo

static const char *TAG = "ota_example";

#define WIFI_SSID "MiFibra-D96B"          // Reemplaza con tu SSID
#define WIFI_PASS "PCTXV2vr"              // Reemplaza con tu contraseña
#define THINGSBOARD_TOKEN "OhLePMiP1VhGU3QsZWNg" // Reemplaza con el token de tu dispositivo
#define THINGSBOARD_SERVER "http://demo.thingsboard.io" // Cambia si usas tu propio servidor

static EventGroupHandle_t wifi_event_group;
const int CONNECTED_BIT = BIT0;

// Funciones estáticas
static void wifi_init(void);
static void send_data_to_thingsboard(void);
static void check_for_ota_update(void);

// Manejador de eventos Wi-Fi
static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                esp_wifi_connect();
                break;
            case WIFI_EVENT_STA_DISCONNECTED:
                ESP_LOGI(TAG, "Desconectado, reconectando...");
                esp_wifi_connect();
                xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
                break;
            case WIFI_EVENT_STA_CONNECTED:
                ESP_LOGI(TAG, "Conectado al AP");
                break;
        }
    } else if (event_base == IP_EVENT) {
        switch (event_id) {
            case IP_EVENT_STA_GOT_IP:
                ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
                ESP_LOGI(TAG, "Dirección IP obtenida: %s", ip4addr_ntoa(&event->ip_info.ip));
                xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
                break;
        }
    }
}

// Inicializa Wi-Fi
static void wifi_init(void) {
    wifi_event_group = xEventGroupCreate();

    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_mode(WIFI_MODE_STA);

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };

    esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config);
    esp_wifi_start();
    esp_wifi_connect();

    ESP_LOGI(TAG, "Conectando a Wi-Fi...");

    // Esperar hasta conectarse
    EventBits_t bits = xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, 10000 / portTICK_PERIOD_MS);
    if (bits & CONNECTED_BIT) {
        ESP_LOGI(TAG, "Conectado a %s", WIFI_SSID);
    } else {
        ESP_LOGE(TAG, "Fallo en la conexión a Wi-Fi");
    }
}

// Enviar datos a ThingsBoard
static void send_data_to_thingsboard(void) {
    esp_http_client_config_t config = {
        .url = THINGSBOARD_SERVER "/api/v1/" THINGSBOARD_TOKEN "/telemetry",
        .method = HTTP_METHOD_POST,
        .cert_pem = NULL, // Cambia esto si quieres proporcionar un certificado
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);

    // Enviar datos en una sola línea, como en el comando curl
    const char *data = "{\"temperature\": 85}";  // Cambia este valor según sea necesario

    // Establecer el campo del post con el tipo de contenido
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, data, strlen(data)); 

    ESP_LOGI(TAG, "Intentando enviar datos a ThingsBoard...");

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Datos enviados a ThingsBoard correctamente.");
    } else {
        ESP_LOGE(TAG, "Error enviando datos a ThingsBoard: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
}

// Comprobar actualizaciones OTA
static void check_for_ota_update(void) {
    esp_http_client_config_t config = {
        .url = "https://github.com/Hanane-EB/ota/blob/master/build/app-template.bin", // Asegúrate de que esta URL apunte a un archivo .bin accesible
        .cert_pem = NULL, // Cambia esto si quieres proporcionar un certificado
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    ESP_LOGI(TAG, "Iniciando actualización OTA desde %s", config.url);

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Firmware descargado correctamente.");
        // Aquí deberías manejar la actualización del firmware
    } else {
        ESP_LOGE(TAG, "Error en la descarga del firmware: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
}

void app_main(void) {
    // Inicializar NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Registro del manejador de eventos Wi-Fi
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL);

    // Inicializar Wi-Fi
    wifi_init();

    // Comprobar actualizaciones OTA inicialmente
    check_for_ota_update();

    // Enviar datos a ThingsBoard
    send_data_to_thingsboard();

    // Lógica para realizar comprobaciones de OTA cada 2 minutos
    while (true) {
        check_for_ota_update(); // Comprobar actualizaciones OTA
        vTaskDelay(1200 / portTICK_PERIOD_MS); // Esperar 2 minutos
    }
}
