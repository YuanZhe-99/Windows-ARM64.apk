#pragma once

#include "surface.h"

namespace winapk::bridge {

/// Adds the restricted Android Vulkan instance/WSI provider to the guest namespace.
void AddGuestVulkanLibrary(std::vector<guest::BuiltinLibrary>& libraries);
/// Reports whether a live guest Vulkan surface owns the window's graphics endpoint.
bool GuestVulkanOwnsWindow();
/// Invalidates WSI queries on content replacement; closes all Vulkan state on window close.
void RetireGuestVulkan(HWND window, bool closing = false);

} // namespace winapk::bridge
