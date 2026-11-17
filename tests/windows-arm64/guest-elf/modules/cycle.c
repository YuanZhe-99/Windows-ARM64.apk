// T07 cycle: linked against a seed whose SONAME is this image's own basename.
extern int dependency_value(void);

/// Keeps the self DT_NEEDED edge live so the graph loader must detect a cycle before execution.
int cycle_value(void) { return dependency_value(); }
