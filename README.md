# LottoAxe OS — NerdQaxe Edition (GPL Source)

Custom firmware for NerdQaxe-compatible SHA-256 solo lottery miners.

> **BETA** — This is an early-access release. No physical NerdQaxe hardware has been tested by the developer yet.

## Supported Boards

| Board | ASIC | Chips | Max Power |
|-------|------|-------|-----------|
| NerdQAxe+ | BM1368 | 4 | 70W |
| NerdQAxe++ | BM1370 | 4 | 100W |
| NerdQX | BM1370 | 4 | 240W |

## What This Repo Contains

This repository contains the **complete source code** required to build LottoAxe OS NerdQaxe Edition firmware, published under the GNU General Public License v3.0 to comply with GPL obligations.

Included:
- `main/` — ESP32 firmware source (C++)
- `main/http_server/axe-os/` — Angular web dashboard source (TypeScript)
- `components/` — ESP-IDF component libraries
- `CMakeLists.txt`, `partitions.csv`, `sdkconfig.*` — Build configuration
- `docker/` — Docker build environment
- `test/` — Test files

## Building

### Prerequisites
- ESP-IDF v5.5.x
- Node.js 18+ (for Angular web UI)
- ESP32-S3 toolchain

### Firmware Build
```bash
# Set your target board
export BOARD=NERDQAXEPLUS    # or NERDQAXEPLUS2 or NERDQX

# Configure and build
idf.py set-target esp32s3
idf.py build
```

### Web UI Build
```bash
cd main/http_server/axe-os
npm install --legacy-peer-deps
npm run build
```

> **Note:** You must delete the `build/` directory when switching between board targets.

## Upstream

Based on [ESP-Miner-NerdQAxePlus](https://github.com/shufps/ESP-Miner-NerdQAxePlus) by shufps.

## License

This source code is licensed under the [GNU General Public License v3.0](LICENSE).

See [NOTICE](NOTICE) for third-party attribution.

## Disclaimer

LottoAxe OS is independent custom firmware. It is **not affiliated** with NerdQaxe, NerdAxe, Qaxe, OSMU, Bitaxe, or any hardware manufacturer.

Solo mining is a lottery — there is no guarantee of finding a block.
