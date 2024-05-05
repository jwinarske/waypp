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

#include "vk_backend.h"

#include <utility>

#include "logging.h"

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

const auto &d = vk::defaultDispatchLoaderDynamic;

#define S1(x) #x
#define S2(x) S1(x)
#define LOCATION __FILE__ " : " S2(__LINE__)

#define CHECK_VK_RESULT(x)                                 \
  do {                                                     \
    vk::resultCheck(static_cast<vk::Result>(x), LOCATION); \
  } while (0)

VulkanBackend::VulkanBackend(std::string app_id,
                             bool enable_validation_layers) : d_(d), app_id_(std::move(app_id)),
                                                              width_(0),
                                                              height_(0),
                                                              enable_validation_layers_(enable_validation_layers),
                                                              resize_pending_(false) {
    VULKAN_HPP_DEFAULT_DISPATCHER.init();
    createInstance();
    setupDebugMessenger();
}

VulkanBackend::~VulkanBackend() {
    if (device_ != VK_NULL_HANDLE) {
        if (swapchain_command_pool_) {
            d_.vkDestroyCommandPool(device_, swapchain_command_pool_, nullptr);
        }
        if (post_acquire_semaphore_) {
            d_.vkDestroySemaphore(device_, post_acquire_semaphore_, nullptr);
        }
        if (pre_submit_semaphore_) {
            d_.vkDestroySemaphore(device_, pre_submit_semaphore_, nullptr);
        }
        if (exec_fence_) {
            d_.vkDestroyFence(device_, exec_fence_, nullptr);
        }
        d_.vkDestroyDevice(device_, nullptr);
    }
    if (surface_ != VK_NULL_HANDLE) {
        d_.vkDestroySurfaceKHR(instance_, surface_, nullptr);
    }
    if (enable_validation_layers_) {
        if (mDebugCallback) {
            d_.vkDestroyDebugReportCallbackEXT(instance_, mDebugCallback, VKALLOC);
        }
        if (mDebugMessenger) {
            d_.vkDestroyDebugUtilsMessengerEXT(instance_, mDebugMessenger, VKALLOC);
        }
    }
    if (instance_ != nullptr) {
        d_.vkDestroyInstance(instance_, nullptr);
    }
    if (!enabled_instance_extensions_.empty()) {
        for (auto it: enabled_instance_extensions_) {
            free((void *) it);
        }
    }
    if (!enabled_layer_extensions_.empty()) {
        for (auto it: enabled_layer_extensions_) {
            free((void *) it);
        }
    }
}

