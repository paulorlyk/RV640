#!/bin/bash

set -ex

DIR="$(dirname "$0")"
cd "$DIR"

docker build \
  --progress=plain \
  -f build.Dockerfile \
  --target dist \
  --output type=local,dest=dist \
  .
