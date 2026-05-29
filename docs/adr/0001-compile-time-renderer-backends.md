# Compile-time renderer backends

The visual runtime is built with one renderer backend selected by `VISUAL_RUNTIME_BACKEND` rather than containing multiple backends chosen at runtime. This keeps the harness independent of renderer backend choice, preserves the existing hot-reload dylib shape, and avoids introducing a backend abstraction before the course needs one.