void VulkanBackend::createInstance() {
    auto instance_extensions = vk::enumerateInstanceExtensionProperties();
    spdlog::debug("Vulkan Instance Extensions:");
    bool supports_minimum_required[2]{};
    for (const auto &l: instance_extensions.value) {
        spdlog::debug("\t{}: version: {}", l.extensionName, l.specVersion);
        if (enable_validation_layers_) {
            if (strcmp(l.extensionName, VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME) == 0) {
                enabled_instance_extensions_.push_back(strdup(l.extensionName));
            }
            if (strcmp(l.extensionName, VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0) {
                debugUtilsSupported_ = true;
                enabled_instance_extensions_.push_back(strdup(l.extensionName));
            }
            if (strcmp(l.extensionName, VK_EXT_DEBUG_REPORT_EXTENSION_NAME) == 0) {
                enabled_instance_extensions_.push_back(strdup(l.extensionName));
            }
        }
        if (strcmp(l.extensionName, VK_KHR_SURFACE_EXTENSION_NAME) == 0) {
            supports_minimum_required[0] = true;
            enabled_instance_extensions_.push_back(strdup(l.extensionName));
        }
        if (strcmp(l.extensionName, VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME) == 0) {
            supports_minimum_required[1] = true;
            enabled_instance_extensions_.push_back(strdup(l.extensionName));
        }
    }

    if (!supports_minimum_required[0] && !supports_minimum_required[1]) {
        spdlog::critical("The Vulkan driver does not support the required instance extensions");
        exit(EXIT_FAILURE);
    }

    std::ostringstream ss;
    ss << "Enabling " << enabled_instance_extensions_.size() << " instance extensions";
    if (!enabled_instance_extensions_.empty()) {
        ss << ":";
    }
    spdlog::info(ss.str().c_str());
    for (auto &extension: enabled_instance_extensions_) {
        spdlog::debug("\t{}", extension);
    }

    VkApplicationInfo app_info{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = app_id_.c_str();
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "No Engine";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_MAKE_VERSION(1, 1, 0);

    VkInstanceCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    info.pApplicationInfo = &app_info;
    info.enabledExtensionCount = static_cast<uint32_t>(enabled_instance_extensions_.size());
    info.ppEnabledExtensionNames = enabled_instance_extensions_.data();

    static constexpr char VK_LAYER_KHRONOS_VALIDATION_NAME[] = "VK_LAYER_KHRONOS_validation";

    auto available_layers = vk::enumerateInstanceLayerProperties();
    SPDLOG_DEBUG("Vulkan Instance Layers:");
    for (const auto &l: available_layers.value) {
        SPDLOG_DEBUG("\t{} - {}", l.layerName, l.description);
        if (enable_validation_layers_ && strcmp(l.layerName, VK_LAYER_KHRONOS_VALIDATION_NAME) == 0) {
            enabled_layer_extensions_.push_back(VK_LAYER_KHRONOS_VALIDATION_NAME);
            break;
        }
    }

    ss.clear();
    ss.str("");
    ss << "Enabling " << enabled_layer_extensions_.size() << " layer extensions";
    if (!enabled_layer_extensions_.empty()) {
        ss << ":";
    }
    for (const auto &layer: enabled_layer_extensions_) {
        ss << "\n\t" << layer;
    }
    spdlog::info(ss.str());

    info.enabledLayerCount = static_cast<uint32_t>(enabled_layer_extensions_.size());
    info.ppEnabledLayerNames = enabled_layer_extensions_.data();

    CHECK_VK_RESULT(d_.vkCreateInstance(&info, nullptr, &instance_) != VK_SUCCESS);

    VULKAN_HPP_DEFAULT_DISPATCHER.init(vk::Instance(instance_));
}

void VulkanBackend::setupDebugMessenger() {
    if (!enable_validation_layers_)
        return;

    if (debugUtilsSupported_) {
        VkDebugUtilsMessengerCreateInfoEXT createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        createInfo.messageSeverity =
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        createInfo.pfnUserCallback = debugUtilsCallback;

        CHECK_VK_RESULT(d_.vkCreateDebugUtilsMessengerEXT(
                instance_, &createInfo, VKALLOC, &mDebugMessenger) != VK_SUCCESS);
    } else if (d_.vkCreateDebugReportCallbackEXT) {
        VkDebugReportCallbackCreateInfoEXT cb_info{};
        cb_info.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
        cb_info.flags = VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_ERROR_BIT_EXT;
        cb_info.pfnCallback = debugReportCallback;
        CHECK_VK_RESULT(d_.vkCreateDebugReportCallbackEXT(
                instance_, &cb_info, VKALLOC, &mDebugCallback) != VK_SUCCESS);
    }
}

void VulkanBackend::findPhysicalDevice() {
    uint32_t count;
    CHECK_VK_RESULT(d_.vkEnumeratePhysicalDevices(instance_, &count, nullptr));
    std::vector<VkPhysicalDevice> physical_devices(count);
    CHECK_VK_RESULT(d_.vkEnumeratePhysicalDevices(instance_, &count, physical_devices.data()));

    SPDLOG_DEBUG("Enumerating {} physical device(s).", count);

    uint32_t selected_score = 0;
    for (const auto &physical_device: physical_devices) {
        VkPhysicalDeviceProperties properties;
        VkPhysicalDeviceFeatures features;
        d_.vkGetPhysicalDeviceProperties(physical_device, &properties);
        d_.vkGetPhysicalDeviceFeatures(physical_device, &features);

        SPDLOG_DEBUG("Checking device: {}", properties.deviceName);

        uint32_t score = 0;
        std::vector<const char *> supported_extensions;

        uint32_t qfp_count;
        d_.vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &qfp_count, nullptr);
        std::vector<VkQueueFamilyProperties> qfp(qfp_count);
        d_.vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &qfp_count, qfp.data());
        std::optional<uint32_t> graphics_queue_family;
        for (uint32_t i = 0; i < qfp.size(); i++) {
            SPDLOG_DEBUG("Queue Count: {}", qfp[i].queueCount);

            // Only pick graphics queues that can also present to the surface.
            // Graphics queues that can't present are rare if not nonexistent, but
            // the spec allows for this, so check it anyhow.
            VkBool32 surface_present_supported;
            CHECK_VK_RESULT(d_.vkGetPhysicalDeviceSurfaceSupportKHR(
                    physical_device, i, surface_, &surface_present_supported));

            if (!graphics_queue_family.has_value() &&
                qfp[i].queueFlags & VK_QUEUE_GRAPHICS_BIT &&
                surface_present_supported) {
                graphics_queue_family = i;
            }
        }

        // Skip physical devices that don't have a graphics queue.
        if (!graphics_queue_family.has_value()) {
            spdlog::info("  - Skipping due to no suitable graphics queues.");
            continue;
        }

        // Prefer discrete GPUs.
        if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            score += 1 << 30;
        }

        uint32_t extension_count;
        CHECK_VK_RESULT(d_.vkEnumerateDeviceExtensionProperties(
                physical_device, nullptr, &extension_count, nullptr));
        std::vector<VkExtensionProperties> available_extensions(extension_count);
        CHECK_VK_RESULT(d_.vkEnumerateDeviceExtensionProperties(
                physical_device, nullptr, &extension_count,
                available_extensions.data()));

        bool supports_swapchain = false;
        for (const auto &available_extension: available_extensions) {
            if (strcmp(VK_KHR_SWAPCHAIN_EXTENSION_NAME, available_extension.extensionName) == 0) {
                supports_swapchain = true;
                supported_extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
            }
                // The spec requires VK_KHR_portability_subset be enabled whenever it's
                // available on a device. It's present on compatibility ICDs like MoltenVK.
            else if (strcmp("VK_KHR_portability_subset", available_extension.extensionName) == 0) {
                supported_extensions.push_back("VK_KHR_portability_subset");
            }
                // Prefer GPUs that support VK_KHR_get_memory_requirements2.
            else if (strcmp(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME, available_extension.extensionName) == 0) {
                score += 1 << 29;
                supported_extensions.push_back(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME);
            }
        }

        // Skip physical devices that don't have swapchain support.
        if (!supports_swapchain) {
            SPDLOG_DEBUG("  - Skipping due to lack of swapchain support.");
            continue;
        }

        // Prefer GPUs with larger max texture sizes.
        score += properties.limits.maxImageDimension2D;

        if (selected_score < score) {
            SPDLOG_DEBUG("  - This is the best device so far. Score: 0x{:x}", score);

            selected_score = score;
            physical_device_ = physical_device;
            enabled_device_extensions_ = supported_extensions;
            spdlog::debug("supported extensions: {}", supported_extensions.size());
            for (auto &extension: supported_extensions)
                spdlog::debug("\t{}", extension);
            queue_family_index_ = graphics_queue_family.value_or(std::numeric_limits<uint32_t>::max());

            // Bingo, we finally found a physical device that supports everything we need.
            d_.vkGetPhysicalDeviceFeatures(physical_device, &physical_device_features_);
            d_.vkGetPhysicalDeviceProperties(physical_device, &physical_device_properties_);
            d_.vkGetPhysicalDeviceMemoryProperties(physical_device, &physical_device_memory_properties_);

            // Print some driver or MoltenVK information if it is available.
            if (d_.vkGetPhysicalDeviceProperties2KHR) {
                VkPhysicalDeviceDriverProperties driverProperties{};
                driverProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES;

                VkPhysicalDeviceProperties2 physicalDeviceProperties2{};
                physicalDeviceProperties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
                physicalDeviceProperties2.pNext = &driverProperties;

                d_.vkGetPhysicalDeviceProperties2KHR(physical_device_, &physicalDeviceProperties2);
                spdlog::info("Vulkan device driver: {} {}", driverProperties.driverName, driverProperties.driverInfo);
            }

            // Print out some properties of the GPU for diagnostic purposes.
            spdlog::info("Vendor {:x}, device {:x}, driver {:x}, api {}.{}",
                         properties.vendorID, properties.deviceID,
                         properties.driverVersion,
                         VK_VERSION_MAJOR(properties.apiVersion),
                         VK_VERSION_MINOR(properties.apiVersion));
            break;
        }
    }

    if (physical_device_ == nullptr) {
        spdlog::critical("Failed to find a compatible Vulkan physical device.");
        exit(EXIT_FAILURE);
    }
}

