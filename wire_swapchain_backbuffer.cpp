// wire_swapchain_backbuffer.cpp
#include "wire_swapchain_backbuffer.h"

WireSwapchainBackbuffer::~WireSwapchainBackbuffer()
{
    Destroy();
}

bool WireSwapchainBackbuffer::Initialize(
    VkDevice device,
    VkQueue graphicsQueue,
    VkQueue presentQueue,
    Vulkan::Swapchain* swapchain,
    PM4VulkanBackbuffer* backbuffer)
{
    if (device == VK_NULL_HANDLE ||
        graphicsQueue == VK_NULL_HANDLE ||
        presentQueue == VK_NULL_HANDLE ||
        swapchain == nullptr ||
        backbuffer == nullptr)
    {
        return false;
    }

    device_ = device;
    graphicsQueue_ = graphicsQueue;
    presentQueue_ = presentQueue;
    swapchain_ = swapchain;
    backbuffer_ = backbuffer;

    return CreateSyncObjects();
}

bool WireSwapchainBackbuffer::CreateSyncObjects()
{
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType =
        VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    if (vkCreateSemaphore(
            device_,
            &semaphoreInfo,
            nullptr,
            &imageAvailable_) != VK_SUCCESS)
    {
        return false;
    }

    if (vkCreateSemaphore(
            device_,
            &semaphoreInfo,
            nullptr,
            &renderFinished_) != VK_SUCCESS)
    {
        DestroySyncObjects();
        return false;
    }

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType =
        VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags =
        VK_FENCE_CREATE_SIGNALED_BIT;

    if (vkCreateFence(
            device_,
            &fenceInfo,
            nullptr,
            &inFlight_) != VK_SUCCESS)
    {
        DestroySyncObjects();
        return false;
    }

    return true;
}

void WireSwapchainBackbuffer::DestroySyncObjects()
{
    if (device_ == VK_NULL_HANDLE)
        return;

    if (inFlight_ != VK_NULL_HANDLE)
    {
        vkDestroyFence(
            device_,
            inFlight_,
            nullptr);

        inFlight_ = VK_NULL_HANDLE;
    }

    if (renderFinished_ != VK_NULL_HANDLE)
    {
        vkDestroySemaphore(
            device_,
            renderFinished_,
            nullptr);

        renderFinished_ = VK_NULL_HANDLE;
    }

    if (imageAvailable_ != VK_NULL_HANDLE)
    {
        vkDestroySemaphore(
            device_,
            imageAvailable_,
            nullptr);

        imageAvailable_ = VK_NULL_HANDLE;
    }
}

bool WireSwapchainBackbuffer::BeginFrame()
{
    if (frameActive_ ||
        swapchain_ == nullptr ||
        backbuffer_ == nullptr)
    {
        return false;
    }

    if (vkWaitForFences(
            device_,
            1,
            &inFlight_,
            VK_TRUE,
            UINT64_MAX) != VK_SUCCESS)
    {
        return false;
    }

    if (vkResetFences(
            device_,
            1,
            &inFlight_) != VK_SUCCESS)
    {
        return false;
    }

    if (!swapchain_->AcquireNextImage(
            &imageAvailable_,
            nullptr))
    {
        return false;
    }

    if (!backbuffer_->BeginFrame())
        return false;

    if (!backbuffer_->BeginRendering(
            true,
            true))
    {
        backbuffer_->EndFrame();
        return false;
    }

    frameActive_ = true;

    return true;
}

VkCommandBuffer WireSwapchainBackbuffer::CommandBuffer() const
{
    if (backbuffer_ == nullptr)
        return VK_NULL_HANDLE;

    return backbuffer_->GetCommandBuffer();
}

bool WireSwapchainBackbuffer::EndFrame()
{
    if (!frameActive_ ||
        swapchain_ == nullptr ||
        backbuffer_ == nullptr)
    {
        return false;
    }

    if (!backbuffer_->EndRendering())
    {
        frameActive_ = false;
        return false;
    }

    if (!backbuffer_->CopyToSwapchain(
            swapchain_->CurrentImage(),
            swapchain_->Extent()))
    {
        backbuffer_->EndFrame();
        frameActive_ = false;
        return false;
    }

    if (!backbuffer_->EndFrame())
    {
        frameActive_ = false;
        return false;
    }

    VkPipelineStageFlags waitStage =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkCommandBuffer commandBuffer =
        backbuffer_->GetCommandBuffer();

    VkSubmitInfo submitInfo{};
    submitInfo.sType =
        VK_STRUCTURE_TYPE_SUBMIT_INFO;

    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores =
        &imageAvailable_;

    submitInfo.pWaitDstStageMask =
        &waitStage;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers =
        &commandBuffer;

    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores =
        &renderFinished_;

    if (vkQueueSubmit(
            graphicsQueue_,
            1,
            &submitInfo,
            inFlight_) != VK_SUCCESS)
    {
        frameActive_ = false;
        return false;
    }

    if (!swapchain_->Present(
            &presentQueue_,
            &renderFinished_))
    {
        frameActive_ = false;
        return false;
    }

    frameActive_ = false;

    return true;
}

void WireSwapchainBackbuffer::Destroy()
{
    if (device_ != VK_NULL_HANDLE)
    {
        vkDeviceWaitIdle(device_);
        DestroySyncObjects();
    }

    swapchain_ = nullptr;
    backbuffer_ = nullptr;

    graphicsQueue_ = VK_NULL_HANDLE;
    presentQueue_ = VK_NULL_HANDLE;
    device_ = VK_NULL_HANDLE;

    frameActive_ = false;
}