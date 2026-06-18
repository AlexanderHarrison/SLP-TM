#! /usr/bin/env bash

set -Eeo pipefail

mkdir -p build

# ensure an iso was passed
if [ -z "${1}" ]; then
    echo "Usage:"
    echo "    $0 <path/to/vanilla/melee.iso> [release]"
    exit 1
fi
iso="${1}"

if [ "${2}" = "release" ]; then
    release=true
elif [ -n "${2}" ]; then
    mode="${2}"
fi

if [ ! -f "${iso}" ]; then
    echo "Error: path '${iso}' does not exist!"
    exit 1
fi

if [[ "$(uname)" =~ "MSYS" ]]; then
    gc_fst="bin/gc_fst.exe"
    hgecko="bin/hgecko.exe"
    hmex="bin/hmex.exe"
    xdelta="bin/xdelta.exe"
else
    gc_fst="bin/gc_fst"
    hgecko="bin/hgecko"
    hmex="bin/hmex"
    xdelta="xdelta3"
fi

# build src
warn="-Wall -Wextra -Wno-char-subscripts -Wno-builtin-declaration-mismatch -Wno-unused-parameter"
if [ "${release}" = true ]; then
    opt="-O2"
else
    opt="-DDEBUG"
fi
${hmex} -q -l "MexTK/melee.link" -f "${warn} ${opt}" -s "fns" -t "src/fns" -o "build/slp.dat" -i src/slp.c

# build ASM
${hgecko} ASM build/codes.gct

# copy iso over
if [ ! -f SLP.iso ]; then cp "${iso}" SLP.iso; fi

# add files to iso
${gc_fst} fs SLP.iso \
    delete MvHowto.mth \
    delete MvOmake15.mth \
    insert codes.gct build/codes.gct \
    insert slp.dat build/slp.dat

echo "built SLP.iso"
