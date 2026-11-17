#pragma once

namespace winapk::guest {

class Module;

/// Registers a protected image for fault attribution before any guest code runs.
/// Returns: false if the Windows handler cannot be installed; allocation may throw to the loader.
bool TrackModule(const Module* module);

/// Removes a module from fault attribution before its executable mappings are released.
void UntrackModule(const Module* module);

}  // namespace winapk::guest
