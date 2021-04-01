#!/usr/bin/env bash

my_dir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
. "$my_dir/../../../deps/python-cmake-wheel/python.bash"

venv=$(mktemp -d)
echo "→ Setting up a virtual environment in $venv ..."
python3 -m venv "$venv"
source "$venv/$activate_path"
python -m pip install -U pip

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

exit 0
