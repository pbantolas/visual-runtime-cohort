build_dir := "build"

# configure + build everything
build:
    cmake -B {{build_dir}} -G Ninja -DCMAKE_BUILD_TYPE=Debug
    cmake --build {{build_dir}}

# build only the engine dylib (configures if needed)
dylib:
    cmake -B {{build_dir}} -G Ninja -DCMAKE_BUILD_TYPE=Debug
    cmake --build {{build_dir}} --target engine

# run the cli host (terminal 1)
run: build
    ./{{build_dir}}/host/cli/cli

# rebuild only the engine dylib — triggers hot reload in a running host (terminal 2)
reload:
    cmake --build {{build_dir}} --target engine

macos_dir := "host/macos"
macos_build_dir := "build/macos"
macos_app := "build/macos/Build/Products/Debug/Host.app"

# build the macOS host app via xcodebuild
macos-build:
    xcodebuild -workspace {{macos_dir}}/Host.xcworkspace \
               -scheme Host \
               -configuration Debug \
               -derivedDataPath {{macos_build_dir}} \
               build

# build and launch the macOS host app
macos-run: macos-build
    open {{macos_app}}

# open the macOS host workspace in Xcode
macos-open:
    open {{macos_dir}}/Host.xcworkspace

# wipe the build directory
clean:
    rm -rf {{build_dir}}
