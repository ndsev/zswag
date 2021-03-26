#!/usr/bin/env bash

setupfile=${1:-setup.py}

rm -rf dist
twine upload dist/*
