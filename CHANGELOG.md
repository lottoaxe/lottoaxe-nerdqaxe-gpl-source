# Changelog — LottoAxe OS NerdQaxe Edition

## v1.0.0-beta1 (2026-05-25)

Initial beta release.

### Added
- LottoAxe PrimeNG dashboard replacing stock Angular/Nebular UI
- NerdQaxe API adapter service mapping NerdQaxe fields to LottoAxe interface
- Custom LottoAxe firmware endpoints:
  - `/api/lottoaxe/profiles` — Pool profile management
  - `/api/lottoaxe/presets` — Conservative tuning presets (stock, eco, mild)
  - `/api/lottoaxe/safety` — Live safety status
  - `/api/lottoaxe/diagnostics` — Full diagnostic dump for beta testers
  - `/api/lottoaxe/config/export` and `/import` — Config backup/restore
  - `/api/lottoaxe/config/factory-reset` — NVS erase + restart
  - `/api/lottoaxe/version` — LottoAxe version info
- Beta badge and NerdQaxe Edition label in dashboard
- Diagnostics page with export/copy/preview
- Conservative-only presets (YOLO OC and Lotto Mode disabled)
- Overclock toggle disabled for safety
- Board-specific firmware builds for NerdQAxe+, NerdQAxe++, NerdQX
- Six-layer safety system (preset caps, board limits, fan control, power management, PID thermal, hardware VR protection)

### Based On
- ESP-Miner-NerdQAxePlus v1.0.38-alpha4 (develop branch) by shufps
