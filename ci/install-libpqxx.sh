#!/usr/bin/env bash
# Собирает и устанавливает libpqxx из исходников.
# Нужен на Ubuntu: пакет libpqxx-dev там старее, чем версия, на которой проверялся проект.
#   ci/install-libpqxx.sh <версия> <каталог установки>
set -euo pipefail

VERSION="${1:?укажите версию libpqxx, например 8.0.2}"
PREFIX="${2:?укажите каталог установки}"

if [ -f "$PREFIX/lib/cmake/libpqxx/libpqxx-config.cmake" ]; then
    echo "libpqxx $VERSION уже установлена в $PREFIX"
    exit 0
fi

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

curl -fsSL "https://github.com/jtv/libpqxx/archive/refs/tags/${VERSION}.tar.gz" \
    | tar -xz -C "$WORK"

cmake -S "$WORK/libpqxx-${VERSION}" -B "$WORK/build" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$PREFIX" \
    -DCMAKE_CXX_STANDARD=20 \
    -DBUILD_SHARED_LIBS=OFF \
    -DBUILD_TEST=OFF \
    -DBUILD_DOC=OFF
cmake --build "$WORK/build" --parallel
cmake --install "$WORK/build"
