#!/usr/bin/env bash

setupfile=${1:-setup.py}

rm -rf dist
python3 $setupfile bdist_wheel
twine upload dist/*
