import AppKit
import SwiftUI

public struct ContentView: View {
    let session: VisualRuntimeSession

    public init(session: VisualRuntimeSession) {
        self.session = session
    }

    public var body: some View {
        ZStack(alignment: .topLeading) {
            MetalView(session: session)

            WindowDragView(session: session)

            VStack(alignment: .leading, spacing: 4) {
                Text("q to close, r to reload")
                Text("backend: \(session.backendName)")
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
    let session: VisualRuntimeSession

    func makeNSView(context: Context) -> NSView {
        WindowDraggingNSView(session: session)
    }

    func updateNSView(_ nsView: NSView, context: Context) {}
}

private final class WindowDraggingNSView: NSView {
    private let session: VisualRuntimeSession

    init(session: VisualRuntimeSession) {
        self.session = session
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
            session.reload()
        default:
            super.keyDown(with: event)
        }
    }
}
