import QuartzCore

public final class VisualRuntimeSession {
    private var host: VisualRuntimeHost
    public var backendName: String {
        String(host.backendName())
    }

    public init?(libPath: String) {
        let host = VisualRuntimeHost(std.string(libPath))
        guard host.valid() else {
            return nil
        }
        self.host = host
    }

    func attach(_ layer: CAMetalLayer) {
        let size = layer.drawableSize
        host.attachSurface(
            Unmanaged.passUnretained(layer).toOpaque(),
            UInt32(size.width),
            UInt32(size.height)
        )
    }

    func resize(width: UInt32, height: UInt32) {
        host.resize(width, height)
    }

    func tick(_ dt: Float) {
        host.tick(dt)
    }

    func reload() {
        _ = host.reload()
    }
}
