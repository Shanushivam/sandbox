#!/bin/bash
# Populates ./rootfs with a few host binaries (plus their shared libraries)
# and the built example programs, enough to run them inside the sandbox.
set -e
cd "$(dirname "$0")/.."

ROOTFS=rootfs
BINARIES=(sh ls cat echo grep head tail tr touch mkdir rm sleep ps id hostname env)

copy_with_libs() {
    local source="$1" dest="$2"
    mkdir -p "$(dirname "$dest")"
    cp -L "$source" "$dest"
    # ldd prints the absolute path of each shared library the binary needs.
    ldd "$source" 2>/dev/null | grep -o '/[^ ]*' | while read -r lib; do
        mkdir -p "$ROOTFS$(dirname "$lib")"
        cp -L "$lib" "$ROOTFS$lib"
    done
}

if [ "$(uname -s)" != "Linux" ]; then
    echo "setup_rootfs.sh must be run on Linux." >&2
    exit 1
fi

mkdir -p "$ROOTFS"/{bin,dev,proc,tmp}

for name in "${BINARIES[@]}"; do
    path=$(type -P "$name" || true)
    if [ -z "$path" ]; then
        echo "warning: $name not found on host, skipping" >&2
        continue
    fi
    copy_with_libs "$path" "$ROOTFS/bin/$name"
done

if [ -d build/examples ]; then
    for example in build/examples/*; do
        [ -x "$example" ] && copy_with_libs "$example" "$ROOTFS/examples/$(basename "$example")"
    done
else
    echo "note: build the project first to include the example programs." >&2
fi

echo "rootfs ready: $ROOTFS"
