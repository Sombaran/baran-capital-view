#!/usr/bin/env bash
set -euo pipefail

build_dir=${1:?build directory is required}
output_dir=${2:?output directory is required}

rm -f "$output_dir"/*.gcov

while IFS= read -r -d '' data_file; do
  object_file=${data_file%.gcda}.o
  [[ -f "$object_file" ]] || continue
  gcov -b -c -f -o "$(dirname "$data_file")" "$object_file" \
    >"$output_dir/$(basename "$data_file").txt"
  find . -maxdepth 1 -name '*.gcov' -exec mv {} "$output_dir"/ \;
done < <(find "$build_dir" -type f -name '*.gcda' -print0)

if ! find "$output_dir" -type f -name '*.gcov' -print -quit | grep -q .; then
  echo "No gcov data was produced. Run the instrumented tests first." >&2
  exit 1
fi

echo "Raw gcov reports: $output_dir"