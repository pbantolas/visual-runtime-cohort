build_dir := "build"
backend := "Metal"
macos_dir := "host/macos"
macos_build_dir := "build/macos"
macos_app := "build/macos/Build/Products/Debug/Host.app"
macos_project := "host/macos/Host.xcodeproj"
macos_workspace := "host/macos/Host.xcworkspace"

default:
    @just --list

[private]
configure backend=backend:
    cmake -B {{build_dir}} -G Ninja -DCMAKE_BUILD_TYPE=Debug -DENGINE_BACKEND={{backend}}

# configure + build everything
build backend=backend: (configure backend)
    cmake --build {{build_dir}}

# generate compile_commands.json for clangd / IDE tooling
compile-commands backend=backend:
    cmake -B {{build_dir}} -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DENGINE_BACKEND={{backend}}
    ln -sf {{build_dir}}/compile_commands.json compile_commands.json

# run the cli host (terminal 1)
run backend=backend: (build backend)
    ./{{build_dir}}/host/cli/cli

# build the engine dylib — run in a second terminal to trigger hot reload in a running host
engine backend=backend: (configure backend)
    cmake --build {{build_dir}} --target engine

[private]
ensure-macos-project:
    #!/usr/bin/env bash
    set -euo pipefail
    if [[ ! -d "{{macos_project}}" || ! -d "{{macos_workspace}}" ]]; then
        echo "error: checked-in macOS project files are missing." >&2
        echo "Install Tuist and run 'just macos-generate-project' to regenerate them." >&2
        exit 1
    fi

# generate the macOS Xcode project from Tuist
macos-generate-project:
    #!/usr/bin/env bash
    set -euo pipefail
    command -v tuist >/dev/null || {
        echo "error: tuist is required for macOS recipes. Install it, then rerun this command." >&2
        exit 127
    }
    cd {{macos_dir}}
    tuist generate --no-open

# build the macOS host app via xcodebuild
macos-build: ensure-macos-project
    #!/usr/bin/env bash
    set -euo pipefail
    args=(-project {{macos_project}} -scheme Host -configuration Debug -derivedDataPath {{macos_build_dir}} build)
    if command -v xcbeautify &>/dev/null; then
        xcodebuild "${args[@]}" | xcbeautify
    else
        xcodebuild "${args[@]}"
    fi

# build and launch the macOS host app
macos-run: engine macos-build
    open {{macos_app}}

# open the macOS host workspace in Xcode
macos-open: ensure-macos-project
    open {{macos_workspace}}

# wipe all build artifacts
clean:
    rm -rf {{build_dir}} compile_commands.json
