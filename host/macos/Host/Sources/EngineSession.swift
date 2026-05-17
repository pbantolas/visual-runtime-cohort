import QuartzCore

public final class EngineSession {
    private var engineHost: EngineHost

    public init?(libPath: String) {
        let engineHost = EngineHost(std.string(libPath))
        guard engineHost.valid() else {
            return nil
        }
        self.engineHost = engineHost
    }

    func attach(_ layer: CAMetalLayer) {
        let size = layer.drawableSize
        engineHost.attachSurface(
            Unmanaged.passUnretained(layer).toOpaque(),
            UInt32(size.width),
            UInt32(size.height)
        )
    }

    func resize(width: UInt32, height: UInt32) {
        engineHost.resize(width, height)
    }

    func tick(_ dt: Float) {
        engineHost.tick(dt)
    }

    func reload() {
        _ = engineHost.reload()
    }
}
