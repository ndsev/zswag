#!/usr/bin/env bash

my_dir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
. "$my_dir/../../../deps/python-cmake-wheel/python.bash"

venv=$(mktemp -d)
echo "→ Setting up a virtual environment in $venv ..."
python3 -m venv "$venv"
source "$venv/$activate_path"
python -m pip install -U pip

trap 'echo "→ Removing $venv"; rm -rf "$venv"' EXIT

while [[ $# -gt 0 ]]; do
  case $1 in
    -w|--wheels-dir)
      echo "→ Installing wheels from $2 ..."
      pip install "$2"/*
      shift
      shift
      ;;
  esac
done

echo "→ [Test 1/4] Generate with auto-translation ..."
python -m zswag.gen \
  --service calculator.Calculator \
  --input "$my_dir/calc/calculator.zs" \
  --config get,path,flat bitMul:post,body \
  --config identity:put \
  --config "byteSum:path=/byte_sum_endpoint,values?format=base64&name=data&in=query" \
  --output "$my_dir/.test.yaml"
diff -w "$my_dir/.test.yaml" "$my_dir/test_openapi_generator_1.yaml"

echo "→ [Test 2/4] Generate with Python source ..."
python -m zswag.gen \
  --service calculator.Calculator \
  --input "$(python -m zswag.test.calc path)" \
  --config put "*?name=data&in=query&format=base64" \
  --output "$my_dir/.test.yaml"
diff -w "$my_dir/.test.yaml" "$my_dir/test_openapi_generator_2.yaml"

echo "→ [Test 3/4] Generate with base_config ..."
python -m zswag.gen \
  --service calculator.Calculator \
  --input "$my_dir/calc/calculator.zs" \
  --base-config "$my_dir/test_openapi_generator_base_config.yaml" \
  --output "$my_dir/.test.yaml"
diff -w "$my_dir/.test.yaml" "$my_dir/calc/api.yaml"

echo "→ [Test 4/4] Generate with zserio root-dir ..."
python -m zswag.gen \
  --service test_nested_service.services.MyService \
  --input "test_nested_service/services.zs" \
  --zserio-source-root "$my_dir" \
  --output "$my_dir/.test.yaml"
diff -w "$my_dir/.test.yaml" "$my_dir/test_openapi_generator_3.yaml"