void VulkanBackend::createLogicalDevice() {
    std::ostringstream ss;
    ss << "Enabling " << enabled_device_extensions_.size() << " device extensions";
    if (!enabled_device_extensions_.empty()) {
        ss << ":";
    }
    spdlog::debug(ss.str().c_str());
    for (const char *extension: enabled_device_extensions_) {
        spdlog::debug("\t{}", extension);
    }

    float priority = 1.0f;
    VkDeviceQueueCreateInfo queue_info{};
    queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_info.queueFamilyIndex = queue_family_index_;
    queue_info.queueCount = 1; //TODO - driver can support more than one
    queue_info.pQueuePriorities = &priority;

    VkPhysicalDeviceFeatures device_features{};
    VkDeviceCreateInfo device_info{};
    device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_info.queueCreateInfoCount = 1;
    device_info.pQueueCreateInfos = &queue_info;
    device_info.enabledExtensionCount = static_cast<uint32_t>(enabled_device_extensions_.size());
    device_info.ppEnabledExtensionNames = enabled_device_extensions_.data();
    device_info.pEnabledFeatures = &device_features;

    CHECK_VK_RESULT(d_.vkCreateDevice(physical_device_, &device_info, nullptr, &device_));

    d_.vkGetDeviceQueue(device_, queue_family_index_, 0, &queue_);
}

