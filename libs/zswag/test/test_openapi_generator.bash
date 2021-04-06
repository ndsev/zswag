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

test_zs_path=$(python -m zswag.test.calc path)

python -m zswag.gen \
  --service calculator.calculator.Calculator.Service \
  --path "$test_zs_path" \
  --zs "$test_zs_path" \
  --config get,path,flat \
  --output "$my_dir/.test.yaml"
cmp -l "$my_dir/.test.yaml" "$my_dir/test_openapi_generator_1.yaml"
