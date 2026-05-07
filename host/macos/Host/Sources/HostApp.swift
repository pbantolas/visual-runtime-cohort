import SwiftUI

@main
struct HostApp: App {
    let manager: RuntimeManager = {
        let path = Bundle.main.infoDictionary?["EngineLibPath"] as? String ?? ""
        guard let m = RuntimeManager(libPath: path) else {
            fatalError("Failed to load engine from: \(path)")
        }
        return m
    }()

    var body: some Scene {
        WindowGroup {
            ContentView(manager: manager)
        }
    }
}
