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
        let size = layer.bounds.size
        let scale = layer.contentsScale
        var surface = RuntimeSurfaceDescriptor(
            metal_layer: Unmanaged.passUnretained(layer).toOpaque(),
            width: UInt32(size.width * scale),
            height: UInt32(size.height * scale)
        )
        runtime_attach_surface(bridge, &surface)
    }

    func tick(_ dt: Float) {
        runtime_tick(bridge, dt)
    }

    func reload() {
        _ = runtime_reload(bridge)
    }
}
