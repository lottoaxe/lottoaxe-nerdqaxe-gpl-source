/**
 * LottoAxe OS Custom Endpoints (NerdQaxe Edition)
 *
 * Provides pool profiles, tuning presets (conservative only),
 * config export/import, safety status, and diagnostics for beta testers.
 *
 * Storage: Uses a dedicated NVS namespace "lottoaxe" for JSON blobs.
 * All presets are CONSERVATIVE — no YOLO OC, no aggressive Auto-OC.
 */

#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"

#include "ArduinoJson.h"

#include "psram_allocator.h"
#include "global_state.h"
#include "nvs_config.h"
#include "http_cors.h"
#include "http_utils.h"

static const char *TAG = "http_lottoaxe";

#define LOTTOAXE_NVS_NAMESPACE "lottoaxe"
#define LOTTOAXE_VERSION "1.0.0-beta1"
#define LOTTOAXE_EDITION "NerdQaxe Edition"

// NVS keys (max 15 chars)
#define NVS_KEY_PROFILES "la_profiles"
#define NVS_KEY_ACTIVE_PROFILE "la_act_prof"

// Max blob sizes
#define MAX_PROFILES_BLOB 4096
#define MAX_CONFIG_BLOB 8192

// ============================================================================
// Helper: open lottoaxe NVS namespace
// ============================================================================
static esp_err_t lottoaxe_nvs_open(nvs_handle_t *handle, nvs_open_mode_t mode)
{
    return nvs_open(LOTTOAXE_NVS_NAMESPACE, mode, handle);
}

// ============================================================================
// GET /api/lottoaxe/profiles — List saved pool profiles
// ============================================================================
esp_err_t GET_lottoaxe_profiles(httpd_req_t *req)
{
    ConGuard g(http_server, req);

    if (is_network_allowed(req) != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Unauthorized");
    }

    httpd_resp_set_type(req, "application/json");
    if (set_cors_headers(req) != ESP_OK) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    PSRAMAllocator allocator;
    JsonDocument doc(&allocator);

    // Read profiles blob from NVS
    nvs_handle_t nvs;
    if (lottoaxe_nvs_open(&nvs, NVS_READONLY) == ESP_OK) {
        size_t blob_size = 0;
        if (nvs_get_blob(nvs, NVS_KEY_PROFILES, NULL, &blob_size) == ESP_OK && blob_size > 0) {
            char *blob = (char *)malloc(blob_size + 1);
            if (blob) {
                if (nvs_get_blob(nvs, NVS_KEY_PROFILES, blob, &blob_size) == ESP_OK) {
                    blob[blob_size] = '\0';
                    // Parse stored JSON array into response
                    JsonDocument stored(&allocator);
                    if (deserializeJson(stored, blob) == DeserializationError::Ok) {
                        doc["profiles"] = stored.as<JsonArray>();
                    }
                }
                free(blob);
            }
        }
        nvs_close(nvs);
    }

    // If no profiles found, return empty array
    if (!doc.containsKey("profiles")) {
        doc["profiles"].to<JsonArray>();
    }

    // Include active profile index
    nvs_handle_t nvs2;
    uint16_t active = 0;
    if (lottoaxe_nvs_open(&nvs2, NVS_READONLY) == ESP_OK) {
        size_t s = sizeof(uint16_t);
        nvs_get_blob(nvs2, NVS_KEY_ACTIVE_PROFILE, &active, &s);
        nvs_close(nvs2);
    }
    doc["activeProfile"] = active;

    return sendJsonResponse(req, doc);
}

