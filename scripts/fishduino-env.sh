#!/usr/bin/env bash
# Source ESP-IDF 5.5.4 and verify idf.py is available.
# Works locally (~/.espressif) and in GitHub Actions (IDF_PATH from install-esp-idf-action).
set -euo pipefail

if ! command -v idf.py >/dev/null 2>&1; then
    IDF_EXPORT=""

    if [[ -n "${IDF_PATH:-}" && -f "${IDF_PATH}/export.sh" ]]; then
        IDF_EXPORT="${IDF_PATH}/export.sh"
    elif [[ -f "${HOME}/.espressif/v5.5.4/esp-idf/export.sh" ]]; then
        IDF_EXPORT="${HOME}/.espressif/v5.5.4/esp-idf/export.sh"
    fi

    if [[ -z "${IDF_EXPORT}" ]]; then
        echo "error: ESP-IDF export script not found." >&2
        echo "  Set IDF_PATH to your ESP-IDF tree, or install v5.5.4 under:" >&2
        echo "  ${HOME}/.espressif/v5.5.4/esp-idf/export.sh" >&2
        exit 1
    fi

    # shellcheck source=/dev/null
    source "${IDF_EXPORT}"
fi

if ! command -v idf.py >/dev/null 2>&1; then
    echo "error: idf.py not on PATH after sourcing ESP-IDF" >&2
    exit 1
fi

idf.py --version
