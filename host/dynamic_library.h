#pragma once

#include <dlfcn.h>
#include <filesystem>
#include <cstdio>

namespace fs = std::filesystem;

struct DynamicLibrary {
    DynamicLibrary() = default;
    DynamicLibrary(const DynamicLibrary&) = delete;
    DynamicLibrary& operator=(const DynamicLibrary&) = delete;
    DynamicLibrary(DynamicLibrary&& o) noexcept
        : handle(o.handle), last_modified(o.last_modified),
          source(std::move(o.source)), active(std::move(o.active))
    { o.handle = nullptr; }

    ~DynamicLibrary() { unload(); }

    static DynamicLibrary open(const char* source_path) {
        DynamicLibrary lib;
        lib.source = source_path;
        lib.active = lib.source;
        lib.load();
        return lib;
    }

    bool changed() const {
        std::error_code ec;
        auto t = fs::last_write_time(source, ec);
        return !ec && t != last_modified;
    }

    bool reload() {
        unload();
        return load();
    }

    void* sym(const char* name) const {
        if (!handle) return nullptr;
        void* s = dlsym(handle, name);
        if (!s) std::fprintf(stderr, "[dynamic_library] symbol not found: %s\n", name);
        return s;
    }

    explicit operator bool() const { return handle != nullptr; }

private:
    void* handle         = nullptr;
    fs::file_time_type last_modified;
    fs::path source;
    fs::path active;

    bool load() {
        handle = dlopen(active.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (!handle) {
            std::fprintf(stderr, "[dynamic_library] dlopen: %s\n", dlerror());
            return false;
        }
        last_modified = fs::last_write_time(source);
        return true;
    }

    void unload() {
        if (handle) { dlclose(handle); handle = nullptr; }
    }
};
