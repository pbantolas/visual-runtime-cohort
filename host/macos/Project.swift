import ProjectDescription

let project = Project(
    name: "Host",
    targets: [
        .target(
            name: "Host",
            destinations: .macOS,
            product: .app,
            bundleId: "dev.tuist.Host",
            deploymentTargets: .macOS("15.0"),
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
            headers: .headers(project: ["Host/Sources/EngineHost.h"]),
            settings: .settings(base: [
                // Exposes the C++ host facade to Swift.
                "SWIFT_OBJC_BRIDGING_HEADER": "$(SRCROOT)/Host/Sources/Host-Bridging-Header.h",
                "SWIFT_OBJC_INTEROP_MODE": "objcxx",
                // Expose host/ and core/include/ to the C++ facade.
                "HEADER_SEARCH_PATHS": [
                    "$(SRCROOT)/../",
                    "$(SRCROOT)/../../core/include",
                ],
                "OTHER_CPLUSPLUSFLAGS": "-std=c++17",
                // Path to the CMake-built engine dylib during development.
                "ENGINE_LIB_PATH": "$(SRCROOT)/../../build/lib/libengine.dylib",
            ])
        ),
        .target(
            name: "HostTests",
            destinations: .macOS,
            product: .unitTests,
            bundleId: "dev.tuist.HostTests",
            deploymentTargets: .macOS("15.0"),
            infoPlist: .default,
            sources: [.glob("Host/Tests/**/*.swift")],
            dependencies: [.target(name: "Host")],
            settings: .settings(base: [
                "SWIFT_OBJC_INTEROP_MODE": "objcxx",
                "HEADER_SEARCH_PATHS": [
                    "$(SRCROOT)/../",
                    "$(SRCROOT)/../../core/include",
                ],
                "OTHER_CPLUSPLUSFLAGS": "-std=c++17",
            ])
        ),
    ]
)