// ============================================================================
// POST /api/lottoaxe/profiles — Save pool profiles array
// Body: { "profiles": [...], "activeProfile": 0 }
// ============================================================================
esp_err_t POST_lottoaxe_profiles(httpd_req_t *req)
{
    ConGuard g(http_server, req);

    if (is_network_allowed(req) != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Unauthorized");
    }

    httpd_resp_set_type(req, "application/json");
    if (set_cors_headers(req) != ESP_OK) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    PSRAMAllocator allocator;
    JsonDocument doc(&allocator);

    if (getJsonData(req, doc) != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
    }

    // Validate
    if (!doc.containsKey("profiles") || !doc["profiles"].is<JsonArray>()) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing profiles array");
    }

    JsonArray profiles = doc["profiles"].as<JsonArray>();
    if (profiles.size() > 10) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Max 10 profiles allowed");
    }

    // Serialize profiles array to string
    char *blob = (char *)malloc(MAX_PROFILES_BLOB);
    if (!blob) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OOM");
    }

    size_t len = serializeJson(profiles, blob, MAX_PROFILES_BLOB);

    // Store in NVS
    nvs_handle_t nvs;
    esp_err_t err = lottoaxe_nvs_open(&nvs, NVS_READWRITE);
    if (err != ESP_OK) {
        free(blob);
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "NVS open failed");
    }

    err = nvs_set_blob(nvs, NVS_KEY_PROFILES, blob, len);
    free(blob);

    if (err != ESP_OK) {
        nvs_close(nvs);
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "NVS write failed");
    }

    // Store active profile index
    if (doc.containsKey("activeProfile")) {
        uint16_t active = doc["activeProfile"].as<uint16_t>();
        nvs_set_blob(nvs, NVS_KEY_ACTIVE_PROFILE, &active, sizeof(uint16_t));
    }

    nvs_commit(nvs);
    nvs_close(nvs);

    // Response
    JsonDocument resp(&allocator);
    resp["success"] = true;
    resp["saved"] = (int)profiles.size();
    return sendJsonResponse(req, resp);
}

// ============================================================================
// GET /api/lottoaxe/presets — List available tuning presets
// CONSERVATIVE ONLY — no YOLO OC, no aggressive presets
// ============================================================================
esp_err_t GET_lottoaxe_presets(httpd_req_t *req)
{
    ConGuard g(http_server, req);

    if (is_network_allowed(req) != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Unauthorized");
    }

    httpd_resp_set_type(req, "application/json");
    if (set_cors_headers(req) != ESP_OK) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    PSRAMAllocator allocator;
    JsonDocument doc(&allocator);

    Board *board = SYSTEM_MODULE.getBoard();
    const char *model = board->getDeviceModel();

    JsonArray presets = doc["presets"].to<JsonArray>();

    // Default/Stock preset — always available
    {
        JsonObject p = presets.add<JsonObject>();
        p["id"] = "stock";
        p["name"] = "Stock (Default)";
        p["description"] = "Factory default settings - safest option";
        p["frequency"] = board->getDefaultAsicFrequency();
        p["coreVoltage"] = board->getDefaultAsicVoltageMillis();
        p["risk"] = "none";
        p["recommended"] = true;
    }

    // Conservative undervolt — slightly lower power, same hashrate
    {
        JsonObject p = presets.add<JsonObject>();
        p["id"] = "eco";
        p["name"] = "Eco (Conservative)";
        p["description"] = "Slightly reduced voltage for lower power consumption";
        p["frequency"] = board->getDefaultAsicFrequency();
        p["coreVoltage"] = (int)(board->getDefaultAsicVoltageMillis() * 0.97f); // 3% undervolt
        p["risk"] = "low";
        p["recommended"] = false;
    }

    // Mild overclock — small frequency bump with voltage headroom
    {
        JsonObject p = presets.add<JsonObject>();
        p["id"] = "mild";
        p["name"] = "Mild Tune";
        p["description"] = "Small frequency increase with adequate voltage margin";
        p["frequency"] = (int)(board->getDefaultAsicFrequency() * 1.05f); // +5%
        p["coreVoltage"] = (int)(board->getDefaultAsicVoltageMillis() * 1.02f); // +2% voltage
        p["risk"] = "low";
        p["recommended"] = false;
    }

    // That's it. No aggressive presets. No YOLO.
    doc["board"] = model;
    doc["warning"] = "All presets are conservative. For custom tuning, use manual settings with caution.";
    doc["currentFrequency"] = Config::nvs_config_get_u16(NVS_CONFIG_ASIC_FREQ, board->getDefaultAsicFrequency());
    doc["currentVoltage"] = Config::nvs_config_get_u16(NVS_CONFIG_ASIC_VOLTAGE, board->getDefaultAsicVoltageMillis());

    return sendJsonResponse(req, doc);
}

