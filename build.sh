#!/bin/sh
# Builds the Sigma compiler into build/sig.
#   ./build.sh            build
#   ./build.sh install    build, then copy `sig` to /usr/local/bin (or $PREFIX/bin)
set -e
cd "$(dirname "$0")"

CXX="${CXX:-c++}"
mkdir -p build/gen

# Embed the C runtime into the compiler binary.
{
  printf 'static const char* const SIGMA_RUNTIME = R"SIGRT('
  cat runtime/sigma_runtime.h
  printf ')SIGRT";\n'
} > build/gen/runtime_embed.h

echo "building build/sig ..."
"$CXX" -std=c++17 -O2 -Wall -Wextra -Ibuild/gen compiler/*.cpp -o build/sig
echo "done: $(./build/sig version)"

if [ "$1" = "install" ]; then
  PREFIX="${PREFIX:-/usr/local}"
  mkdir -p "$PREFIX/bin" 2>/dev/null || true
  if [ -w "$PREFIX/bin" ]; then
    cp build/sig "$PREFIX/bin/sig"
  else
    echo "installing to $PREFIX/bin needs admin rights:"
    sudo cp build/sig "$PREFIX/bin/sig"
  fi
  echo "installed $PREFIX/bin/sig"
fi
