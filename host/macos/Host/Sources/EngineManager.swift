import QuartzCore

public final class EngineManager {
    private let engine: OpaquePointer

    public init?(libPath: String) {
        guard let engine = engine_open(libPath) else {
            return nil
        }
        self.engine = engine
    }

    deinit {
        engine_destroy(engine)
    }

    func attach(_ layer: CAMetalLayer) {
        let size = layer.bounds.size
        let scale = layer.contentsScale
        var surface = EngineSurfaceDescriptor(
            metal_layer: Unmanaged.passUnretained(layer).toOpaque(),
            width: UInt32(size.width * scale),
            height: UInt32(size.height * scale)
        )
        engine_attach_surface(engine, &surface)
    }

    func tick(_ dt: Float) {
        engine_tick(engine, dt)
    }
}