bool VulkanBackend::InitializeSwapChain() {
    if (resize_pending_) {
        resize_pending_ = false;
        d_.vkDestroySwapchainKHR(device_, swapchain_, nullptr);

        CHECK_VK_RESULT(d_.vkQueueWaitIdle(queue_));
        CHECK_VK_RESULT(
                d_.vkResetCommandPool(device_, swapchain_command_pool_,
                                      VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT));
    }

    // --------------------------------------------------------------------------
    // Choose an image format that can be presented to the surface, preferring
    // the common BGRA+sRGB if available.
    // --------------------------------------------------------------------------

    uint32_t format_count;
    CHECK_VK_RESULT(d_.vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device_, surface_, &format_count, nullptr));
    std::vector<VkSurfaceFormatKHR> formats(format_count);
    CHECK_VK_RESULT(d_.vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device_, surface_, &format_count, formats.data()));

    surface_format_ = formats[0];
    for (const auto &format: formats) {
        if (format.format == VK_FORMAT_B8G8R8A8_UNORM &&
            format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            surface_format_ = format;
            break;
        }
    }

    // --------------------------------------------------------------------------
    // Choose the presentable image size that's as close as possible to the
    // window size.
    // --------------------------------------------------------------------------

    VkExtent2D clientSize;

    VkSurfaceCapabilitiesKHR surface_capabilities;
    CHECK_VK_RESULT(d_.vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device_, surface_, &surface_capabilities));

    if (surface_capabilities.currentExtent.width != UINT32_MAX) {
        // If the surface reports a specific extent, we must use it.
        clientSize = surface_capabilities.currentExtent;
    } else {
        VkExtent2D actual_extent{};
        actual_extent.width = width_;
        actual_extent.height = height_;

        clientSize.width =
                std::max(surface_capabilities.minImageExtent.width,
                         std::min(surface_capabilities.maxImageExtent.width,
                                  actual_extent.width));
        clientSize.height =
                std::max(surface_capabilities.minImageExtent.height,
                         std::min(surface_capabilities.maxImageExtent.height,
                                  actual_extent.height));
    }

    // --------------------------------------------------------------------------
    // Desired image count
    // --------------------------------------------------------------------------

    const uint32_t maxImageCount = surface_capabilities.maxImageCount;
    const uint32_t minImageCount = surface_capabilities.minImageCount;
    uint32_t desiredImageCount = minImageCount + 1;

    // According to section 30.5 of VK 1.1, maxImageCount of zero means "that
    // there is no limit on the number of images, though there may be limits
    // related to the total amount of memory used by presentable images."
    if (maxImageCount != 0 && desiredImageCount > maxImageCount) {
        spdlog::error("Swap chain does not support {} images.", desiredImageCount);
        desiredImageCount = surface_capabilities.minImageCount;
    }

    // --------------------------------------------------------------------------
    // Choose the present mode.
    // --------------------------------------------------------------------------

    uint32_t mode_count;
    CHECK_VK_RESULT(d_.vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device_, surface_, &mode_count, nullptr));
    std::vector<VkPresentModeKHR> modes(mode_count);
    CHECK_VK_RESULT(
            d_.vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device_, surface_, &mode_count, modes.data()));
    assert(!formats.empty());  // Shouldn't be possible.

    // If the preferred mode isn't available, just choose the first one.
    VkPresentModeKHR present_mode = modes[0];
    for (const auto &mode: modes) {
        if (mode == kPreferredPresentMode) {
            present_mode = mode;
            break;
        }
    }

    // --------------------------------------------------------------------------
    // Create the swapchain.
    // --------------------------------------------------------------------------

    const VkCompositeAlphaFlagBitsKHR compositeAlpha =
            (surface_capabilities.supportedCompositeAlpha &
             VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR)
            ? VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR
            : VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

    VkSwapchainCreateInfoKHR info{};
    info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    info.surface = surface_;
    info.minImageCount = desiredImageCount;
    info.imageFormat = surface_format_.format;
    info.imageColorSpace = surface_format_.colorSpace;
    info.imageExtent = clientSize;
    info.imageArrayLayers = 1;
    info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.preTransform = surface_capabilities.currentTransform;
    info.compositeAlpha = compositeAlpha;
    info.presentMode = present_mode;
    info.clipped = VK_TRUE;

    auto result = d_.vkCreateSwapchainKHR(device_, &info, VKALLOC, &swapchain_);
    CHECK_VK_RESULT(result);
    if (result != VK_SUCCESS) {
        return false;
    }

    // --------------------------------------------------------------------------
    // Fetch SwapChain images
    // --------------------------------------------------------------------------

    uint32_t image_count;
    CHECK_VK_RESULT(d_.vkGetSwapchainImagesKHR(device_, swapchain_, &image_count, nullptr));
    swapchain_images_.reserve(image_count);
    CHECK_VK_RESULT(d_.vkGetSwapchainImagesKHR(device_, swapchain_, &image_count, swapchain_images_.data()));
    swapchain_images_.resize(image_count);

    SPDLOG_DEBUG("Swapchain Image Count: {}", swapchain_images_.size());

    // --------------------------------------------------------------------------
    // Record a command buffer for each of the images to be executed prior to
    // presenting.
    // --------------------------------------------------------------------------

    present_transition_buffers_.resize(swapchain_images_.size());

    VkCommandBufferAllocateInfo buffers_info{};
    buffers_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    buffers_info.commandPool = swapchain_command_pool_;
    buffers_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    buffers_info.commandBufferCount = static_cast<uint32_t>(present_transition_buffers_.size());

    CHECK_VK_RESULT(d_.vkAllocateCommandBuffers(device_, &buffers_info, present_transition_buffers_.data()));

    for (size_t i = 0; i < swapchain_images_.size(); i++) {
        auto image = swapchain_images_[i];
        auto buffer = present_transition_buffers_[i];

        VkCommandBufferBeginInfo begin_info{};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        CHECK_VK_RESULT(d_.vkBeginCommandBuffer(buffer, &begin_info));

        // Filament Engine hands back the image after writing to it
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
        };
        d_.vkCmdPipelineBarrier(buffer,
                                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr,
                                0, nullptr, 1, &barrier);

        CHECK_VK_RESULT(d_.vkEndCommandBuffer(buffer));
    }

    return true;
}

