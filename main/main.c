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
#include "esp_ota_ops.h"

#define WIFI_SSID "MiFibra-D96B" // Cambia esto por el SSID de tu red Wi-Fi
#define WIFI_PASSWORD "PCTXV2vr"   // Cambia esto por la contraseña de tu red Wi-Fi
#define CHECK_UPDATE_INTERVAL pdMS_TO_TICKS(60000) // Intervalo de verificación de actualización (1 minuto)
#define OTA_URL "https://raw.githubusercontent.com/Hanane-EB/ota/master/build/app-template.bin" // Cambia esto a la URL de tu firmware

// Control de conexión
static bool is_connected = false;

// Certificado de seguridad (ejemplo, asegúrate de usar el correcto)
const char *cert_pem = \
"-----BEGIN CERTIFICATE-----\n"
"MIIEozCCBEmgAwIBAgIQTij3hrZsGjuULNLEDrdCpTAKBggqhkjOPQQDAjCBjzEL\n"
"MAkGA1UEBhMCR0IxGzAZBgNVBAgTEkdyZWF0ZXIgTWFuY2hlc3RlcjEQMA4GA1UE\n"
"BxMHU2FsZm9yZDEYMBYGA1UEChMPU2VjdGlnbyBMaW1pdGVkMTcwNQYDVQQDEy5T\n"
"ZWN0aWdvIEVDQyBEb21haW4gVmFsaWRhdGlvbiBTZWN1cmUgU2VydmVyIENBMB4X\n"
"DTI0MDMwNzAwMDAwMFoXDTI1MDMwNzIzNTk1OVowFTETMBEGA1UEAxMKZ2l0aHVi\n"
"LmNvbTBZMBMGByqGSM49AgEGCCqGSM49AwEHA0IABARO/Ho9XdkY1qh9mAgjOUkW\n"
"mXTb05jgRulKciMVBuKB3ZHexvCdyoiCRHEMBfFXoZhWkQVMogNLo/lW215X3pGj\n"
"ggL+MIIC+jAfBgNVHSMEGDAWgBT2hQo7EYbhBH0Oqgss0u7MZHt7rjAdBgNVHQ4E\n"
"FgQUO2g/NDr1RzTK76ZOPZq9Xm56zJ8wDgYDVR0PAQH/BAQDAgeAMAwGA1UdEwEB\n"
"/wQCMAAwHQYDVR0lBBYwFAYIKwYBBQUHAwEGCCsGAQUFBwMCMEkGA1UdIARCMEAw\n"
"NAYLKwYBBAGyMQECAgcwJTAjBggrBgEFBQcCARYXaHR0cHM6Ly9zZWN0aWdvLmNv\n"
"bS9DUFMwCAYGZ4EMAQIBMIGEBggrBgEFBQcBAQR4MHYwTwYIKwYBBQUHMAKGQ2h0\n"
"dHA6Ly9jcnQuc2VjdGlnby5jb20vU2VjdGlnb0VDQ0RvbWFpblZhbGlkYXRpb25T\n"
"ZWN1cmVTZXJ2ZXJDQS5jcnQwIwYIKwYBBQUHMAGGF2h0dHA6Ly9vY3NwLnNlY3Rp\n"
"Z28uY29tMIIBgAYKKwYBBAHWeQIEAgSCAXAEggFsAWoAdwDPEVbu1S58r/OHW9lp\n"
"LpvpGnFnSrAX7KwB0lt3zsw7CAAAAY4WOvAZAAAEAwBIMEYCIQD7oNz/2oO8VGaW\n"
"WrqrsBQBzQH0hRhMLm11oeMpg1fNawIhAKWc0q7Z+mxDVYV/6ov7f/i0H/aAcHSC\n"
"Ii/QJcECraOpAHYAouMK5EXvva2bfjjtR2d3U9eCW4SU1yteGyzEuVCkR+cAAAGO\n"
"Fjrv+AAABAMARzBFAiEAyupEIVAMk0c8BVVpF0QbisfoEwy5xJQKQOe8EvMU4W8C\n"
"IGAIIuzjxBFlHpkqcsa7UZy24y/B6xZnktUw/Ne5q5hCAHcATnWjJ1yaEMM4W2zU\n"
"3z9S6x3w4I4bjWnAsfpksWKaOd8AAAGOFjrv9wAABAMASDBGAiEA+8OvQzpgRf31\n"
"uLBsCE8ktCUfvsiRT7zWSqeXliA09TUCIQDcB7Xn97aEDMBKXIbdm5KZ9GjvRyoF\n"
"9skD5/4GneoMWzAlBgNVHREEHjAcggpnaXRodWIuY29tgg53d3cuZ2l0aHViLmNv\n"
"bTAKBggqhkjOPQQDAgNIADBFAiEAru2McPr0eNwcWNuDEY0a/rGzXRfRrm+6XfZe\n"
"SzhYZewCIBq4TUEBCgapv7xvAtRKdVdi/b4m36Uyej1ggyJsiesA\n"
"-----END CERTIFICATE-----\n";

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
        .cert_pem = cert_pem, // Incluir el certificado
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);

    // Realizar la actualización OTA
    esp_err_t err = esp_http_client_perform(client);
    if (err != ESP_OK) {
        ESP_LOGE("OTA", "Error en la conexión HTTP: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return;
    }

    ESP_LOGI("OTA", "Descargando el firmware...");

    int content_length = esp_http_client_get_content_length(client);
    if (content_length <= 0) {
        ESP_LOGE("OTA", "Error: Tamaño del contenido no válido.");
        esp_http_client_cleanup(client);
        return;
    }

    esp_ota_handle_t update_handle = 0;
    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
    if (update_partition == NULL) {
        ESP_LOGE("OTA", "Error: No hay partición para actualizaciones.");
        esp_http_client_cleanup(client);
        return;
    }

    ESP_ERROR_CHECK(esp_ota_begin(update_partition, content_length, &update_handle));

    char buffer[4096]; // Buffer de 4KB
    int data_read = 0;
    while ((data_read = esp_http_client_read(client, buffer, sizeof(buffer))) > 0) {
        ESP_LOGI("OTA", "Escribiendo %d bytes a la partición %s", data_read, update_partition->label);
        esp_err_t write_err = esp_ota_write(update_handle, (const void *)buffer, data_read);
        if (write_err != ESP_OK) {
            ESP_LOGE("OTA", "Error al escribir en la partición OTA: %s", esp_err_to_name(write_err));
            esp_http_client_cleanup(client);
            return;
        }
    }

    esp_err_t end_err = esp_ota_end(update_handle);
    if (end_err != ESP_OK) {
        ESP_LOGE("OTA", "Error finalizando la actualización OTA: %s", esp_err_to_name(end_err));
    } else {
        ESP_LOGI("OTA", "Actualización OTA completada con éxito. Reiniciando...");
        esp_restart();
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
