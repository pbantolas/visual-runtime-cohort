build_dir := "build"
macos_dir := "host/macos"
macos_build_dir := "build/macos"
macos_app := "build/macos/Build/Products/Debug/Host.app"

# configure + build everything
build:
    cmake -B {{build_dir}} -G Ninja -DCMAKE_BUILD_TYPE=Debug
    cmake --build {{build_dir}}

# generate compile_commands.json for clangd / IDE tooling
compile-commands:
    cmake -B {{build_dir}} -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    ln -sf {{build_dir}}/compile_commands.json compile_commands.json

# run the cli host (terminal 1)
run: build
    ./{{build_dir}}/host/cli/cli

# rebuild only the engine dylib — triggers hot reload in a running host (terminal 2)
reload:
    cmake --build {{build_dir}} --target engine

# build the macOS host app via xcodebuild
macos-build:
    #!/usr/bin/env bash
    set -euo pipefail
    args=(-workspace {{macos_dir}}/Host.xcworkspace -scheme Host -configuration Debug -derivedDataPath {{macos_build_dir}} build)
    if command -v xcbeautify &>/dev/null; then
        xcodebuild "${args[@]}" | xcbeautify
    else
        xcodebuild "${args[@]}"
    fi

# build and launch the macOS host app
macos-run: macos-build
    open {{macos_app}}

# open the macOS host workspace in Xcode
macos-open:
    open {{macos_dir}}/Host.xcworkspace

# wipe all build artifacts
clean:
    rm -rf {{build_dir}} {{macos_build_dir}}
