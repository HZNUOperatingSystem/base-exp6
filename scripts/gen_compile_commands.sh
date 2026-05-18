#!/bin/sh

set -eu

cd "$(dirname "$0")/.."

if [ "${GEN_COMPILE_COMMANDS_FROM_MAKE:-0}" != 1 ]; then
    exec make compile_commands
fi

root=$(pwd -P)
out=${COMPILE_COMMANDS_OUT:-compile_commands.json}
tmp="${out}.$$"
sources="${tmp}.sources"
first=1

trap 'rm -f "$tmp" "$sources"' EXIT HUP INT TERM

json_escape() {
    printf '%s' "$1" | sed 's/\\/\\\\/g; s/"/\\"/g'
}

emit_entry() {
    file=$1
    command=$2

    if [ "$first" -eq 1 ]; then
        first=0
    else
        printf ',\n' >> "$tmp"
    fi

    printf '  {\n' >> "$tmp"
    printf '    "directory": "%s",\n' "$(json_escape "$root")" >> "$tmp"
    printf '    "file": "%s",\n' "$(json_escape "$file")" >> "$tmp"
    printf '    "command": "%s"\n' "$(json_escape "$command")" >> "$tmp"
    printf '  }' >> "$tmp"
}

emit_sources() {
    dir=$1
    flags=$2

    find "$dir" \( -name '*.c' -o -name '*.S' \) -type f | sort > "$sources"
    while IFS= read -r file; do
        obj="$BUILD_DIR/${file%.*}.o"
        emit_entry "$file" "$flags -c -o $obj $file"
    done < "$sources"
}

: "${BUILD_DIR:?}"
: "${CLANGD_CC:?}"
: "${CLANGD_TARGET:?}"
: "${CLANGD_RESOURCE_INCLUDE:?}"
: "${CFLAGS:?}"
: "${KERNEL_CFLAGS:?}"
: "${USER_CFLAGS:?}"
: "${HOST_CC:?}"
: "${MKFS_CFLAGS:?}"

xv6_common="$CLANGD_CC --target=$CLANGD_TARGET $CFLAGS"
xv6_common="$xv6_common -nostdinc -isystem $CLANGD_RESOURCE_INCLUDE"

kernel_flags="$xv6_common $KERNEL_CFLAGS"
user_flags="$xv6_common $USER_CFLAGS"
tool_flags="$HOST_CC -Wno-unknown-attributes $MKFS_CFLAGS"

printf '[\n' > "$tmp"
emit_sources kernel "$kernel_flags"
emit_sources user "$user_flags"

if [ -f tools/mkfs.c ]; then
    emit_entry tools/mkfs.c "$tool_flags -c -o $BUILD_DIR/mkfs/mkfs.o tools/mkfs.c"
fi

printf '\n]\n' >> "$tmp"
mv "$tmp" "$out"
rm -f "$sources"
trap - EXIT HUP INT TERM
