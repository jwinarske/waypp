#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 || $# -gt 2 ]]; then
  echo "Usage: $0 <name> [working_directory]" >&2
  echo "Example: $0 my_plugin plugins_source/plugins" >&2
  exit 2
fi

name="$1"
workdir="${2:-.}"
file_list="clang-format-files"

cd "$workdir"

find "$name" -type d -name third_party -prune -false -o -name '*.cc' -o -name '*.hpp' -o -name '*.h' > "$file_list"

echo "Formatting files:"
cat "$file_list"

which clang-format-19
clang-format-19 --version
clang-format-19 -i --files="$file_list"