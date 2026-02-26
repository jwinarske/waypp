#!/usr/bin/env bash

set -euo pipefail

MYPATH="$(readlink -f "${BASH_SOURCE[0]}")"
MYDIR="${MYPATH%/*}"
MYNAME="${MYPATH##*/}"

TARGET_PATH="${1:-}"
WORK_DIR="${2:-$(pwd)}"
BUILD_DIR="${3:-$(readlink -f "${WORK_DIR}/../../build")}"
LOG_FILE="tidy-results-log.txt"
FIX_FILE="tidy-results-suggested-fixes.txt"

print_results() {
  [ -f "${WORK_DIR}/${LOG_FILE}" ] && cat "${WORK_DIR}/${LOG_FILE}"
  [ -f "${WORK_DIR}/${FIX_FILE}" ] && cat "${WORK_DIR}/${FIX_FILE}"
}

trap print_results EXIT

if [ -z "${TARGET_PATH}" ]; then
  echo "Usage: ${MYNAME} <target-path> [work-dir] [build-dir]"
  echo "Example: ${MYNAME} plugin_a ./plugins_source/plugins ./build"
  exit 1
fi

if ! command -v run-clang-tidy-18 >/dev/null 2>&1; then
  echo "run-clang-tidy-18 not found in PATH"
  exit 1
fi

if [ ! -d "${WORK_DIR}" ]; then
  echo "work-dir does not exist: ${WORK_DIR}"
  exit 1
fi

if [ ! -d "${BUILD_DIR}" ]; then
  echo "build-dir does not exist: ${BUILD_DIR}"
  exit 1
fi

pushd "${WORK_DIR}" >/dev/null

if [ ! -e "${TARGET_PATH}" ]; then
  echo "target-path does not exist in work-dir: ${TARGET_PATH}"
  popd >/dev/null
  exit 1
fi

mapfile -t FILES < <(
  find "${TARGET_PATH}" \
    -type d -name third_party -prune -o \
    -type f \( -name '*.cc' -o -name '*.hpp' -o -name '*.h' \) -print
)

if [ ${#FILES[@]} -eq 0 ]; then
  echo "No files found for clang-tidy under: ${TARGET_PATH}"
  popd >/dev/null
  exit 0
fi

printf '%s\n' "${FILES[@]}"

run-clang-tidy-18 \
  -warnings-as-errors='*,-bugprone-macro-parentheses' \
  -export-fixes="${FIX_FILE}" \
  -p "${BUILD_DIR}" \
  "${FILES[@]}" \
  2>/dev/null \
  | tee "${LOG_FILE}"

if [ -f "${FIX_FILE}" ] && grep -q "Level: Error" "${FIX_FILE}"; then
  popd >/dev/null
  exit 1
fi

popd >/dev/null