VKAPI_ATTR VkBool32

VKAPI_CALL VulkanBackend::debugReportCallback(
        VkDebugReportFlagsEXT flags,
        VkDebugReportObjectTypeEXT objectType,
        uint64_t object,
        size_t location,
        int32_t messageCode,
        const char *pLayerPrefix,
        const char *pMessage,
        void *pUserData) {
    (void) objectType;
    (void) object;
    (void) location;
    (void) messageCode;
    (void) pUserData;
    if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT) {
        spdlog::error("VULKAN ERROR: ({}) {}", pLayerPrefix, pMessage);
    } else {
        spdlog::warn("VULKAN WARNING: ({}) {}", pLayerPrefix, pMessage);
    }
    return VK_FALSE;
}

VKAPI_ATTR VkBool32 VKAPI_CALL VulkanBackend::debugUtilsCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT severity,
        VkDebugUtilsMessageTypeFlagsEXT types,
        const VkDebugUtilsMessengerCallbackDataEXT *cbdata,
        void *pUserData) {
    (void) types;
    (void) pUserData;
    if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        spdlog::error("VULKAN ERROR: ({}) {}", cbdata->pMessageIdName,
                      cbdata->pMessage);
    } else {
        // TODO: emit best practices warnings about aggressive pipeline barriers.
        if (strstr(cbdata->pMessage, "ALL_GRAPHICS_BIT") ||
            strstr(cbdata->pMessage, "ALL_COMMANDS_BIT")) {
            return VK_FALSE;
        }
        spdlog::warn("VULKAN WARNING: ({}) {}", cbdata->pMessageIdName,
                     cbdata->pMessage);
    }
    return VK_TRUE;
}


