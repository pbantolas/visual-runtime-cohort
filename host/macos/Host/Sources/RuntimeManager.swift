import QuartzCore

public final class RuntimeManager {
    private var runtime: HostRuntime

    public init?(libPath: String) {
        let runtime = HostRuntime(std.string(libPath))
        guard runtime.valid() else {
            return nil
        }
        self.runtime = runtime
    }

    func attach(_ layer: CAMetalLayer) {
        let size = layer.drawableSize
        runtime.attachSurface(
            Unmanaged.passUnretained(layer).toOpaque(),
            UInt32(size.width),
            UInt32(size.height)
        )
    }

    func resize(width: UInt32, height: UInt32) {
        runtime.resize(width, height)
    }

    func tick(_ dt: Float) {
        runtime.tick(dt)
    }

    func reload() {
        _ = runtime.reload()
    }
}
