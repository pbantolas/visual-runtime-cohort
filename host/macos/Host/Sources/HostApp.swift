import SwiftUI

@main
struct HostApp: App {
    let session: VisualRuntimeSession = {
        let path = Bundle.main.infoDictionary?["VisualRuntimeLibPath"] as? String ?? ""
        guard let session = VisualRuntimeSession(libPath: path) else {
            fatalError("Failed to load visual runtime from: \(path)")
        }
        return session
    }()

    var body: some Scene {
        WindowGroup {
            ContentView(session: session)
        }
    }
}
