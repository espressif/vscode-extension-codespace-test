/*
   glowswitch confidential
   glowswitch-wifi-matter-0.1
*/

#include <esp_err.h>
#include <esp_log.h>
#include <esp_wifi.h>
#include <nvs_flash.h>

#include <esp_matter.h>
#include <esp_matter_console.h>
#include <esp_matter_ota.h>

#include <common_macros.h>
#include <log_heap_numbers.h>

#include <app_priv.h>
#include <app_reset.h>
#if CHIP_DEVICE_CONFIG_ENABLE_THREAD
#include <platform/ESP32/OpenthreadLauncher.h>
#endif

#include <app/server/CommissioningWindowManager.h>
#include <app/server/Server.h>

#ifdef CONFIG_ENABLE_SET_CERT_DECLARATION_API
#include <esp_matter_providers.h>
#include <lib/support/Span.h>
#ifdef CONFIG_SEC_CERT_DAC_PROVIDER
#include <platform/ESP32/ESP32SecureCertDACProvider.h>
#elif defined(CONFIG_FACTORY_PARTITION_DAC_PROVIDER)
#include <platform/ESP32/ESP32FactoryDataProvider.h>
#endif
using namespace chip::DeviceLayer;
#endif

static const char *TAG = "app_main";
uint16_t light_endpoint_id = 0;
uint16_t invert_switch_endpoint_id = 0;

static bool invert_switch_state = false;
constexpr uint32_t kVendorClusterId = 0xFFF1FC10;
constexpr uint32_t kInvertAttributeId = 0x0000;


using namespace esp_matter;
using namespace esp_matter::attribute;
using namespace esp_matter::endpoint;
using namespace chip::app::Clusters;

constexpr auto k_timeout_seconds = 300;

#ifdef CONFIG_ENABLE_SET_CERT_DECLARATION_API
extern const uint8_t cd_start[] asm("_binary_certification_declaration_der_start");
extern const uint8_t cd_end[] asm("_binary_certification_declaration_der_end");

const chip::ByteSpan cdSpan(cd_start, static_cast<size_t>(cd_end - cd_start));
#endif // CONFIG_ENABLE_SET_CERT_DECLARATION_API

#if CONFIG_ENABLE_ENCRYPTED_OTA
extern const char decryption_key_start[] asm("_binary_esp_image_encryption_key_pem_start");
extern const char decryption_key_end[] asm("_binary_esp_image_encryption_key_pem_end");

static const char *s_decryption_key = decryption_key_start;
static const uint16_t s_decryption_key_len = decryption_key_end - decryption_key_start;
#endif // CONFIG_ENABLE_ENCRYPTED_OTA

static void app_event_cb(const ChipDeviceEvent *event, intptr_t arg)
{
    switch (event->Type) {
    case chip::DeviceLayer::DeviceEventType::kInterfaceIpAddressChanged:
        ESP_LOGI(TAG, "Interface IP Address changed");
        break;

    case chip::DeviceLayer::DeviceEventType::kCommissioningComplete:
        ESP_LOGI(TAG, "Commissioning complete");
        MEMORY_PROFILER_DUMP_HEAP_STAT("commissioning complete");
        break;

    case chip::DeviceLayer::DeviceEventType::kFailSafeTimerExpired:
        ESP_LOGI(TAG, "Commissioning failed, fail safe timer expired");
        break;

    case chip::DeviceLayer::DeviceEventType::kCommissioningSessionStarted:
        ESP_LOGI(TAG, "Commissioning session started");
        break;

    case chip::DeviceLayer::DeviceEventType::kCommissioningSessionStopped:
        ESP_LOGI(TAG, "Commissioning session stopped");
        break;

    case chip::DeviceLayer::DeviceEventType::kCommissioningWindowOpened:
        ESP_LOGI(TAG, "Commissioning window opened");
        MEMORY_PROFILER_DUMP_HEAP_STAT("commissioning window opened");
        break;

    case chip::DeviceLayer::DeviceEventType::kCommissioningWindowClosed:
        ESP_LOGI(TAG, "Commissioning window closed");
        break;

    case chip::DeviceLayer::DeviceEventType::kFabricRemoved:
        {
            ESP_LOGI(TAG, "Fabric removed successfully");
            if (chip::Server::GetInstance().GetFabricTable().FabricCount() == 0)
            {
            ESP_LOGI(TAG, "Factory Resetting.");
              chip::DeviceLayer::ConfigurationMgr().InitiateFactoryReset();
            }
        break;
        }

    case chip::DeviceLayer::DeviceEventType::kFabricWillBeRemoved:
        ESP_LOGI(TAG, "Fabric will be removed");
        break;

    case chip::DeviceLayer::DeviceEventType::kFabricUpdated:
        ESP_LOGI(TAG, "Fabric is updated");
        break;

    case chip::DeviceLayer::DeviceEventType::kFabricCommitted:
        ESP_LOGI(TAG, "Fabric is committed");
        break;

    case chip::DeviceLayer::DeviceEventType::kBLEDeinitialized:
        ESP_LOGI(TAG, "BLE deinitialized and memory reclaimed");
        MEMORY_PROFILER_DUMP_HEAP_STAT("BLE deinitialized");
        break;

    default:
        break;
    }
}

