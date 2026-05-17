import SwiftUI

@main
struct HostApp: App {
    let engineSession: EngineSession = {
        let path = Bundle.main.infoDictionary?["EngineLibPath"] as? String ?? ""
        guard let engineSession = EngineSession(libPath: path) else {
            fatalError("Failed to load engine from: \(path)")
        }
        return engineSession
    }()

    var body: some Scene {
        WindowGroup {
            ContentView(engineSession: engineSession)
        }
    }
}
