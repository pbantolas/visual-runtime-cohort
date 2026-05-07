import QuartzCore

public final class RuntimeManager {
    private let bridge: OpaquePointer

    public init?(libPath: String) {
        guard let bridge = runtime_open(libPath) else {
            return nil
        }
        self.bridge = bridge
    }

    deinit {
        runtime_destroy(bridge)
    }

    func attach(_ layer: CAMetalLayer) {
        let size = layer.drawableSize
        var surface = RuntimeSurfaceDescriptor(
            metal_layer: Unmanaged.passUnretained(layer).toOpaque(),
            width: UInt32(size.width),
            height: UInt32(size.height)
        )
        runtime_attach_surface(bridge, &surface)
    }

    func resize(width: UInt32, height: UInt32) {
        runtime_resize(bridge, width, height)
    }

    func tick(_ dt: Float) {
        runtime_tick(bridge, dt)
    }

    func reload() {
        _ = runtime_reload(bridge)
    }
}
