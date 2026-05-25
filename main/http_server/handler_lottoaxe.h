#pragma once

#include "esp_http_server.h"

// LottoAxe custom endpoints
esp_err_t GET_lottoaxe_profiles(httpd_req_t *req);
esp_err_t POST_lottoaxe_profiles(httpd_req_t *req);
esp_err_t GET_lottoaxe_presets(httpd_req_t *req);
esp_err_t POST_lottoaxe_presets_apply(httpd_req_t *req);
esp_err_t GET_lottoaxe_config_export(httpd_req_t *req);
esp_err_t POST_lottoaxe_config_import(httpd_req_t *req);
esp_err_t POST_lottoaxe_factory_reset(httpd_req_t *req);
esp_err_t GET_lottoaxe_safety(httpd_req_t *req);
esp_err_t GET_lottoaxe_diagnostics(httpd_req_t *req);
esp_err_t GET_lottoaxe_version(httpd_req_t *req);
