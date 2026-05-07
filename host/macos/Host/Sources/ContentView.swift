import AppKit
import SwiftUI

public struct ContentView: View {
    let manager: RuntimeManager

    public init(manager: RuntimeManager) {
        self.manager = manager
    }

    public var body: some View {
        ZStack(alignment: .topLeading) {
            MetalView(manager: manager)

            WindowDragView(manager: manager)

            Text("q to close, r to reload")
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
    let manager: RuntimeManager

    func makeNSView(context: Context) -> NSView {
        WindowDraggingNSView(manager: manager)
    }

    func updateNSView(_ nsView: NSView, context: Context) {}
}

private final class WindowDraggingNSView: NSView {
    private let manager: RuntimeManager

    init(manager: RuntimeManager) {
        self.manager = manager
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
            manager.reload()
        default:
            super.keyDown(with: event)
        }
    }
}
