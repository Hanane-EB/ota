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
#include "cJSON.h" // Incluir cJSON para manejar JSON

#define WIFI_SSID "MiFibra-D96B" // Cambia esto por el SSID de tu red Wi-Fi
#define WIFI_PASSWORD "PCTXV2vr"   // Cambia esto por la contraseña de tu red Wi-Fi
#define THINGSBOARD_URL "http://demo.thingsboard.io/api/v1/OhLePMiP1VhGU3QsZWNg/telemetry" // Cambia esto por tu URL de Thingsboard
#define OTA_URL "https://example.com/your-firmware.bin" // Cambia a tu URL real para OTA

// Control de conexión
static bool is_connected = false;

// Manejo de eventos Wi-Fi e IP
static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
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
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI("WiFi", "IP obtenido: " IPSTR, IP2STR(&event->ip_info.ip));
        is_connected = true; // Conectado
    }
}

// Función para realizar actualizaciones OTA
void perform_ota_update(const char *url) {
    ESP_LOGI("OTA", "Iniciando actualización OTA desde: %s", url);
    // Implementación de la actualización OTA aquí
}

// Función para verificar actualizaciones
void check_for_updates(TimerHandle_t xTimer) {
    if (!is_connected) {
        ESP_LOGI("WiFi", "No hay conexión a la red, no se puede verificar actualizaciones.");
        return;
    }

    ESP_LOGI("OTA", "Verificando actualizaciones de firmware...");

    esp_http_client_config_t config = {
        .url = THINGSBOARD_URL,
        .method = HTTP_METHOD_GET,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        if (status_code == 200) {
            char response[512]; // Buffer para la respuesta
            esp_http_client_read(client, response, sizeof(response) - 1);
            response[sizeof(response) - 1] = '\0'; // Asegura terminación nula

            // Procesar la respuesta JSON
            cJSON *json = cJSON_Parse(response);
            if (json != NULL) {
                cJSON *url = cJSON_GetObjectItemCaseSensitive(json, "url");
                if (cJSON_IsString(url) && (url->valuestring != NULL)) {
                    perform_ota_update(url->valuestring);
                } else {
                    ESP_LOGE("OTA", "URL de firmware no encontrada o no es una cadena.");
                }
                cJSON_Delete(json);
            } else {
                ESP_LOGE("OTA", "Error al analizar la respuesta JSON: %s", cJSON_GetErrorPtr());
            }
        } else {
            ESP_LOGE("OTA", "Error al obtener actualizaciones: %d", status_code);
        }
    } else {
        ESP_LOGE("OTA", "Error en la solicitud: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
}

// Función para inicializar Wi-Fi
static void wifi_init(void)
{
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
            .listen_interval = 10,
        },
    };

    ESP_LOGI("WiFi", "Conectando al AP: %s", wifi_config.sta.ssid);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

// Función de temporizador para verificar actualizaciones periódicamente
void check_updates_timer_callback(TimerHandle_t xTimer) {
    check_for_updates(xTimer);
}

void app_main(void)
{
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
    TimerHandle_t update_timer = xTimerCreate(
        "UpdateTimer",
        pdMS_TO_TICKS(60000), // Cada 60 segundos
        pdTRUE, // Auto-reload
        NULL,
        check_updates_timer_callback
    );

    if (update_timer != NULL) {
        xTimerStart(update_timer, 0);
    } else {
        ESP_LOGE("OTA", "Error creando el temporizador de verificación de actualizaciones");
    }
}
