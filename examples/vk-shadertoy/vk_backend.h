/*
 * Copyright © 2024 Joel Winarske
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#pragma once

#define VULKAN_HPP_NO_EXCEPTIONS 1
#define VK_USE_PLATFORM_WAYLAND_KHR 1
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1

#include <vulkan/vulkan.hpp>

#include <wayland-client.h>

// Default
constexpr struct VkAllocationCallbacks *VKALLOC = nullptr;

class VulkanBackend {
public:
    VulkanBackend(std::string app_id, bool enable_validation_layers);

    ~VulkanBackend();

    void CreateSurface(struct wl_display *display, struct wl_surface *surface, int32_t width, int32_t height,
                       uint32_t command_buffer_count);

    void Resize(int32_t width, int32_t height);

private:
    static constexpr VkPresentModeKHR kPreferredPresentMode = VK_PRESENT_MODE_FIFO_KHR;

    const vk::DispatchLoaderDynamic &d_;

    std::string app_id_;
    std::vector<const char *> enabled_instance_extensions_{};
    std::vector<const char *> enabled_device_extensions_{};
    std::vector<const char *> enabled_layer_extensions_{};
    VkInstance instance_{};
    VkSurfaceKHR surface_{};

    VkPhysicalDevice physical_device_{};
    VkPhysicalDeviceFeatures physical_device_features_{};
    VkPhysicalDeviceProperties physical_device_properties_{};
    VkPhysicalDeviceMemoryProperties physical_device_memory_properties_{};

    VkDevice device_{};
    uint32_t queue_family_index_{};
    VkQueue queue_{};

    bool debugUtilsSupported_{};
    bool enable_validation_layers_;

    VkSurfaceFormatKHR surface_format_{};
    VkSwapchainKHR swapchain_{};
    VkCommandPool swapchain_command_pool_{};
    uint32_t swapchain_command_buffers_count_{};
    VkCommandBuffer swapchain_buffers_{};
    std::vector<VkImage> swapchain_images_;
    std::vector<VkCommandBuffer> present_transition_buffers_;

    VkSemaphore post_acquire_semaphore_{};
    VkSemaphore pre_submit_semaphore_{};
    VkFence exec_fence_{};

    bool resize_pending_;

    struct wl_display *wl_display_{};
    struct wl_surface *wl_surface_{};
    uint32_t width_;
    uint32_t height_;

    /**
     * @brief Create Vulkan instance
     * @return void
     * @relation
     * wayland
     */
    void createInstance();

    /**
     * @brief Setup Vulkan debug callback
     * @return void
     * @relation
     * wayland
     */
    void setupDebugMessenger();

    /**
     * @brief Find a compatible Vulkan physical device
     * @return void
     * @relation
     * wayland
     */
    void findPhysicalDevice();

    /**
     * @brief Create Vulkan logical device
     * @return void
     * @relation
     * wayland
     */
    void createLogicalDevice();

    /**
     * @brief Initialize Vulkan swapchain
     * @return bool
     * @retval true Normal end
     * @retval false Abnormal end
     * @relation
     * wayland
     */
    bool InitializeSwapChain();

    VkDebugReportCallbackEXT mDebugCallback = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT mDebugMessenger = VK_NULL_HANDLE;

    /**
     * @brief Callback to VK_EXT_debug_utils
     * @param[in] severity Bitmask of VkDebugUtilsMessageSeverityFlagBitsEXT
     * @param[in] types No use
     * @param[in] cbdata Structure specifying parameters returned to the callback
     * @param[in] pUserData No use
     * @return VkBool32
     * @retval VK_FALSE Abnormal end
     * @relation
     * wayland
     */
    static VKAPI_ATTR VkBool32

    VKAPI_CALL
    debugUtilsCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                       VkDebugUtilsMessageTypeFlagsEXT types,
                       const VkDebugUtilsMessengerCallbackDataEXT *cbdata,
                       void *pUserData);

    /**
     * @brief Callback to VK_EXT_debug_report
     * @param[in] flags Bitmask of VkDebugReportFlagBitsEXT
     * @param[in] objectType No use
     * @param[in] object No use
     * @param[in] location No use
     * @param[in] messageCode No use
     * @param[in] pLayerPrefix The name of the component
     * @param[in] pMessage Output message
     * @param[in] pUserData No use
     * @return VkBool32
     * @retval VK_FALSE Abnormal end
     * @relation
     * wayland
     */
    static VKAPI_ATTR VkBool32

    VKAPI_CALL
    debugReportCallback(VkDebugReportFlagsEXT flags,
                        VkDebugReportObjectTypeEXT objectType,
                        uint64_t object,
                        size_t location,
                        int32_t messageCode,
                        const char *pLayerPrefix,
                        const char *pMessage,
                        void *pUserData);
};