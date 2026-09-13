#!/usr/bin/env bash
# SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Builds the FFmpeg libraries shadPS4 needs, for Android.
#
# ext-ffmpeg-core ships prebuilt archives, but only for Windows, macOS and glibc Linux. Its
# CMakeLists selects by OS, so an Android build takes the UNIX branch and links the glibc build,
# which fails with undefined __errno_location, __isoc99_sscanf, __xpg_strerror_r and friends.
# There is no bionic archive to download, so build one.
#
# Usable standalone or through externals/ffmpeg-android/CMakeLists.txt.

set -euo pipefail

NDK=""
ABI="arm64-v8a"
API="28"
PREFIX=""
VERSION="7.1"
TARBALL=""
ASM="auto"
JOBS="$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)"

usage() {
    cat >&2 <<EOF
Usage: $0 --ndk <path> --prefix <path> [options]

  --ndk <path>      Android NDK root (required)
  --prefix <path>   Install prefix for the built libraries (required)
  --abi <abi>       arm64-v8a (default), armeabi-v7a, x86_64 or x86
  --api <level>     Minimum Android API level (default: $API)
  --version <ver>   FFmpeg version (default: $VERSION); must match ext-ffmpeg-core's headers
  --tarball <path>  Use this source tarball instead of downloading one
  --asm <auto|on|off>
                    Build the hand written assembly (default: auto -- on everywhere
                    except Windows; see the note in this script)
  --jobs <n>        Parallel jobs (default: $JOBS)
EOF
    exit 2
}

while [ $# -gt 0 ]; do
    case "$1" in
        --ndk) NDK="$2"; shift 2 ;;
        --abi) ABI="$2"; shift 2 ;;
        --api) API="$2"; shift 2 ;;
        --prefix) PREFIX="$2"; shift 2 ;;
        --version) VERSION="$2"; shift 2 ;;
        --tarball) TARBALL="$2"; shift 2 ;;
        --asm) ASM="$2"; shift 2 ;;
        --jobs) JOBS="$2"; shift 2 ;;
        -h|--help) usage ;;
        *) echo "unknown argument: $1" >&2; usage ;;
    esac
done

[ -n "$NDK" ] || { echo "--ndk is required" >&2; usage; }
[ -n "$PREFIX" ] || { echo "--prefix is required" >&2; usage; }
[ -d "$NDK" ] || { echo "NDK not found: $NDK" >&2; exit 1; }

case "$ABI" in
    arm64-v8a)   ARCH=aarch64; TRIPLE=aarch64-linux-android ;;
    armeabi-v7a) ARCH=arm;     TRIPLE=armv7a-linux-androideabi ;;
    x86_64)      ARCH=x86_64;  TRIPLE=x86_64-linux-android ;;
    x86)         ARCH=x86;     TRIPLE=i686-linux-android ;;
    *) echo "unsupported ABI: $ABI" >&2; exit 1 ;;
esac

# The NDK ships host toolchains under a platform-specific directory name.
HOST_TAG=""
for tag in linux-x86_64 darwin-x86_64 windows-x86_64; do
    if [ -d "$NDK/toolchains/llvm/prebuilt/$tag" ]; then HOST_TAG="$tag"; break; fi
done
[ -n "$HOST_TAG" ] || { echo "no prebuilt toolchain found under $NDK/toolchains/llvm/prebuilt" >&2; exit 1; }
BIN="$NDK/toolchains/llvm/prebuilt/$HOST_TAG/bin"

WORK="$(cd "$(dirname "$PREFIX")" && pwd)/ffmpeg-android-build"
mkdir -p "$WORK"

# FFmpeg's aarch64 and arm SIMD sources are .S files, which the compiler preprocesses before
# assembling; a .s file is assembled as-is. On a case insensitive filesystem those are the same
# name, so the assembler receives the unpreprocessed source and every use of the `function` and
# `endfunc` macros from libavutil/aarch64/asm.S fails with "unrecognized instruction mnemonic".
#
# NTFS can mark a directory case sensitive and new subdirectories inherit it, which fixes that --
# but the native Windows GNU make then cannot find the .S files at all, because it normalises
# case internally and asks for a lowercase .s that no longer exists. Building the assembly on
# Windows therefore also needs an MSYS2 or Cygwin make, so default to leaving it out there and
# let anyone who has one opt back in.
IS_WINDOWS=0
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*)
        IS_WINDOWS=1
        # configure writes probe files into TMPDIR and cannot cope with a Windows style path
        # containing backslashes.
        TMPDIR="$WORK/tmp"
        mkdir -p "$TMPDIR"
        export TMPDIR
        ;;
