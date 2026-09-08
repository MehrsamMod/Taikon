#pragma once

#include <vulkan/vulkan.h>
#include <cstdint>
#include <vector>

namespace Vulkan {

struct SwapchainImage {
    VkImage image = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
};

class Swapchain {
public:
    Swapchain() = default;
    ~Swapchain();

    Swapchain(const Swapchain&) = delete;
    Swapchain& operator=(const Swapchain&) = delete;

    bool Create(
        VkPhysicalDevice* physical_device,
        VkDevice* device,
        VkSurfaceKHR* surface,
        uint32_t* width,
        uint32_t* height);

    void Destroy();

    bool Recreate(
        uint32_t* width,
        uint32_t* height);

    bool AcquireNextImage(
        VkSemaphore* image_available,
        VkFence* fence = nullptr);

    bool Present(
        VkQueue* present_queue,
        VkSemaphore* render_finished);

    VkSwapchainKHR Handle() const {
        return swapchain_;
    }

    VkFormat Format() const {
        return format_;
    }

    VkExtent2D Extent() const {
        return extent_;
    }

    uint32_t ImageCount() const {
        return static_cast<uint32_t>(images_.size());
    }

    uint32_t CurrentImageIndex() const {
        return current_image_;
    }

    const SwapchainImage& Image(uint32_t index) const {
        return images_[index];
    }

    VkImage CurrentImage() const {
        if (current_image_ >= images_.size())
            return VK_NULL_HANDLE;

        return images_[current_image_].image;
    }

    VkImageView CurrentImageView() const {
        if (current_image_ >= images_.size())
            return VK_NULL_HANDLE;

        return images_[current_image_].view;
    }

private:
    bool FindQueueFamilies();

    bool CreateSwapchain(
        uint32_t* width,
        uint32_t* height,
        VkSwapchainKHR* old_swapchain);

    bool CreateImageViews();

    void DestroyImageViews();

private:
    VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;

    VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;

    VkFormat format_ = VK_FORMAT_UNDEFINED;

    VkColorSpaceKHR color_space_ =
        VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;

    VkExtent2D extent_{};

    VkPresentModeKHR present_mode_ =
        VK_PRESENT_MODE_FIFO_KHR;

    VkSurfaceTransformFlagBitsKHR pre_transform_ =
        VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;

    VkCompositeAlphaFlagBitsKHR composite_alpha_ =
        VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

    uint32_t graphics_queue_family_ = UINT32_MAX;
    uint32_t present_queue_family_ = UINT32_MAX;

    std::vector<SwapchainImage> images_;

    uint32_t current_image_ = 0;
};

} // namespace Vulkan