// ============================================================================
// POST /api/lottoaxe/presets/apply — Apply a tuning preset
// Body: { "presetId": "stock" }
// ============================================================================
esp_err_t POST_lottoaxe_presets_apply(httpd_req_t *req)
{
    ConGuard g(http_server, req);

    if (is_network_allowed(req) != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Unauthorized");
    }

    httpd_resp_set_type(req, "application/json");
    if (set_cors_headers(req) != ESP_OK) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    // Validate OTP if enabled
    if (validateOTP(req) != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "OTP required");
    }

    PSRAMAllocator allocator;
    JsonDocument doc(&allocator);

    if (getJsonData(req, doc) != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
    }

    const char *presetId = doc["presetId"] | "";
    if (strlen(presetId) == 0) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing presetId");
    }

    Board *board = SYSTEM_MODULE.getBoard();
    uint16_t freq = 0;
    uint16_t voltage = 0;

    if (strcmp(presetId, "stock") == 0) {
        freq = board->getDefaultAsicFrequency();
        voltage = board->getDefaultAsicVoltageMillis();
    } else if (strcmp(presetId, "eco") == 0) {
        freq = board->getDefaultAsicFrequency();
        voltage = (uint16_t)(board->getDefaultAsicVoltageMillis() * 0.97f);
    } else if (strcmp(presetId, "mild") == 0) {
        freq = (uint16_t)(board->getDefaultAsicFrequency() * 1.05f);
        voltage = (uint16_t)(board->getDefaultAsicVoltageMillis() * 1.02f);
    } else {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Unknown preset");
    }

    // Safety check: never exceed board maximums
    if (freq > board->getAbsMaxAsicFrequency()) {
        freq = (uint16_t)board->getAbsMaxAsicFrequency();
    }
    if (voltage > board->getAbsMaxAsicVoltageMillis()) {
        voltage = (uint16_t)board->getAbsMaxAsicVoltageMillis();
    }

    // Apply settings
    Config::setAsicFrequency(freq);
    Config::setAsicVoltage(voltage);

    ESP_LOGI(TAG, "Applied preset '%s': freq=%u, voltage=%u", presetId, freq, voltage);

    // Response
    JsonDocument resp(&allocator);
    resp["success"] = true;
    resp["preset"] = presetId;
    resp["appliedFrequency"] = freq;
    resp["appliedVoltage"] = voltage;
    resp["restartRequired"] = true;
    return sendJsonResponse(req, resp);
}

// ============================================================================
// GET /api/lottoaxe/config/export — Export full device config as JSON
// ============================================================================
esp_err_t GET_lottoaxe_config_export(httpd_req_t *req)
{
    ConGuard g(http_server, req);

    if (is_network_allowed(req) != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Unauthorized");
    }

    httpd_resp_set_type(req, "application/json");
    if (set_cors_headers(req) != ESP_OK) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    PSRAMAllocator allocator;
    JsonDocument doc(&allocator);

    Board *board = SYSTEM_MODULE.getBoard();

    doc["exportVersion"] = 1;
    doc["lottoaxeVersion"] = LOTTOAXE_VERSION;
    doc["edition"] = LOTTOAXE_EDITION;
    doc["board"] = board->getDeviceModel();
    doc["timestamp"] = (uint64_t)(esp_timer_get_time() / 1000000ULL);

    // Network settings (exclude password for security)
    JsonObject network = doc["network"].to<JsonObject>();
    char *ssid = Config::getWifiSSID();
    char *hostname = Config::getHostname();
    network["ssid"] = ssid;
    network["hostname"] = hostname;
    free(ssid);
    free(hostname);

    // Mining settings
    JsonObject mining = doc["mining"].to<JsonObject>();
    char *stratumURL = Config::getStratumURL();
    char *stratumUser = Config::getStratumUser();
    char *fbURL = Config::getStratumFallbackURL();
    char *fbUser = Config::getStratumFallbackUser();
    mining["stratumURL"] = stratumURL;
    mining["stratumPort"] = Config::getStratumPortNumber();
    mining["stratumUser"] = stratumUser;
    mining["fallbackStratumURL"] = fbURL;
    mining["fallbackStratumPort"] = Config::getStratumFallbackPortNumber();
    mining["fallbackStratumUser"] = fbUser;
    mining["frequency"] = Config::nvs_config_get_u16(NVS_CONFIG_ASIC_FREQ, board->getDefaultAsicFrequency());
    mining["coreVoltage"] = Config::nvs_config_get_u16(NVS_CONFIG_ASIC_VOLTAGE, board->getDefaultAsicVoltageMillis());
    free(stratumURL);
    free(stratumUser);
    free(fbURL);
    free(fbUser);

    // Fan settings
    JsonObject fan = doc["fan"].to<JsonObject>();
    fan["mode"] = Config::getTempControlMode();
    fan["speed"] = Config::getFanSpeed();
    fan["overheatTemp"] = Config::getOverheatTemp();

    // Pool mode
    doc["poolMode"] = Config::getPoolMode();
    doc["poolBalance"] = Config::getPoolBalance();

    return sendJsonResponse(req, doc);
}

