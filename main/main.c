#include <string.h>
#include <stdlib.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/timers.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_pm.h"
#include "nvs_flash.h"
#include "esp_http_client.h"
#include "esp_ota_ops.h" // Incluir para las operaciones OTA
#include "cJSON.h" // Asegúrate de tener cJSON instalado

#define WIFI_SSID "MiFibra-D96B" // Cambia esto por el SSID de tu red Wi-Fi
#define WIFI_PASSWORD "PCTXV2vr"   // Cambia esto por la contraseña de tu red Wi-Fi
#define THINGSBOARD_URL "http://demo.thingsboard.io/api/v1/OhLePMiP1VhGU3QsZWNg/telemetry"
#define CHECK_UPDATE_INTERVAL pdMS_TO_TICKS(60000) // Intervalo de verificación de actualización (1 minuto)
#define OTA_URL "https://raw.githubusercontent.com/Hanane-EB/ota/master/build/app-template.bin" // Cambia esto a la URL de tu firmware

// Control de conexión
static bool is_connected = false;

// Función para manejar eventos de Wi-Fi
static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                esp_wifi_connect();
                break;

            case WIFI_EVENT_STA_DISCONNECTED:
                is_connected = false;
                esp_wifi_connect();
                ESP_LOGI("WiFi", "Intentando reconectar...");
                break;

            default:
                break;
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI("WiFi", "IP obtenido: " IPSTR, IP2STR(&event->ip_info.ip));
        is_connected = true;
    }
}

// Función para realizar la actualización OTA
void perform_ota_update(const char *url) {
    ESP_LOGI("OTA", "Iniciando la actualización desde: %s", url);
    
    esp_http_client_config_t config = {
        .url = url,
        .cert_pem = NULL,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);

    // Realizar la actualización OTA
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI("OTA", "Descargando el firmware...");

        // Aquí debes asegurarte de que la respuesta sea correcta
        int content_length = esp_http_client_get_content_length(client);
        if (content_length <= 0) {
            ESP_LOGE("OTA", "Error: Tamaño del contenido no válido.");
            esp_http_client_cleanup(client);
            return;
        }

        // Almacenar el firmware descargado
        esp_ota_handle_t update_handle = 0;
        const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
        if (update_partition == NULL) {
            ESP_LOGE("OTA", "Error: No hay partición para actualizaciones.");
            esp_http_client_cleanup(client);
            return;
        }

        ESP_ERROR_CHECK(esp_ota_begin(update_partition, content_length, &update_handle));

        // Descarga el firmware en bloques
        char *buffer = malloc(4096); // Buffer de 4KB
        if (buffer == NULL) {
            ESP_LOGE("OTA", "Error: No se pudo asignar memoria para el buffer.");
            esp_http_client_cleanup(client);
            return;
        }

        int data_read = 0;
        while ((data_read = esp_http_client_read(client, buffer, sizeof(buffer))) > 0) {
            ESP_LOGI("OTA", "Escribiendo %d bytes a la partición %s", data_read, update_partition->label);
            esp_err_t write_err = esp_ota_write(update_handle, (const void *)buffer, data_read);
            if (write_err != ESP_OK) {
                ESP_LOGE("OTA", "Error al escribir en la partición OTA: %s", esp_err_to_name(write_err));
                free(buffer);
                esp_http_client_cleanup(client);
                return;
            }
        }

        // Finalizar la escritura
        esp_err_t end_err = esp_ota_end(update_handle);
        if (end_err != ESP_OK) {
            ESP_LOGE("OTA", "Error finalizando la actualización OTA: %s", esp_err_to_name(end_err));
        } else {
            ESP_LOGI("OTA", "Actualización OTA completada con éxito. Reiniciando...");

            // Reiniciar el dispositivo
            esp_restart();
        }

        free(buffer);
    } else {
        ESP_LOGE("OTA", "Error en la actualización: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
}

// Función para verificar si hay actualizaciones
void check_for_updates(TimerHandle_t xTimer) {
    if (!is_connected) {
        ESP_LOGI("WiFi", "No hay conexión a la red, no se puede verificar actualizaciones.");
        return;
    }

    ESP_LOGI("OTA", "Verificando actualizaciones de firmware...");
    perform_ota_update(OTA_URL);
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
    esp_wifi_connect();
}

// Función principal
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

    // Crear un temporizador para verificar actualizaciones periódicamente
    TimerHandle_t update_check_timer = xTimerCreate(
        "UpdateCheckTimer",
        CHECK_UPDATE_INTERVAL,
        pdTRUE, // Timer auto-reload
        (void *)0,
        check_for_updates
    );

    if (update_check_timer != NULL) {
        xTimerStart(update_check_timer, 0);
    } else {
        ESP_LOGE("OTA", "Error creando el temporizador de verificación de actualizaciones");
    }
}
