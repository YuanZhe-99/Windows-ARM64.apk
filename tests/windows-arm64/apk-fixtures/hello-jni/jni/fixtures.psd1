# T09/G3: ordinary NDK links at the manifest API, dependency order and source/links recorded in hello-jni.json.
@{
    Fixtures = @(
        @{ Source = 'dependency.c'; Name = 'libdependency.so'; Links = @() }
        @{ Source = 'version.c'; Name = 'libversion.so'; Links = @() }
        @{ Source = 'hello.c'; Name = 'libhello.so'; Links = @('libdependency.so', 'libversion.so') }
    )
}