// ============================================================================
// POST /api/lottoaxe/config/import — Import config from JSON
// Body: full config JSON from export
// ============================================================================
esp_err_t POST_lottoaxe_config_import(httpd_req_t *req)
{
    ConGuard g(http_server, req);

    if (is_network_allowed(req) != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Unauthorized");
    }

    httpd_resp_set_type(req, "application/json");
    if (set_cors_headers(req) != ESP_OK) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    // Validate OTP if enabled
    if (validateOTP(req) != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "OTP required");
    }

    PSRAMAllocator allocator;
    JsonDocument doc(&allocator);

    if (getJsonData(req, doc) != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
    }

    // Validate export version
    int ver = doc["exportVersion"] | 0;
    if (ver < 1) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid config format");
    }

    int applied = 0;

    // Import network settings
    if (doc.containsKey("network")) {
        JsonObject network = doc["network"].as<JsonObject>();
        if (network.containsKey("ssid")) {
            Config::setWifiSSID(network["ssid"].as<const char*>());
            applied++;
        }
        if (network.containsKey("hostname")) {
            Config::setHostname(network["hostname"].as<const char*>());
            applied++;
        }
    }

    // Import mining settings
    if (doc.containsKey("mining")) {
        JsonObject mining = doc["mining"].as<JsonObject>();
        if (mining.containsKey("stratumURL")) {
            Config::setStratumURL(mining["stratumURL"].as<const char*>());
            applied++;
        }
        if (mining.containsKey("stratumPort")) {
            Config::setStratumPortNumber(mining["stratumPort"].as<uint16_t>());
            applied++;
        }
        if (mining.containsKey("stratumUser")) {
            Config::setStratumUser(mining["stratumUser"].as<const char*>());
            applied++;
        }
        if (mining.containsKey("fallbackStratumURL")) {
            Config::setStratumFallbackURL(mining["fallbackStratumURL"].as<const char*>());
            applied++;
        }
        if (mining.containsKey("fallbackStratumPort")) {
            Config::setStratumFallbackPortNumber(mining["fallbackStratumPort"].as<uint16_t>());
            applied++;
        }
        if (mining.containsKey("fallbackStratumUser")) {
            Config::setStratumFallbackUser(mining["fallbackStratumUser"].as<const char*>());
            applied++;
        }
        if (mining.containsKey("frequency")) {
            Config::setAsicFrequency(mining["frequency"].as<uint16_t>());
            applied++;
        }
        if (mining.containsKey("coreVoltage")) {
            Config::setAsicVoltage(mining["coreVoltage"].as<uint16_t>());
            applied++;
        }
    }

    // Import fan settings
    if (doc.containsKey("fan")) {
        JsonObject fan = doc["fan"].as<JsonObject>();
        if (fan.containsKey("mode")) {
            Config::setTempControlMode(fan["mode"].as<uint16_t>());
            applied++;
        }
        if (fan.containsKey("speed")) {
            Config::setFanSpeed(fan["speed"].as<uint16_t>());
            applied++;
        }
        if (fan.containsKey("overheatTemp")) {
            Config::setOverheatTemp(fan["overheatTemp"].as<uint16_t>());
            applied++;
        }
    }

    // Pool mode
    if (doc.containsKey("poolMode")) {
        Config::setPoolMode(doc["poolMode"].as<uint16_t>());
        applied++;
    }
    if (doc.containsKey("poolBalance")) {
        Config::setPoolBalance(doc["poolBalance"].as<uint16_t>());
        applied++;
    }

    ESP_LOGI(TAG, "Config import: %d settings applied", applied);

    JsonDocument resp(&allocator);
    resp["success"] = true;
    resp["settingsApplied"] = applied;
    resp["restartRequired"] = true;
    return sendJsonResponse(req, resp);
}

