#!/usr/bin/env bash
# Select Fishduino hardware target (4" DSI dev board or 7B integrated LCD) and build.
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

echo "==> Fishduino target: ${TARGET}"

if [[ -f sdkconfig ]]; then
    BACKUP="sdkconfig.backup.$(date +%Y%m%d_%H%M%S)"
    echo "==> Backing up existing sdkconfig to ${BACKUP}"
    cp sdkconfig "${BACKUP}"
fi

echo "==> Writing sdkconfig.defaults (common + ${TARGET})"
cat "${PROJECT_DIR}/sdkconfig.defaults.common" \
    "${PROJECT_DIR}/sdkconfig.defaults.${TARGET}" > "${PROJECT_DIR}/sdkconfig.defaults"

echo "==> Installing main/idf_component.yml for ${TARGET}"
cp "${DEPS_DIR}/idf_component.full.${TARGET}.yml" "${PROJECT_DIR}/main/idf_component.yml"

if [[ -d managed_components ]]; then
    echo "==> Note: removing managed_components/ so the Component Manager fetches the correct BSP"
    rm -rf managed_components dependencies.lock
fi

rm -f sdkconfig

echo "==> idf.py set-target esp32p4"
idf.py set-target esp32p4

if [[ "${RECONFIGURE_ONLY}" -eq 1 ]]; then
    echo "==> Reconfigure complete for target ${TARGET}"
    exit 0
fi

echo "==> idf.py build"
idf.py build

echo "==> Build OK (${TARGET}): ${PROJECT_DIR}/build/fishduino.bin"
echo "    Flash: idf.py -p /dev/ttyACM0 flash monitor"
