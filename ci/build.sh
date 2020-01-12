#!/usr/bin/env sh

cmake -D STATIC_BUILD=on .
make
strip deepextract