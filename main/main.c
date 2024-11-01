#include <string.h>
#include <stdlib.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/timers.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_http_client.h"
#include "esp_ota_ops.h"

#define WIFI_SSID "AndroidAP1477" // Cambia esto por el SSID de tu red Wi-Fi
#define WIFI_PASSWORD "20030036" // Cambia esto por la contraseña de tu red Wi-Fi
#define THINGSBOARD_URL "https://demo.thingsboard.io/api/v1/oz7wJXZiuAxkUrB5gmd0/firmware" // Cambia la URL según sea necesario
#define OTA_URL "https://github.com/Hanane-EB/ota/raw/master/build/app-template.bin" // URL del firmware
#define CONTENT_TYPE "application/json"

#define CHECK_INTERVAL pdMS_TO_TICKS(60000) // Intervalo de chequeo de 1 minuto

static bool is_connected = false;

// Función para manejar los eventos Wi-Fi e IP
static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                esp_wifi_connect();
                break;

            case WIFI_EVENT_STA_DISCONNECTED:
                is_connected = false; // Desconectar
                esp_wifi_connect(); // Intentar reconectar
                ESP_LOGI("WiFi", "Intentando reconectar...");
                break;

            default:
                break;
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
        ESP_LOGI("WiFi", "IP obtenido: " IPSTR, IP2STR(&event->ip_info.ip));
        is_connected = true; // Conectado
    }
}

// Función para realizar la actualización OTA
void perform_ota_update(const char *ota_url) {
    ESP_LOGI("OTA", "Iniciando actualización OTA desde %s", ota_url);

    esp_http_client_config_t config = {
        .url = ota_url,
        .cert_pem = NULL, // Cambia esto si necesitas verificación de certificado
        .event_handler = NULL
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        ESP_LOGI("OTA", "OTA realizada con éxito");
    } else {
        ESP_LOGE("OTA", "Error en OTA: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
}

// Inicializa Wi-Fi
static void wifi_init(void) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD,
        },
    };

    ESP_LOGI("WiFi", "Conectando al AP: %s", wifi_config.sta.ssid);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

// Función para verificar actualizaciones de firmware
void check_for_updates(TimerHandle_t xTimer) {
    if (!is_connected) {
        ESP_LOGI("WiFi", "No hay conexión a la red, no se puede verificar actualizaciones.");
        return;
    }

    // Aquí se llamaría a la API de ThingsBoard para verificar si hay una nueva versión
    // y obtener la URL del firmware a actualizar.

    // Por simplicidad, se está utilizando una URL de OTA fija. Debes reemplazarla con la URL obtenida de ThingsBoard.
    perform_ota_update(OTA_URL);
}

void app_main(void) {
    // Inicializar NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Inicializar Wi-Fi
    wifi_init();

    // Crear un temporizador para verificar actualizaciones cada minuto
    TimerHandle_t update_check_timer = xTimerCreate(
        "UpdateCheckTimer",
        CHECK_INTERVAL, // Cada 1 minuto
        pdTRUE, // Auto-reload
        (void *) 0,
        check_for_updates
    );

    if (update_check_timer != NULL) {
        xTimerStart(update_check_timer, 0);
    } else {
        ESP_LOGE("OTA", "Error creando el temporizador de verificación de actualizaciones");
    }
}