// This callback is invoked when clients interact with the Identify Cluster.
// In the callback implementation, an endpoint can identify itself. (e.g., by flashing an LED or light).
static esp_err_t app_identification_cb(identification::callback_type_t type, uint16_t endpoint_id, uint8_t effect_id,
                                       uint8_t effect_variant, void *priv_data)
{
    ESP_LOGI(TAG, "Identification callback: type: %u, effect: %u, variant: %u", type, effect_id, effect_variant);
    return ESP_OK;
}

// This callback is called for every attribute update. The callback implementation shall
// handle the desired attributes and return an appropriate error code. If the attribute
// is not of your interest, please do not return an error code and strictly return ESP_OK.
static esp_err_t app_attribute_update_cb(attribute::callback_type_t type, uint16_t endpoint_id, uint32_t cluster_id,
                                         uint32_t attribute_id, esp_matter_attr_val_t *val, void *priv_data)
{
    esp_err_t err = ESP_OK;

    if (type == PRE_UPDATE) {
        // Handle the invert switch OnOff attribute
        if (endpoint_id == invert_switch_endpoint_id && 
            cluster_id == OnOff::Id && 
            attribute_id == OnOff::Attributes::OnOff::Id) {
            
            invert_switch_state = val->val.b;
            ESP_LOGI(TAG, "Invert switch set to: %s", invert_switch_state ? "ON (inverted)" : "OFF (normal)");
            
            // Sync with vendor attribute
            node_t *node = node::get();
            endpoint_t *root_ep = endpoint::get(node, 0);
            cluster_t *custom_cluster = cluster::get(root_ep, kVendorClusterId);
            if (custom_cluster) {
                attribute_t *invert_attr = attribute::get(custom_cluster, kInvertAttributeId);
                if (invert_attr) {
                    esp_matter_attr_val_t invert_val = esp_matter_bool(invert_switch_state);
                    attribute::set_val(invert_attr, &invert_val);
                }
            }
            return ESP_OK;
        }

        // Handle Invert Switch attribute
        if (endpoint_id == 0 && cluster_id == kVendorClusterId && attribute_id == kInvertAttributeId) {
            invert_switch_state = val->val.b;
            ESP_LOGI(TAG, "Invert switch set to: %s", invert_switch_state ? "ON" : "OFF");
            
            // Sync with switch endpoint
            node_t *node = node::get();
            endpoint_t *switch_ep = endpoint::get(node, invert_switch_endpoint_id);
            if (switch_ep) {
                cluster_t *onoff_cluster = cluster::get(switch_ep, OnOff::Id);
                if (onoff_cluster) {
                    attribute_t *onoff_attr = attribute::get(onoff_cluster, OnOff::Attributes::OnOff::Id);
                    if (onoff_attr) {
                        esp_matter_attr_val_t onoff_val = esp_matter_bool(invert_switch_state);
                        attribute::set_val(onoff_attr, &onoff_val);
                    }
                }
            }
            return ESP_OK;
        }

        // Handle OnOff with possible inversion
        if (endpoint_id == light_endpoint_id &&
            cluster_id == OnOff::Id &&
            attribute_id == OnOff::Attributes::OnOff::Id) {

            bool requested = val->val.b;
            bool actual = invert_switch_state ? !requested : requested;
            val->val.b = actual;  // override before reaching driver
        }

        // Driver update
        app_driver_handle_t driver_handle = (app_driver_handle_t)priv_data;
        err = app_driver_attribute_update(driver_handle, endpoint_id, cluster_id, attribute_id, val);
    }

    return err;
}

static esp_err_t create_invert_switch_endpoint(node_t *node)
{
    on_off_plugin_unit::config_t switch_config;
    switch_config.on_off.on_off = invert_switch_state;
    
    endpoint_t *switch_endpoint = on_off_plugin_unit::create(node, &switch_config, ENDPOINT_FLAG_NONE, nullptr);
    if (!switch_endpoint) {
        ESP_LOGE(TAG, "Failed to create invert switch endpoint");
        return ESP_FAIL;
    }
    
    invert_switch_endpoint_id = endpoint::get_id(switch_endpoint);
    ESP_LOGI(TAG, "Invert switch created with endpoint_id %d", invert_switch_endpoint_id);
    
    return ESP_OK;
}