esac

if [ "$ASM" = "auto" ]; then
    if [ "$IS_WINDOWS" = "1" ]; then
        ASM="off"
        echo "note: building without assembly optimisations; a Windows host cannot produce them" >&2
        echo "      (see the comment in $0). Release artifacts should be built on Linux or CI." >&2
    else
        ASM="on"
    fi
fi

ASM_FLAGS=""
if [ "$ASM" = "off" ]; then
    ASM_FLAGS="--disable-asm"
elif [ "$IS_WINDOWS" = "1" ]; then
    if command -v fsutil.exe >/dev/null 2>&1 && command -v cygpath >/dev/null 2>&1; then
        fsutil.exe file setCaseSensitiveInfo "$(cygpath -w "$WORK")" enable >/dev/null 2>&1 \
            || echo "warning: could not enable case sensitivity on $WORK; assembly will fail" >&2
    fi
fi

SRC="$WORK/ffmpeg-$VERSION"
if [ ! -d "$SRC" ]; then
    if [ -z "$TARBALL" ]; then
        TARBALL="$WORK/ffmpeg-$VERSION.tar.xz"
        [ -f "$TARBALL" ] || curl -fsSL -o "$TARBALL" "https://ffmpeg.org/releases/ffmpeg-$VERSION.tar.xz"
    fi
    tar --force-local -xf "$TARBALL" -C "$WORK"
fi

# Matches the option set ext-ffmpeg-core applies in its vcpkg portfile patch, so the Android build
# exposes the same codecs as every other platform. Two names in that list are wrong and warn on
# every platform -- there is no `mov` decoder and no `h265` demuxer -- so use the real ones here.
# dshow is a Windows capture device and avdevice is not linked, so both are dropped.
cd "$SRC"
if [ ! -f ffbuild/config.mak ]; then
    ./configure \
        --prefix="$PREFIX" \
        --target-os=android --arch="$ARCH" --enable-cross-compile \
        --cc="$BIN/${TRIPLE}${API}-clang" --cxx="$BIN/${TRIPLE}${API}-clang++" \
        --ar="$BIN/llvm-ar" --nm="$BIN/llvm-nm" \
        --ranlib="$BIN/llvm-ranlib" --strip="$BIN/llvm-strip" \
        --enable-pic --disable-doc --enable-runtime-cpudetect --disable-autodetect $ASM_FLAGS \
        --disable-shared --enable-static --disable-programs --disable-avdevice \
        --disable-everything \
        --enable-decoder=aac --enable-decoder=aac_latm --enable-decoder=atrac3 \
        --enable-decoder=atrac3p --enable-decoder=atrac9 --enable-decoder=mp3 \
        --enable-decoder=pcm_s16le --enable-decoder=pcm_s8 \
        --enable-decoder=h264 --enable-decoder=mpeg4 --enable-decoder=mpeg2video \
        --enable-decoder=mjpeg --enable-decoder=mjpegb --enable-decoder=hevc \
        --enable-encoder=pcm_s16le --enable-encoder=ffv1 --enable-encoder=mpeg4 \
        --enable-encoder=ljpeg --enable-encoder=mjpeg \
        --enable-muxer=avi \
        --enable-demuxer=h264 --enable-demuxer=hevc --enable-demuxer=m4v --enable-demuxer=mp3 \
        --enable-demuxer=mpegvideo --enable-demuxer=mpegps --enable-demuxer=mjpeg \
        --enable-demuxer=mov --enable-demuxer=avi --enable-demuxer=aac --enable-demuxer=pmp \
        --enable-demuxer=oma --enable-demuxer=pcm_s16le --enable-demuxer=pcm_s8 \
        --enable-demuxer=wav \
        --enable-parser=h264 --enable-parser=mpeg4video --enable-parser=mpegaudio \
        --enable-parser=mpegvideo --enable-parser=mjpeg --enable-parser=aac \
        --enable-parser=aac_latm \
        --enable-protocol=file --enable-bsf=mjpeg2jpeg
fi

MAKE=make
command -v make >/dev/null 2>&1 || MAKE=mingw32-make

# -f Makefile is not redundant: GNU make probes GNUmakefile, then makefile, then Makefile, and the
# native Windows build gives up when the lowercase probe returns ENOENT instead of continuing --
# which it does here precisely because the work directory was made case sensitive above.
"$MAKE" -f Makefile -j"$JOBS"
"$MAKE" -f Makefile install

echo "FFmpeg $VERSION for $ABI (API $API) installed to $PREFIX"