// ============================================================================
// POST /api/lottoaxe/config/factory-reset — Factory reset all settings
// ============================================================================
esp_err_t POST_lottoaxe_factory_reset(httpd_req_t *req)
{
    ConGuard g(http_server, req);

    if (is_network_allowed(req) != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Unauthorized");
    }

    httpd_resp_set_type(req, "application/json");
    if (set_cors_headers(req) != ESP_OK) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    // Validate OTP if enabled
    if (validateOTP(req) != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "OTP required");
    }

    ESP_LOGW(TAG, "Factory reset requested!");

    // Erase main NVS partition
    esp_err_t err = nvs_flash_erase();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS erase failed: %s", esp_err_to_name(err));
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Reset failed");
    }

    // Re-init NVS
    err = nvs_flash_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS re-init failed: %s", esp_err_to_name(err));
    }

    PSRAMAllocator allocator;
    JsonDocument resp(&allocator);
    resp["success"] = true;
    resp["message"] = "Factory reset complete. Device will restart.";
    sendJsonResponse(req, resp);

    // Schedule restart
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();

    return ESP_OK;
}

// ============================================================================
// GET /api/lottoaxe/safety — Safety status (temps, voltages, limits)
// ============================================================================
esp_err_t GET_lottoaxe_safety(httpd_req_t *req)
{
    ConGuard g(http_server, req);

    if (is_network_allowed(req) != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Unauthorized");
    }

    httpd_resp_set_type(req, "application/json");
    if (set_cors_headers(req) != ESP_OK) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    PSRAMAllocator allocator;
    JsonDocument doc(&allocator);

    Board *board = SYSTEM_MODULE.getBoard();

    // Current readings
    doc["chipTempMax"] = POWER_MANAGEMENT_MODULE.getChipTempMax();
    doc["vrTemp"] = POWER_MANAGEMENT_MODULE.getVRTemp();
    doc["power"] = POWER_MANAGEMENT_MODULE.getPower();
    doc["current"] = POWER_MANAGEMENT_MODULE.getCurrent();
    doc["voltage"] = board->getVout();
    doc["fanRPM"] = POWER_MANAGEMENT_MODULE.getFanRPM(0);
    doc["fanPercent"] = POWER_MANAGEMENT_MODULE.getFanPerc();
    doc["isShutdown"] = POWER_MANAGEMENT_MODULE.isShutdown();

    // Per-ASIC temps
    {
        JsonArray temps = doc["asicTemps"].to<JsonArray>();
        for (int i = 0; i < board->getAsicCount(); i++) {
            temps.add(board->getChipTemp(i));
        }
    }

    // Safety limits (from board definition)
    JsonObject limits = doc["limits"].to<JsonObject>();
    limits["maxPower"] = board->getMaxPin();
    limits["minPower"] = board->getMinPin();
    limits["maxVoltage"] = board->getMaxVin();
    limits["minVoltage"] = board->getMinVin();
    limits["maxCurrent"] = board->getMaxCurrentA();
    limits["minCurrent"] = board->getMinCurrentA();
    limits["overheatTemp"] = Config::getOverheatTemp();
    limits["maxFrequency"] = board->getAbsMaxAsicFrequency();
    limits["maxVoltageMillis"] = board->getAbsMaxAsicVoltageMillis();

    // Current tuning
    JsonObject tuning = doc["tuning"].to<JsonObject>();
    tuning["frequency"] = Config::nvs_config_get_u16(NVS_CONFIG_ASIC_FREQ, board->getDefaultAsicFrequency());
    tuning["coreVoltage"] = Config::nvs_config_get_u16(NVS_CONFIG_ASIC_VOLTAGE, board->getDefaultAsicVoltageMillis());
    tuning["defaultFrequency"] = board->getDefaultAsicFrequency();
    tuning["defaultVoltage"] = board->getDefaultAsicVoltageMillis();

    // Safety flags
    float tempMax = POWER_MANAGEMENT_MODULE.getChipTempMax();
    float overheatThresh = (float)Config::getOverheatTemp();
    doc["overTemp"] = (tempMax >= overheatThresh);
    doc["nearOverTemp"] = (tempMax >= overheatThresh * 0.9f);
    doc["overPower"] = (POWER_MANAGEMENT_MODULE.getPower() > board->getMaxPin());
    doc["overCurrent"] = (POWER_MANAGEMENT_MODULE.getCurrent() / 1000.0f > board->getMaxCurrentA());

    return sendJsonResponse(req, doc);
}

