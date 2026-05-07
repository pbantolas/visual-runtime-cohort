import ProjectDescription

let project = Project(
    name: "Host",
    targets: [
        .target(
            name: "Host",
            destinations: .macOS,
            product: .app,
            bundleId: "dev.tuist.Host",
            // ENGINE_LIB_PATH is resolved at build time; the resulting absolute
            // path is embedded in the app bundle via Info.plist.
            infoPlist: .extendingDefault(with: [
                "EngineLibPath": .string("$(ENGINE_LIB_PATH)")
            ]),
            sources: [
                .glob("Host/Sources/**/*.swift"),
                .glob("Host/Sources/**/*.cpp"),
            ],
            resources: [.glob(pattern: "Host/Resources/**")],
            headers: .headers(project: ["Host/Sources/Engine.h"]),
            settings: .settings(base: [
                // Exposes the plain C host shim to Swift.
                "SWIFT_OBJC_BRIDGING_HEADER": "$(SRCROOT)/Host/Sources/Host-Bridging-Header.h",
                // Expose host/ and core/include/ to the C++ shim.
                "HEADER_SEARCH_PATHS": [
                    "$(SRCROOT)/../",
                    "$(SRCROOT)/../../core/include",
                ],
                "OTHER_CPLUSPLUSFLAGS": "-std=c++20",
                // Path to the CMake-built engine dylib during development.
                "ENGINE_LIB_PATH": "$(SRCROOT)/../../build/lib/libengine.dylib",
            ])
        ),
        .target(
            name: "HostTests",
            destinations: .macOS,
            product: .unitTests,
            bundleId: "dev.tuist.HostTests",
            infoPlist: .default,
            sources: [.glob("Host/Tests/**/*.swift")],
            dependencies: [.target(name: "Host")]
        ),
    ]
)
