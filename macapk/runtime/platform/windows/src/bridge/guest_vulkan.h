#pragma once

#include "surface.h"

namespace winapk::bridge {

/// Adds the restricted Android Vulkan instance/WSI provider to the guest namespace.
void AddGuestVulkanLibrary(std::vector<guest::BuiltinLibrary>& libraries);
/// Records the active guest namespace so host->guest Vulkan callbacks verify executable ranges.
void SetGuestVulkanModules(guest::ModuleSet* modules);
/// Reports whether a live guest Vulkan surface owns the window's graphics endpoint.
bool GuestVulkanOwnsWindow();
/// Reports that the active guest Vulkan swapchain has presented to this HWND.
bool GuestVulkanPresents(HWND window);
/// Invalidates WSI queries on content replacement; closes all Vulkan state on window close.
void RetireGuestVulkan(HWND window, bool closing = false);

} // namespace winapk::bridge