extern "C" void app_main()
{
    ESP_LOGI(TAG, "Hello there glowswitch beta hacker!");
    esp_err_t err = ESP_OK;

    /* Initialize the ESP NVS layer */
    nvs_flash_init();

    MEMORY_PROFILER_DUMP_HEAP_STAT("Bootup");

    /* Initialize driver */
    app_driver_handle_t light_handle = app_driver_light_init();
    app_driver_handle_t button_handle = app_driver_button_init();
    app_reset_button_register(button_handle);

    /* Create a Matter node and add the mandatory Root Node device type on endpoint 0 */
    node::config_t node_config;

    // node handle can be used to add/modify other endpoints.
    node_t *node = node::create(&node_config, app_attribute_update_cb, app_identification_cb);
    ABORT_APP_ON_FAILURE(node != nullptr, ESP_LOGE(TAG, "Failed to create Matter node"));

    MEMORY_PROFILER_DUMP_HEAP_STAT("node created");

    on_off_light::config_t light_config;
    light_config.on_off.on_off = DEFAULT_POWER;
    light_config.on_off_lighting.start_up_on_off = nullptr;

    // endpoint handles can be used to add/modify clusters.
    endpoint_t *endpoint = on_off_light::create(node, &light_config, ENDPOINT_FLAG_NONE, light_handle);
    ABORT_APP_ON_FAILURE(endpoint != nullptr, ESP_LOGE(TAG, "Failed to create on_off light endpoint"));

    light_endpoint_id = endpoint::get_id(endpoint);
    ESP_LOGI(TAG, "Light created with endpoint_id %d", light_endpoint_id);

    // Create invert switch endpoint
    esp_err_t switch_err = create_invert_switch_endpoint(node);
    ABORT_APP_ON_FAILURE(switch_err == ESP_OK, ESP_LOGE(TAG, "Failed to create invert switch endpoint"));

    // Add custom "Invert Switch" attribute on endpoint 0
    endpoint_t *root_ep = endpoint::get(node, 0);
    cluster_t *custom_cluster = cluster::create(root_ep, kVendorClusterId, CLUSTER_FLAG_SERVER);
    attribute_t *invert_attr = attribute::create(custom_cluster,
        kInvertAttributeId,
        ATTRIBUTE_FLAG_WRITABLE | MATTER_ATTRIBUTE_FLAG_NONVOLATILE,
        esp_matter_bool(invert_switch_state));
    
    

#if CHIP_DEVICE_CONFIG_ENABLE_THREAD && CHIP_DEVICE_CONFIG_ENABLE_WIFI_STATION
    // Enable secondary network interface
    secondary_network_interface::config_t secondary_network_interface_config;
    endpoint = endpoint::secondary_network_interface::create(node, &secondary_network_interface_config, ENDPOINT_FLAG_NONE, nullptr);
    ABORT_APP_ON_FAILURE(endpoint != nullptr, ESP_LOGE(TAG, "Failed to create secondary network interface endpoint"));
#endif


#if CHIP_DEVICE_CONFIG_ENABLE_THREAD
    /* Set OpenThread platform config */
    esp_openthread_platform_config_t config = {
        .radio_config = ESP_OPENTHREAD_DEFAULT_RADIO_CONFIG(),
        .host_config = ESP_OPENTHREAD_DEFAULT_HOST_CONFIG(),
        .port_config = ESP_OPENTHREAD_DEFAULT_PORT_CONFIG(),
    };
    set_openthread_platform_config(&config);
#endif

#ifdef CONFIG_ENABLE_SET_CERT_DECLARATION_API
    auto * dac_provider = get_dac_provider();
#ifdef CONFIG_SEC_CERT_DAC_PROVIDER
    static_cast<ESP32SecureCertDACProvider *>(dac_provider)->SetCertificationDeclaration(cdSpan);
#elif defined(CONFIG_FACTORY_PARTITION_DAC_PROVIDER)
    static_cast<ESP32FactoryDataProvider *>(dac_provider)->SetCertificationDeclaration(cdSpan);
#endif
#endif // CONFIG_ENABLE_SET_CERT_DECLARATION_API

    /* Matter start */
    err = esp_matter::start(app_event_cb);
    ABORT_APP_ON_FAILURE(err == ESP_OK, ESP_LOGE(TAG, "Failed to start Matter, err:%d", err));

    MEMORY_PROFILER_DUMP_HEAP_STAT("matter started");

    /* Starting driver with default values */
    // app_driver_light_set_defaults(light_endpoint_id);

#if CONFIG_ENABLE_ENCRYPTED_OTA
    err = esp_matter_ota_requestor_encrypted_init(s_decryption_key, s_decryption_key_len);
    ABORT_APP_ON_FAILURE(err == ESP_OK, ESP_LOGE(TAG, "Failed to initialized the encrypted OTA, err: %d", err));
#endif // CONFIG_ENABLE_ENCRYPTED_OTA

#if CONFIG_ENABLE_CHIP_SHELL
    esp_matter::console::diagnostics_register_commands();
    esp_matter::console::wifi_register_commands();
    esp_matter::console::factoryreset_register_commands();
#if CONFIG_OPENTHREAD_CLI
    esp_matter::console::otcli_register_commands();
#endif
    esp_matter::console::init();
#endif

    while (true) {
        MEMORY_PROFILER_DUMP_HEAP_STAT("Idle");
        vTaskDelay(10000 / portTICK_PERIOD_MS);
    }
}