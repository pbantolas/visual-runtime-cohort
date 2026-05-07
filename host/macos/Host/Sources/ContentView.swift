import SwiftUI

public struct ContentView: View {
    let manager: EngineManager

    public init(manager: EngineManager) {
        self.manager = manager
    }

    public var body: some View {
        MetalView(manager: manager)
            .frame(minWidth: 800, minHeight: 600)
    }
}
