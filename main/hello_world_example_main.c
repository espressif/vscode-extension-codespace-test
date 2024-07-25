#include <unistd.h>
#include "esp_log.h"
#include "sdkconfig.h"

static const char *TAG = "example";

void app_main(void)
{
    while (1) {
        ESP_LOGI(TAG, "Hello, World!");
        sleep(1);
    }
}
