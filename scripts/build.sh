#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

# shellcheck source=fishduino-env.sh
source "${SCRIPT_DIR}/fishduino-env.sh"

cd "${PROJECT_DIR}"
exec "${SCRIPT_DIR}/build-target.sh" 4in

echo "Build OK: ${PROJECT_DIR}/build/fishduino.bin"
