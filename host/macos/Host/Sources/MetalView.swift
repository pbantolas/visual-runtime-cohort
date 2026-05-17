import AppKit
import QuartzCore
import SwiftUI

// MARK: - Backing NSView

final class MetalNSView: NSView {

    override init(frame: NSRect) {
        super.init(frame: frame)
        wantsLayer = true
    }

    required init?(coder: NSCoder) { fatalError() }

    // AppKit calls this once when wantsLayer = true to create the backing layer.
    override func makeBackingLayer() -> CALayer {
        CAMetalLayer()
    }

    override var wantsUpdateLayer: Bool { true }

    var drawableSizeDidChange: ((CGSize) -> Void)?

    var metalLayer: CAMetalLayer { layer as! CAMetalLayer }

    override func setFrameSize(_ newSize: NSSize) {
        super.setFrameSize(newSize)
        updateDrawableSize(newSize)
    }

    override func viewDidChangeBackingProperties() {
        super.viewDidChangeBackingProperties()
        updateDrawableSize(frame.size)
    }

    func updateDrawableSize(_ size: NSSize) {
        let scale = window?.backingScaleFactor ?? 1.0
        metalLayer.contentsScale = scale
        let drawableSize = CGSize(width: size.width * scale, height: size.height * scale)
        metalLayer.drawableSize = drawableSize
        drawableSizeDidChange?(drawableSize)
    }
}

// MARK: - SwiftUI wrapper

struct MetalView: NSViewRepresentable {
    let engineSession: EngineSession

    func makeCoordinator() -> Coordinator { Coordinator(engineSession: engineSession) }

    func makeNSView(context: Context) -> MetalNSView {
        let view = MetalNSView()
        context.coordinator.start(view: view)
        return view
    }

    func updateNSView(_ view: MetalNSView, context: Context) {}

    // MARK: Coordinator - owns the display link and drives engine ticks

    final class Coordinator: NSObject {
        let engineSession: EngineSession
        private var displayLink: CADisplayLink?
        private var lastTime: Double = 0

        init(engineSession: EngineSession) { self.engineSession = engineSession }

        func start(view: MetalNSView) {
            view.updateDrawableSize(view.frame.size)
            view.drawableSizeDidChange = { [weak engineSession] size in
                engineSession?.resize(width: UInt32(size.width), height: UInt32(size.height))
            }
            engineSession.attach(view.metalLayer)

            let displayLink = view.displayLink(target: self, selector: #selector(displayLinkFired(_:)))
            displayLink.add(to: .main, forMode: .common)
            self.displayLink = displayLink
        }

        @objc private func displayLinkFired(_ displayLink: CADisplayLink) {
            let now = displayLink.timestamp
            let dt  = lastTime == 0 ? 0.0 : Float(now - lastTime)
            lastTime = now
            engineSession.tick(dt)
        }

        deinit { displayLink?.invalidate() }
    }
}
