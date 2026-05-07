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

# wipe the build directory
clean:
    rm -rf {{build_dir}}
