#include "vulkan_swapchain.h"

#include <algorithm>
#include <limits>

namespace Vulkan {

namespace {

bool FindSurfaceFormat(
    VkPhysicalDevice physical_device,
    VkSurfaceKHR surface,
    VkFormat& format,
    VkColorSpaceKHR& color_space)
{
    uint32_t count = 0;

    if (vkGetPhysicalDeviceSurfaceFormatsKHR(
            physical_device,
            surface,
            &count,
            nullptr) != VK_SUCCESS ||
        count == 0) {
        return false;
    }

    std::vector<VkSurfaceFormatKHR> formats(count);

    if (vkGetPhysicalDeviceSurfaceFormatsKHR(
            physical_device,
            surface,
            &count,
            formats.data()) != VK_SUCCESS) {
        return false;
    }

    if (count == 1 &&
        formats[0].format == VK_FORMAT_UNDEFINED) {
        format = VK_FORMAT_B8G8R8A8_UNORM;
        color_space = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        return true;
    }

    format = formats[0].format;
    color_space = formats[0].colorSpace;

    return true;
}

VkPresentModeKHR FindPresentMode(
    VkPhysicalDevice physical_device,
    VkSurfaceKHR surface)
{
    uint32_t count = 0;

    if (vkGetPhysicalDeviceSurfacePresentModesKHR(
            physical_device,
            surface,
            &count,
            nullptr) != VK_SUCCESS ||
        count == 0) {
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    std::vector<VkPresentModeKHR> modes(count);

    if (vkGetPhysicalDeviceSurfacePresentModesKHR(
            physical_device,
            surface,
            &count,
            modes.data()) != VK_SUCCESS) {
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    // FIFO is guaranteed by Vulkan.
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkCompositeAlphaFlagBitsKHR FindCompositeAlpha(
    const VkSurfaceCapabilitiesKHR& capabilities)
{
    constexpr VkCompositeAlphaFlagBitsKHR modes[] = {
        VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR
    };

    for (VkCompositeAlphaFlagBitsKHR mode : modes) {
        if (capabilities.supportedCompositeAlpha & mode)
            return mode;
    }

    return VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
}

VkExtent2D ChooseExtent(
    const VkSurfaceCapabilitiesKHR& capabilities,
    uint32_t width,
    uint32_t height)
{
    if (capabilities.currentExtent.width !=
        std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
    }

    VkExtent2D result{width, height};

    result.width = std::max(
        capabilities.minImageExtent.width,
        std::min(
            capabilities.maxImageExtent.width,
            result.width));

    result.height = std::max(
        capabilities.minImageExtent.height,
        std::min(
            capabilities.maxImageExtent.height,
            result.height));

    return result;
}

} // namespace

Swapchain::~Swapchain()
{
    Destroy();
}

bool Swapchain::Create(
    VkPhysicalDevice physical_device,
    VkDevice device,
    VkSurfaceKHR surface,
    uint32_t width,
    uint32_t height)
{
    if (physical_device == VK_NULL_HANDLE ||
        device == VK_NULL_HANDLE ||
        surface == VK_NULL_HANDLE) {
        return false;
    }

    physical_device_ = physical_device;
    device_ = device;
    surface_ = surface;

    if (!FindQueueFamilies())
        return false;

    if (!CreateSwapchain(width, height, VK_NULL_HANDLE))
        return false;

    if (!CreateImageViews()) {
        Destroy();
        return false;
    }

    return true;
}

bool Swapchain::FindQueueFamilies()
{
    uint32_t count = 0;

    vkGetPhysicalDeviceQueueFamilyProperties(
        physical_device_,
        &count,
        nullptr);

    if (count == 0)
        return false;

    std::vector<VkQueueFamilyProperties> properties(count);

    vkGetPhysicalDeviceQueueFamilyProperties(
        physical_device_,
        &count,
        properties.data());

    graphics_queue_family_ = UINT32_MAX;
    present_queue_family_ = UINT32_MAX;

    std::vector<VkBool32> supports_present(count, VK_FALSE);

    for (uint32_t i = 0; i < count; ++i) {
        if (vkGetPhysicalDeviceSurfaceSupportKHR(
                physical_device_,
                i,
                surface_,
                &supports_present[i]) != VK_SUCCESS) {
            return false;
        }

        if ((properties[i].queueFlags &
             VK_QUEUE_GRAPHICS_BIT) != 0) {

            if (graphics_queue_family_ == UINT32_MAX)
                graphics_queue_family_ = i;

            if (supports_present[i] == VK_TRUE) {
                graphics_queue_family_ = i;
                present_queue_family_ = i;
                break;
            }
        }
    }

    if (present_queue_family_ == UINT32_MAX) {
        for (uint32_t i = 0; i < count; ++i) {
            if (supports_present[i] == VK_TRUE) {
                present_queue_family_ = i;
                break;
            }
        }
    }

    return graphics_queue_family_ != UINT32_MAX &&
           present_queue_family_ != UINT32_MAX;
}

bool Swapchain::CreateSwapchain(
    uint32_t width,
    uint32_t height,
    VkSwapchainKHR old_swapchain)
{
    VkSurfaceCapabilitiesKHR capabilities{};

    if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
            physical_device_,
            surface_,
            &capabilities) != VK_SUCCESS) {
        return false;
    }

    if (!FindSurfaceFormat(
            physical_device_,
            surface_,
            format_,
            color_space_)) {
        return false;
    }

    present_mode_ =
        FindPresentMode(
            physical_device_,
            surface_);

    extent_ =
        ChooseExtent(
            capabilities,
            width,
            height);

    uint32_t image_count =
        capabilities.minImageCount;

    if (capabilities.maxImageCount != 0 &&
        image_count > capabilities.maxImageCount) {
        image_count = capabilities.maxImageCount;
    }

    if (capabilities.supportedTransforms &
        VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) {
        pre_transform_ =
            VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    } else {
        pre_transform_ =
            capabilities.currentTransform;
    }

    composite_alpha_ =
        FindCompositeAlpha(capabilities);

    VkSwapchainCreateInfoKHR create_info{};

    create_info.sType =
        VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;

    create_info.surface = surface_;
    create_info.minImageCount = image_count;
    create_info.imageFormat = format_;
    create_info.imageColorSpace = color_space_;
    create_info.imageExtent = extent_;
    create_info.imageArrayLayers = 1;

    create_info.imageUsage =
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
        VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    uint32_t queue_indices[2] = {
        graphics_queue_family_,
        present_queue_family_
    };

    if (graphics_queue_family_ !=
        present_queue_family_) {

        create_info.imageSharingMode =
            VK_SHARING_MODE_CONCURRENT;

        create_info.queueFamilyIndexCount = 2;
        create_info.pQueueFamilyIndices =
            queue_indices;

    } else {
        create_info.imageSharingMode =
            VK_SHARING_MODE_EXCLUSIVE;
    }

    create_info.preTransform =
        pre_transform_;

    create_info.compositeAlpha =
        composite_alpha_;

    create_info.presentMode =
        present_mode_;

    create_info.clipped = VK_TRUE;

    create_info.oldSwapchain =
        old_swapchain;

    VkResult result =
        vkCreateSwapchainKHR(
            device_,
            &create_info,
            nullptr,
            &swapchain_);

    if (result != VK_SUCCESS)
        return false;

    uint32_t count = 0;

    result = vkGetSwapchainImagesKHR(
        device_,
        swapchain_,
        &count,
        nullptr);

    if (result != VK_SUCCESS || count == 0)
        return false;

    std::vector<VkImage> vk_images(count);

    result = vkGetSwapchainImagesKHR(
        device_,
        swapchain_,
        &count,
        vk_images.data());

    if (result != VK_SUCCESS)
        return false;

    images_.resize(count);

    for (uint32_t i = 0; i < count; ++i)
        images_[i].image = vk_images[i];

    current_image_ = 0;

    return true;
}

bool Swapchain::CreateImageViews()
{
    for (auto& image : images_) {
        VkImageViewCreateInfo view_info{};

        view_info.sType =
            VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;

        view_info.image = image.image;
        view_info.viewType =
            VK_IMAGE_VIEW_TYPE_2D;

        view_info.format = format_;

        view_info.components.r =
            VK_COMPONENT_SWIZZLE_IDENTITY;
        view_info.components.g =
            VK_COMPONENT_SWIZZLE_IDENTITY;
        view_info.components.b =
            VK_COMPONENT_SWIZZLE_IDENTITY;
        view_info.components.a =
            VK_COMPONENT_SWIZZLE_IDENTITY;

        view_info.subresourceRange.aspectMask =
            VK_IMAGE_ASPECT_COLOR_BIT;

        view_info.subresourceRange.baseMipLevel = 0;
        view_info.subresourceRange.levelCount = 1;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount = 1;

        if (vkCreateImageView(
                device_,
                &view_info,
                nullptr,
                &image.view) != VK_SUCCESS) {
            return false;
        }
    }

    return true;
}

void Swapchain::DestroyImageViews()
{
    for (auto& image : images_) {
        if (image.view != VK_NULL_HANDLE) {
            vkDestroyImageView(
                device_,
                image.view,
                nullptr);

            image.view = VK_NULL_HANDLE;
        }
    }
}

void Swapchain::Destroy()
{
    if (device_ == VK_NULL_HANDLE)
        return;

    DestroyImageViews();

    if (swapchain_ != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(
            device_,
            swapchain_,
            nullptr);

        swapchain_ = VK_NULL_HANDLE;
    }

    images_.clear();

    graphics_queue_family_ = UINT32_MAX;
    present_queue_family_ = UINT32_MAX;
    current_image_ = 0;
}

bool Swapchain::Recreate(
    uint32_t width,
    uint32_t height)
{
    if (device_ == VK_NULL_HANDLE)
        return false;

    vkDeviceWaitIdle(device_);

    VkSwapchainKHR old_swapchain =
        swapchain_;

    DestroyImageViews();

    images_.clear();

    swapchain_ = VK_NULL_HANDLE;

    if (!CreateSwapchain(
            width,
            height,
            old_swapchain)) {

        if (old_swapchain != VK_NULL_HANDLE)
            vkDestroySwapchainKHR(
                device_,
                old_swapchain,
                nullptr);

        return false;
    }

    if (old_swapchain != VK_NULL_HANDLE)
        vkDestroySwapchainKHR(
            device_,
            old_swapchain,
            nullptr);

    if (!CreateImageViews()) {
        Destroy();
        return false;
    }

    return true;
}

bool Swapchain::AcquireNextImage(
    VkSemaphore image_available,
    VkFence fence)
{
    if (swapchain_ == VK_NULL_HANDLE)
        return false;

    VkResult result =
        vkAcquireNextImageKHR(
            device_,
            swapchain_,
            UINT64_MAX,
            image_available,
            fence,
            &current_image_);

    return result == VK_SUCCESS ||
           result == VK_SUBOPTIMAL_KHR;
}

bool Swapchain::Present(
    VkQueue present_queue,
    VkSemaphore render_finished)
{
    if (swapchain_ == VK_NULL_HANDLE ||
        present_queue == VK_NULL_HANDLE) {
        return false;
    }

    VkPresentInfoKHR present_info{};

    present_info.sType =
        VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores =
        &render_finished;

    present_info.swapchainCount = 1;
    present_info.pSwapchains =
        &swapchain_;

    present_info.pImageIndices =
        &current_image_;

    VkResult result =
        vkQueuePresentKHR(
            present_queue,
            &present_info);

    return result == VK_SUCCESS ||
           result == VK_SUBOPTIMAL_KHR;
}

} // namespace Vulkan