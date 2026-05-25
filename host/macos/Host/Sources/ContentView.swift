import AppKit
import SwiftUI

public struct ContentView: View {
    let engineSession: EngineSession

    public init(engineSession: EngineSession) {
        self.engineSession = engineSession
    }

    public var body: some View {
        ZStack(alignment: .topLeading) {
            MetalView(engineSession: engineSession)

            WindowDragView(engineSession: engineSession)

            VStack(alignment: .leading, spacing: 4) {
                Text("q to close, r to reload")
                Text("backend: \(engineSession.backendName)")
            }
            .font(.body.monospaced().weight(.medium))
            .foregroundStyle(.white)
            .shadow(color: .black.opacity(0.8), radius: 2, x: 0, y: 1)
            .padding(12)
            .allowsHitTesting(false)
        }
        .frame(minWidth: 800, minHeight: 600)
    }
}

private struct WindowDragView: NSViewRepresentable {
    let engineSession: EngineSession

    func makeNSView(context: Context) -> NSView {
        WindowDraggingNSView(engineSession: engineSession)
    }

    func updateNSView(_ nsView: NSView, context: Context) {}
}

private final class WindowDraggingNSView: NSView {
    private let engineSession: EngineSession

    init(engineSession: EngineSession) {
        self.engineSession = engineSession
        super.init(frame: .zero)
    }

    required init?(coder: NSCoder) { fatalError() }

    override var mouseDownCanMoveWindow: Bool { true }
    override var acceptsFirstResponder: Bool { true }

    override func viewDidMoveToWindow() {
        super.viewDidMoveToWindow()
        window?.makeFirstResponder(self)
    }

    override func acceptsFirstMouse(for event: NSEvent?) -> Bool { true }

    override func mouseDown(with event: NSEvent) {
        window?.performDrag(with: event)
    }

    override func keyDown(with event: NSEvent) {
        switch event.charactersIgnoringModifiers {
        case "q":
            NSApp.terminate(nil)
        case "r":
            engineSession.reload()
        default:
            super.keyDown(with: event)
        }
    }
}
