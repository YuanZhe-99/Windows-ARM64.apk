@{
    Fixtures = @(
        @{ Source = 'wsi.c'; Name = 'libguestwsi.so'; Links = @(); AndroidLibraries = @('android', 'vulkan') }
    )
}