void VulkanBackend::Resize(int32_t width,
                           int32_t height) {
    if (width_ != width || height_ != height) {
        resize_pending_ = true;
        width_ = static_cast<uint32_t>(width);
        height_ = static_cast<uint32_t>(height);
    }
}

void VulkanBackend::CreateSurface(struct wl_display *display,
                                  struct wl_surface *surface,
                                  int32_t width,
                                  int32_t height,
                                  uint32_t command_buffer_count) {
    SPDLOG_DEBUG("CreateSurface");

    if (surface_ != VK_NULL_HANDLE) {
        spdlog::error("Vulkan Surface already exists");
        return;
    }

    assert(instance_ != VK_NULL_HANDLE);
    assert(surface != nullptr);

    wl_display_ = display;
    wl_surface_ = surface;
    width_ = static_cast<uint32_t>(width);
    height_ = static_cast<uint32_t>(height);
    swapchain_command_buffers_count_ = command_buffer_count;

    VkWaylandSurfaceCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
    createInfo.display = wl_display_;
    createInfo.surface = wl_surface_;

    CHECK_VK_RESULT(
            d_.vkCreateWaylandSurfaceKHR(instance_, &createInfo, nullptr, &surface_));

    findPhysicalDevice();
    createLogicalDevice();

    // --------------------------------------------------------------------------
    // Create command pool, buffers, and sync primitives
    // --------------------------------------------------------------------------

    VkSemaphoreCreateInfo s_info{};
    s_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    d_.vkCreateSemaphore(device_, &s_info, nullptr, &post_acquire_semaphore_);
    d_.vkCreateSemaphore(device_, &s_info, nullptr, &pre_submit_semaphore_);

    VkCommandPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool_info.queueFamilyIndex = queue_family_index_;
    d_.vkCreateCommandPool(device_, &pool_info, nullptr, &swapchain_command_pool_);

    VkFenceCreateInfo f_info{};
    f_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    d_.vkCreateFence(device_, &f_info, nullptr, &exec_fence_);

    if (swapchain_command_buffers_count_) {
        VkCommandBufferAllocateInfo buffer_info{};
        buffer_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        buffer_info.commandPool = swapchain_command_pool_;
        buffer_info.commandBufferCount = swapchain_command_buffers_count_;
        buffer_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        d_.vkAllocateCommandBuffers(device_, &buffer_info, &swapchain_buffers_);
        spdlog::debug("Created {} command buffers", swapchain_command_buffers_count_);
    }

    if (!InitializeSwapChain()) {
        spdlog::critical("Failed to create swapchain.");
        exit(EXIT_FAILURE);
    }
}
