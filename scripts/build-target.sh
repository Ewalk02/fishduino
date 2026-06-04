#!/usr/bin/env bash
# Select AquaPilot hardware target (4" DSI dev board or 7B integrated LCD) and build.
set -euo pipefail

usage() {
    echo "Usage: $0 <4in|7b> [--reconfigure-only]"
    echo ""
    echo "  4in  Waveshare ESP32-P4-WIFI6 + 4-DSI-TOUCH-A (480x800)"
    echo "  7b   Waveshare ESP32-P4-WIFI6-7inch-Touch-LCD (B) (1024x600)"
    exit 1
}

TARGET="${1:-}"
RECONFIGURE_ONLY=0
if [[ "${2:-}" == "--reconfigure-only" ]]; then
    RECONFIGURE_ONLY=1
fi

case "${TARGET}" in
    4in|7b) ;;
    *) usage ;;
esac

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
DEPS_DIR="${PROJECT_DIR}/dependencies"

# shellcheck source=fishduino-env.sh
source "${SCRIPT_DIR}/fishduino-env.sh"

cd "${PROJECT_DIR}"

echo "==> AquaPilot target: ${TARGET}"

if [[ -f sdkconfig ]]; then
    BACKUP="sdkconfig.backup.$(date +%Y%m%d_%H%M%S)"
    echo "==> Backing up existing sdkconfig to ${BACKUP}"
    cp sdkconfig "${BACKUP}"
fi

echo "==> Writing sdkconfig.defaults (common + ${TARGET})"
cat "${PROJECT_DIR}/sdkconfig.defaults.common" \
    "${PROJECT_DIR}/sdkconfig.defaults.${TARGET}" > "${PROJECT_DIR}/sdkconfig.defaults"

echo "==> Installing idf_component.yml for ${TARGET} (main/ and project root)"
cp "${DEPS_DIR}/idf_component.full.${TARGET}.yml" "${PROJECT_DIR}/main/idf_component.yml"
cp "${DEPS_DIR}/idf_component.full.${TARGET}.yml" "${PROJECT_DIR}/idf_component.yml"

if [[ -d managed_components ]]; then
    echo "==> Note: removing managed_components/ so the Component Manager fetches the correct BSP"
    rm -rf managed_components dependencies.lock
fi

rm -f sdkconfig

echo "==> idf.py set-target esp32p4"
idf.py set-target esp32p4

# Managed-component Kconfig (esp_wifi_remote slave target, etc.) is not fully
# applied on the first pass; reconfigure so sdkconfig matches component defaults.
echo "==> idf.py reconfigure (apply esp_wifi_remote / ESP-Hosted Kconfig)"
idf.py reconfigure

if ! grep -q '^CONFIG_SLAVE_IDF_TARGET_ESP32C6=y' sdkconfig; then
    echo "ERROR: CONFIG_SLAVE_IDF_TARGET_ESP32C6 is not set in sdkconfig."
    echo "       ESP-Hosted needs the onboard ESP32-C6 slave target (SDIO)."
    grep -E 'CONFIG_ESP_WIFI_REMOTE_ENABLED|CONFIG_ESP_HOST_WIFI_ENABLED|CONFIG_ESP_HOSTED_ENABLED' sdkconfig || true
    exit 1
fi

if [[ "${RECONFIGURE_ONLY}" -eq 1 ]]; then
    echo "==> Reconfigure complete for target ${TARGET}"
    exit 0
fi

echo "==> idf.py build"
idf.py build

echo "==> Build OK (${TARGET}): ${PROJECT_DIR}/build/fishduino.bin"
echo "    Flash: idf.py -p /dev/ttyACM0 flash monitor"
