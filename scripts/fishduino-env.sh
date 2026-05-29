#!/usr/bin/env bash
# Source ESP-IDF 5.5.4 and verify idf.py is available.
set -euo pipefail

IDF_EXPORT="${HOME}/.espressif/v5.5.4/esp-idf/export.sh"

if [[ ! -f "${IDF_EXPORT}" ]]; then
    echo "error: ESP-IDF export script not found at:" >&2
    echo "  ${IDF_EXPORT}" >&2
    echo "Install ESP-IDF v5.5.4 or adjust IDF_EXPORT in this script." >&2
    exit 1
fi

# shellcheck source=/dev/null
source "${IDF_EXPORT}"

if ! command -v idf.py >/dev/null 2>&1; then
    echo "error: idf.py not on PATH after sourcing ESP-IDF" >&2
    exit 1
fi

idf.py --version
