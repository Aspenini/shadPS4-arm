<!--
SPDX-FileCopyrightText: 2026 shadPS4 Emulator Project
SPDX-License-Identifier: GPL-2.0-or-later
-->

## Build shadPS4 for Android

> [!IMPORTANT]
> This produces a binary, not a working emulator. shadPS4 executes guest PS4 x86-64 code
> **natively** on the host CPU — `Core::RunMainEntry` jumps straight into it — so on ARM there is
> nothing to run that code. Building for Android is groundwork; a CPU backend is what would make
> games run, and that does not exist yet.

### Prerequisites

- **Android NDK r27 or newer.** shadPS4 is C++23, and 16 KB page support needs r27+. Install it
  through Android Studio: `Tools -> SDK Manager -> SDK Tools`, tick **NDK (Side by side)** and
  **CMake**.
- **CMake 3.24+** and **Ninja**.
- **bash**, to run FFmpeg's `configure` script. Git for Windows and MSYS2 both provide one.

### Building

```sh
cmake -S . -B build-android -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake \
    -DANDROID_ABI=arm64-v8a \
    -DANDROID_PLATFORM=android-28 \
    -DCMAKE_BUILD_TYPE=Release
cmake --build build-android
```

`arm64-v8a` is the only ABI that gets any attention. The others are wired up but untested.

### FFmpeg

`ext-ffmpeg-core` ships prebuilt archives and picks them by operating system. Android matches its
`UNIX` branch, so it would link the glibc Linux build and fail with undefined `__errno_location`,
`__isoc99_sscanf`, `__xpg_strerror_r`, `__fprintf_chk` and `fcntl64`. No Android archive is
published, so the build compiles FFmpeg from source instead, using
`externals/ffmpeg-android/build-ffmpeg.sh`. This happens automatically and takes a few minutes the
first time.

To reuse an existing build — which is what CI should do rather than rebuilding FFmpeg per job:

```sh
-DSHADPS4_FFMPEG_ANDROID_PREFIX=/path/to/ffmpeg/install
```

The script can also be run on its own:

```sh
externals/ffmpeg-android/build-ffmpeg.sh --ndk "$ANDROID_NDK" --prefix /tmp/ffmpeg-android
```

> [!NOTE]
> On Windows the FFmpeg build leaves out the hand written assembly, so decoding is slower than it
> should be. FFmpeg's SIMD sources are `.S` files, which must be preprocessed before assembly;
> on a case insensitive filesystem `.S` and `.s` are the same file, so the assembler gets
> unpreprocessed source. Marking the directory case sensitive fixes that, but the native Windows
> GNU make then cannot find the files, because it normalises case internally. Building the
> assembly needs a case sensitive filesystem *and* an MSYS2 or Cygwin make — pass `--asm on` to
> the script if you have both. Release artifacts should be built on Linux or in CI.

### Known limitations

Beyond the absence of a CPU backend:

- **`libSceFiber` is unavailable.** `fiber_context.cpp` is x86-64 assembly and is excluded from
  the build. It should not be ported directly: the context it saves is the *guest's* x86 state, so
  it belongs behind a CPU backend.
- **The shader recompiler's SRT walker is x86-64 only.** `FlattenExtendedUserdataPass` compiles a
  walker with Xbyak and is `UNREACHABLE` elsewhere, so any shader using extended user data will
  abort. It also serialises raw x86 machine code into the pipeline cache, which SELinux will not
  let an app map executable.
- **Guest signal contexts are empty.** The guest `mcontext` is FreeBSD amd64 shaped; on a native
  ARM64 host there are no guest registers to report until a CPU backend supplies them.
