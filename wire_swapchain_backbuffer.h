// wire_swapchain_backbuffer.h
#pragma once

#include <vulkan/vulkan.h>

#include "pm4_vulkan_backbuffer.h"
#include "vulkan_swapchain.h"

class WireSwapchainBackbuffer
{
public:
    WireSwapchainBackbuffer() = default;
    ~WireSwapchainBackbuffer();

    WireSwapchainBackbuffer(const WireSwapchainBackbuffer&) = delete;
    WireSwapchainBackbuffer& operator=(
        const WireSwapchainBackbuffer&) = delete;

    bool Initialize(
        VkDevice device,
        VkQueue graphicsQueue,
        VkQueue presentQueue,
        Vulkan::Swapchain* swapchain,
        PM4VulkanBackbuffer* backbuffer);

    void Destroy();

    bool BeginFrame();
    bool EndFrame();

    VkCommandBuffer CommandBuffer() const;

private:
    bool CreateSyncObjects();
    void DestroySyncObjects();

private:
    VkDevice device_ = VK_NULL_HANDLE;

    VkQueue graphicsQueue_ = VK_NULL_HANDLE;
    VkQueue presentQueue_ = VK_NULL_HANDLE;

    Vulkan::Swapchain* swapchain_ = nullptr;
    PM4VulkanBackbuffer* backbuffer_ = nullptr;

    VkSemaphore imageAvailable_ = VK_NULL_HANDLE;
    VkSemaphore renderFinished_ = VK_NULL_HANDLE;
    VkFence inFlight_ = VK_NULL_HANDLE;

    bool frameActive_ = false;
};