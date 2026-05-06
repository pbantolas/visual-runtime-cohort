import ProjectDescription

let project = Project(
    name: "Host",
    targets: [
        .target(
            name: "Host",
            destinations: .macOS,
            product: .app,
            bundleId: "dev.tuist.Host",
            infoPlist: .default,
            buildableFolders: [
                "Host/Sources",
                "Host/Resources",
            ],
            dependencies: []
        ),
        .target(
            name: "HostTests",
            destinations: .macOS,
            product: .unitTests,
            bundleId: "dev.tuist.HostTests",
            infoPlist: .default,
            buildableFolders: [
                "Host/Tests"
            ],
            dependencies: [.target(name: "Host")]
        ),
    ]
)
