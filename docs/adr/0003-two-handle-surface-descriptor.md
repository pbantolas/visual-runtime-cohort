# Two-handle surface descriptor

Linux support will extend `SurfaceDescriptor` with `display_handle` and `surface_handle` fields rather than passing a pointer to variant-specific descriptor structs. `display_handle` remains pointer-shaped, while `surface_handle` is integer-shaped so pointer-backed surfaces and X11 window IDs can both cross the engine API boundary without extra indirection. Each `SurfaceKind` defines which fields are meaningful; unused fields are explicitly null or ignored for that variant.
