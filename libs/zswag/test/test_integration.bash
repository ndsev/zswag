#!/usr/bin/env bash

set -euo pipefail

# On windows, there is no python3 executable
if [[ "$OSTYPE" == "msys" ]]; then
    function python3 {
        python "$@"
    }

    function pip3 {
        pip "$@"
    }
    activate_path="Scripts/activate"
else
    activate_path="bin/activate"
fi

venv=$(mktemp -d)
echo "→ Setting up a virtual environment in $venv ..."
python3 -m venv "$venv"
source "$venv/$activate_path"
pip install -U pip

trap 'echo "→ Killing $(jobs -p)"; kill $(jobs -p); echo "→ Removing $venv"; rm -rf "$venv"' EXIT

while [[ $# -gt 0 ]]; do
  case $1 in
    -w|--wheels-dir)
      echo "→ Installing wheels from $2 ..."
      pip install "$2"/*
      shift
      shift
      ;;
    -b|--background)
      echo "→ Launching $2 ..."
      $2 &
      sleep 10
      shift
      shift
      ;;
    -f|--foreground)
      echo "→ Starting $2 ..."
      $2
      shift
      shift
      ;;
  esac
done
