@{
    Fixtures = @(
        @{ Source = 'gles.c'; Name = 'libguestgles.so'; Links = @(); AndroidLibraries = @('android', 'EGL', 'GLESv2') }
    )
}
