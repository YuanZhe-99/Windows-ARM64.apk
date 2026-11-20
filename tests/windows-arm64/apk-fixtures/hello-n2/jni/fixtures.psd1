@{
    Fixtures = @(
        @{ Source = 'n2check.c'; Name = 'libn2check.so'; Links = @(); AndroidLibraries = @('m', 'android', 'jnigraphics', 'OpenSLES', 'aaudio', 'log', 'dl') }
    )
}