// ============================================================================
// GET /api/lottoaxe/diagnostics — Full diagnostics dump for beta testers
// ============================================================================
esp_err_t GET_lottoaxe_diagnostics(httpd_req_t *req)
{
    ConGuard g(http_server, req);

    if (is_network_allowed(req) != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Unauthorized");
    }

    httpd_resp_set_type(req, "application/json");
    if (set_cors_headers(req) != ESP_OK) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    PSRAMAllocator allocator;
    JsonDocument doc(&allocator);

    Board *board = SYSTEM_MODULE.getBoard();
    History *history = SYSTEM_MODULE.getHistory();

    // LottoAxe info
    doc["lottoaxeVersion"] = LOTTOAXE_VERSION;
    doc["edition"] = LOTTOAXE_EDITION;
    doc["exportTimestamp"] = (uint64_t)(esp_timer_get_time() / 1000000ULL);

    // Board info
    JsonObject boardInfo = doc["board"].to<JsonObject>();
    boardInfo["model"] = board->getDeviceModel();
    boardInfo["asicModel"] = board->getAsicModel();
    boardInfo["asicCount"] = board->getAsicCount();
    boardInfo["numFans"] = board->getNumFans();

    // Firmware info
    JsonObject firmware = doc["firmware"].to<JsonObject>();
    const esp_app_desc_t *app_desc = esp_app_get_description();
    firmware["version"] = app_desc->version;
    firmware["idfVersion"] = app_desc->idf_ver;
    firmware["compileDate"] = app_desc->date;
    firmware["compileTime"] = app_desc->time;
    const esp_partition_t *running = esp_ota_get_running_partition();
    firmware["partition"] = running ? running->label : "unknown";

    // System state
    JsonObject sys = doc["system"].to<JsonObject>();
    sys["uptimeSeconds"] = (uint64_t)(esp_timer_get_time() / 1000000ULL);
    sys["freeHeap"] = esp_get_free_heap_size();
    sys["freeHeapInternal"] = esp_get_free_internal_heap_size();
    sys["minFreeHeap"] = esp_get_minimum_free_heap_size();
    sys["chipTempMax"] = POWER_MANAGEMENT_MODULE.getChipTempMax();
    sys["vrTemp"] = POWER_MANAGEMENT_MODULE.getVRTemp();
    sys["power"] = POWER_MANAGEMENT_MODULE.getPower();
    sys["current"] = POWER_MANAGEMENT_MODULE.getCurrent();
    sys["voltage"] = board->getVout();
    sys["isShutdown"] = POWER_MANAGEMENT_MODULE.isShutdown();

    // ASIC temps
    {
        JsonArray temps = sys["asicTemps"].to<JsonArray>();
        for (int i = 0; i < board->getAsicCount(); i++) {
            temps.add(board->getChipTemp(i));
        }
    }

    // Fan status
    JsonObject fanInfo = doc["fans"].to<JsonObject>();
    fanInfo["count"] = board->getNumFans();
    {
        JsonArray fanArr = fanInfo["channels"].to<JsonArray>();
        for (int ch = 0; ch < board->getNumFans(); ch++) {
            JsonObject f = fanArr.add<JsonObject>();
            f["rpm"] = POWER_MANAGEMENT_MODULE.getFanRPM(ch);
            f["percent"] = POWER_MANAGEMENT_MODULE.getFanPerc(ch);
            f["mode"] = Config::getFanMode(ch);
            f["manualSpeed"] = Config::getFanManualSpeed(ch);
        }
    }

    // Mining stats
    JsonObject mining = doc["mining"].to<JsonObject>();
    mining["hashRate"] = SYSTEM_MODULE.getCurrentHashrate();
    mining["hashRate_1m"] = history->getCurrentHashrate1m();
    mining["hashRate_10m"] = history->getCurrentHashrate10m();
    mining["hashRate_1h"] = history->getCurrentHashrate1h();
    mining["hashRate_1d"] = history->getCurrentHashrate1d();
    mining["sharesAccepted"] = STRATUM_MANAGER->getSharesAccepted();
    mining["sharesRejected"] = STRATUM_MANAGER->getSharesRejected();
    mining["bestDiff"] = STRATUM_MANAGER->getBestDiff();
    mining["bestSessionDiff"] = STRATUM_MANAGER->getBestSessionDiff();
    mining["poolDifficulty"] = STRATUM_MANAGER->getPoolDifficulty();
    mining["networkDifficulty"] = STRATUM_MANAGER->getNetworkDifficulty();
    mining["foundBlocks"] = STRATUM_MANAGER->getTotalFoundBlocks();

    // Tuning settings
    JsonObject tuning = doc["tuning"].to<JsonObject>();
    tuning["frequency"] = Config::nvs_config_get_u16(NVS_CONFIG_ASIC_FREQ, board->getDefaultAsicFrequency());
    tuning["coreVoltage"] = Config::nvs_config_get_u16(NVS_CONFIG_ASIC_VOLTAGE, board->getDefaultAsicVoltageMillis());
    tuning["fanMode"] = Config::getTempControlMode();
    tuning["fanSpeed"] = Config::getFanSpeed();
    tuning["overheatTemp"] = Config::getOverheatTemp();

    // Network
    JsonObject net = doc["network"].to<JsonObject>();
    net["ip"] = SYSTEM_MODULE.getIPAddress();
    net["mac"] = SYSTEM_MODULE.getMacAddress();
    net["rssi"] = SYSTEM_MODULE.get_wifi_rssi();
    net["pingRtt"] = get_last_ping_rtt();
    net["pingLoss"] = get_recent_ping_loss();

    // Pool info
    JsonObject pool = doc["pool"].to<JsonObject>();
    char *sURL = Config::getStratumURL();
    char *sUser = Config::getStratumUser();
    pool["url"] = sURL;
    pool["port"] = Config::getStratumPortNumber();
    pool["user"] = sUser;
    free(sURL);
    free(sUser);

    return sendJsonResponse(req, doc);
}

// ============================================================================
// GET /api/lottoaxe/version — LottoAxe version info
// ============================================================================
esp_err_t GET_lottoaxe_version(httpd_req_t *req)
{
    ConGuard g(http_server, req);

    if (is_network_allowed(req) != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Unauthorized");
    }

    httpd_resp_set_type(req, "application/json");
    if (set_cors_headers(req) != ESP_OK) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    PSRAMAllocator allocator;
    JsonDocument doc(&allocator);

    const esp_app_desc_t *app_desc = esp_app_get_description();
    Board *board = SYSTEM_MODULE.getBoard();

    doc["lottoaxeVersion"] = LOTTOAXE_VERSION;
    doc["edition"] = LOTTOAXE_EDITION;
    doc["firmwareVersion"] = app_desc->version;
    doc["idfVersion"] = app_desc->idf_ver;
    doc["board"] = board->getDeviceModel();
    doc["asicModel"] = board->getAsicModel();
    doc["beta"] = true;

    return sendJsonResponse(req, doc);
}
