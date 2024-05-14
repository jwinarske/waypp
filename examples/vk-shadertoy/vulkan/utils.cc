
#include "utils.h"

#include <filesystem>
#include <fstream>

#include "logging.h"

#include <sys/time.h>

#include <chrono>
#include <thread>

VulkanUtils::VulkanUtils() = default;

VulkanUtils::~VulkanUtils() = default;


void VulkanUtils::exit(VkInstance vk) {
    VULKAN_HPP_DEFAULT_DISPATCHER.vkDestroyInstance(vk, nullptr);
}

vk_error
VulkanUtils::enumerate_devices(VkInstance vk, VkSurfaceKHR *surface, struct vk_physical_device *devs, uint32_t *idx,
                               bool use_idx) {

    vk_error retval = VK_ERROR_NONE;
    VkResult res;
    uint32_t count = 0;

    bool last_use_idx = false;
    uint32_t last_idx = 0; // last non DISCRETE_GPU

    res = VULKAN_HPP_DEFAULT_DISPATCHER.vkEnumeratePhysicalDevices(vk, &count, nullptr);
    vk_error_set_vkresult(&retval, res);
    if (res < 0) {
        return retval;
    }
    if (count < 1) {
        printf("No Vulkan device found.\n");
        vk_error_set_vkresult(&retval, VK_ERROR_INCOMPATIBLE_DRIVER);
        return retval;
    }

    VkPhysicalDevice *phy_devs;
    phy_devs = (VkPhysicalDevice *) malloc(count * sizeof(VkPhysicalDevice));


    res = VULKAN_HPP_DEFAULT_DISPATCHER.vkEnumeratePhysicalDevices(vk, &count, phy_devs);
    vk_error_set_vkresult(&retval, res);
    if (res < 0) {
        free(phy_devs);
        phy_devs = nullptr;
        return retval;
    }

    for (uint32_t i = 0; i < count && (!use_idx); i++) {
        uint32_t qfc = 0;
        VULKAN_HPP_DEFAULT_DISPATCHER.vkGetPhysicalDeviceQueueFamilyProperties(phy_devs[i], &qfc, nullptr);
        if (qfc < 1)continue;

        VkQueueFamilyProperties *queue_family_properties;
        queue_family_properties = (VkQueueFamilyProperties *) malloc(qfc * sizeof(VkQueueFamilyProperties));

        VULKAN_HPP_DEFAULT_DISPATCHER.vkGetPhysicalDeviceQueueFamilyProperties(phy_devs[i], &qfc,
                                                                               queue_family_properties);

        for (uint32_t j = 0; j < qfc; j++) {
            VkBool32 supports_present;
            VULKAN_HPP_DEFAULT_DISPATCHER.vkGetPhysicalDeviceSurfaceSupportKHR(phy_devs[i], j, *surface,
                                                                               &supports_present);

            if ((queue_family_properties[j].queueFlags & VK_QUEUE_GRAPHICS_BIT) && supports_present) {
                VkPhysicalDeviceProperties pr;
                VULKAN_HPP_DEFAULT_DISPATCHER.vkGetPhysicalDeviceProperties(phy_devs[i], &pr);
                if (pr.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
                    *idx = i;
                    use_idx = true;
                } else {
                    last_use_idx = true;
                    last_idx = i;
                }
                break;
            }
        }
        free(queue_family_properties);
    }

    if (last_use_idx && (!use_idx)) {
        use_idx = true;
        *idx = last_idx;
    }

    if (!use_idx) {
        printf("Not found suitable queue which supports graphics.\n");
        vk_error_set_vkresult(&retval, VK_ERROR_INCOMPATIBLE_DRIVER);
        free(phy_devs);
        phy_devs = nullptr;
        return retval;
    }
    if (*idx >= count) {
        printf("Wrong GPU index %lu, max devices count %lu\n", (unsigned long) *idx, (unsigned long) count);
        vk_error_set_vkresult(&retval, VK_ERROR_INCOMPATIBLE_DRIVER);
        free(phy_devs);
        phy_devs = nullptr;
        return retval;
    }

    printf("Using GPU device %lu\n", (unsigned long) *idx);

    devs[0].physical_device = phy_devs[*idx];

    VULKAN_HPP_DEFAULT_DISPATCHER.vkGetPhysicalDeviceProperties(devs[0].physical_device, &devs[0].properties);
    VULKAN_HPP_DEFAULT_DISPATCHER.vkGetPhysicalDeviceFeatures(devs[0].physical_device, &devs[0].features);
    VULKAN_HPP_DEFAULT_DISPATCHER.vkGetPhysicalDeviceMemoryProperties(devs[0].physical_device, &devs[0].memories);

    printf("Vulkan GPU - %s: %s (id: 0x%04X) from vendor 0x%04X [driver version: 0x%04X, API version: 0x%04X]\n",
           vk_VkPhysicalDeviceType_string(devs[0].properties.deviceType), devs[0].properties.deviceName,
           devs[0].properties.deviceID, devs[0].properties.vendorID, devs[0].properties.driverVersion,
           devs[0].properties.apiVersion);

    uint32_t qfc = 0;
    devs[0].queue_family_count = kMaxQueueFamily;
    VULKAN_HPP_DEFAULT_DISPATCHER.vkGetPhysicalDeviceQueueFamilyProperties(devs[0].physical_device, &qfc, nullptr);
    VULKAN_HPP_DEFAULT_DISPATCHER.vkGetPhysicalDeviceQueueFamilyProperties(devs[0].physical_device,
                                                                           &devs[0].queue_family_count,
                                                                           devs[0].queue_families);

    devs[0].queue_families_incomplete = devs[0].queue_family_count < qfc;

    free(phy_devs);
    phy_devs = nullptr;
    return retval;
}


vk_error
VulkanUtils::get_commands(struct vk_physical_device *phy_dev, struct vk_device *dev,
                          VkDeviceQueueCreateInfo queue_info[],
                          uint32_t queue_info_count, uint32_t create_count) {
    vk_error retval = VK_ERROR_NONE;
    VkResult res;
    bool create_num_cmd = false; //create many cmd buffers in one Queue

    if (create_count > 0) {
        queue_info_count = 1;
        create_num_cmd = true;
    }

    dev->command_pools = (struct vk_commands *) malloc(queue_info_count * sizeof *dev->command_pools);
    if (dev->command_pools == nullptr) {
        vk_error_set_errno(&retval, errno);
        return retval;
    }

    for (uint32_t i = 0; i < queue_info_count; ++i) {
        struct vk_commands *cmd = &dev->command_pools[i];
        *cmd = (struct vk_commands) {0};

        cmd->qflags = phy_dev->queue_families[queue_info[i].queueFamilyIndex].queueFlags;

        VkCommandPoolCreateInfo pool_info{};
        pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        pool_info.queueFamilyIndex = queue_info[i].queueFamilyIndex;

        res = VULKAN_HPP_DEFAULT_DISPATCHER.vkCreateCommandPool(dev->device, &pool_info, nullptr, &cmd->pool);
        vk_error_set_vkresult(&retval, res);
        if (res < 0)
            return retval;
        ++dev->command_pool_count;

        cmd->queues = (VkQueue *) malloc(queue_info[i].queueCount * sizeof *cmd->queues);
        if (!create_num_cmd) {
            cmd->buffers = (VkCommandBuffer *) malloc(queue_info[i].queueCount * sizeof *cmd->buffers);
        } else {
            cmd->buffers = (VkCommandBuffer *) malloc(create_count * sizeof *cmd->buffers);
        }
        if (cmd->queues == nullptr || cmd->buffers == nullptr) {
            vk_error_set_errno(&retval, errno);
            return retval;
        }

        for (uint32_t j = 0; j < queue_info[i].queueCount; ++j)
            VULKAN_HPP_DEFAULT_DISPATCHER.vkGetDeviceQueue(dev->device, queue_info[i].queueFamilyIndex, j,
                                                           &cmd->queues[j]);
        cmd->queue_count = queue_info[i].queueCount;

        VkCommandBufferAllocateInfo buffer_info{};
        buffer_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        buffer_info.commandPool = cmd->pool;
        buffer_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        buffer_info.commandBufferCount = create_num_cmd ? create_count : queue_info[i].queueCount;

        res = VULKAN_HPP_DEFAULT_DISPATCHER.vkAllocateCommandBuffers(dev->device, &buffer_info, cmd->buffers);
        vk_error_set_vkresult(&retval, res);
        if (res)
            return retval;

        cmd->buffer_count = create_num_cmd ? create_count : queue_info[i].queueCount;
    }
    return retval;

}

void VulkanUtils::cleanup(struct vk_device *dev) {
    VULKAN_HPP_DEFAULT_DISPATCHER.vkDeviceWaitIdle(dev->device);

    for (uint32_t i = 0; i < dev->command_pool_count; ++i) {
        free(dev->command_pools[i].queues);
        free(dev->command_pools[i].buffers);
        VULKAN_HPP_DEFAULT_DISPATCHER.vkDestroyCommandPool(dev->device, dev->command_pools[i].pool, nullptr);
    }
    free(dev->command_pools);

    VULKAN_HPP_DEFAULT_DISPATCHER.vkDestroyDevice(dev->device, nullptr);

    *dev = (struct vk_device) {};
}

vk_error VulkanUtils::load_shader(struct vk_device *dev, const uint32_t *code, VkShaderModule *shader, size_t size) {
    vk_error retval = VK_ERROR_NONE;
    VkResult res;
    VkShaderModuleCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.codeSize = size;
    info.pCode = code;

    res = VULKAN_HPP_DEFAULT_DISPATCHER.vkCreateShaderModule(dev->device, &info, nullptr, shader);
    vk_error_set_vkresult(&retval, res);

    return retval;
}

#ifdef YARIV_SHADER
vk_error vk_load_shader_yariv(struct vk_device *dev, const uint32_t *yariv_code, VkShaderModule *shader, size_t in_yariv_size)
{
    vk_error retval = VK_ERROR_NONE;
    void *in_yariv = (void *)yariv_code;
    size_t out_spirv_size = yariv_decode_size(in_yariv, in_yariv_size);
    uint32_t *out_spirv = malloc(out_spirv_size);
    yariv_decode(out_spirv, out_spirv_size, in_yariv, in_yariv_size);
    retval = vk_load_shader(dev, out_spirv, shader, out_spirv_size);

    free(out_spirv);
    return retval;
}
#endif


vk_error VulkanUtils::load_shader_spirv_file(struct vk_device *dev, const char *spirv_file, VkShaderModule *shader) {
    vk_error retval = VK_ERROR_NONE;

    std::filesystem::path path = spirv_file;
    if (path.empty() || !std::filesystem::exists(path)) {
        spdlog::error("[load_shader_spirv_file] invalid path: {}", path.c_str());
        vk_error_set_errno(&retval, errno);
        return retval;
    }

    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        spdlog::error("[load_shader_spirv_file] Failed to open: {}", path.c_str());
        vk_error_set_errno(&retval, errno);
        return retval;
    }

    const auto end = file.tellg();
    file.seekg(0, std::ios::beg);
    auto file_size = static_cast<std::size_t>(end - file.tellg());

    std::vector<uint8_t> buffer(file_size);
    if (!file.read(reinterpret_cast<char *>(buffer.data()),
                   static_cast<long>(buffer.size()))) {
        buffer.clear();
        spdlog::error("[load_shader_spirv_file] Failed to read: [{}] {}", file_size, path.c_str());
        vk_error_set_errno(&retval, errno);
        return retval;
    }

    retval = load_shader(dev, reinterpret_cast<uint32_t *>(buffer.data()), shader, buffer.size());

    return retval;
}

void VulkanUtils::free_shader(struct vk_device *dev, VkShaderModule shader) {
    VULKAN_HPP_DEFAULT_DISPATCHER.vkDestroyShaderModule(dev->device, shader, nullptr);
}

uint32_t VulkanUtils::find_suitable_memory(struct vk_physical_device *phy_dev, struct vk_device * /* dev */,
                                           VkMemoryRequirements *mem_req, VkMemoryPropertyFlags properties) {
    for (uint32_t i = 0; i < phy_dev->memories.memoryTypeCount; ++i) {
        if ((mem_req->memoryTypeBits & 1 << i) == 0)
            continue;
        if (phy_dev->memories.memoryHeaps[phy_dev->memories.memoryTypes[i].heapIndex].size < mem_req->size)
            continue;
        if ((phy_dev->memories.memoryTypes[i].propertyFlags & properties) == properties)
            return i;
    }

    return phy_dev->memories.memoryTypeCount;
}


vk_error VulkanUtils::init_ext(VkInstance *vk, const char *ext_names[], uint32_t ext_count) {
    vk_error retval = VK_ERROR_NONE;
    VkResult res;

    VkApplicationInfo app_info{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "Vulkan Shader launcher";
    app_info.applicationVersion = 0x010000;
    app_info.pEngineName = "Vulkan Shader launcher";
    app_info.engineVersion = 0x010000;
    app_info.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    info.pNext = nullptr;
    info.pApplicationInfo = &app_info;
    info.enabledLayerCount = 0;
    info.ppEnabledLayerNames = nullptr;
    info.enabledExtensionCount = ext_count;
    info.ppEnabledExtensionNames = ext_names;

    res = VULKAN_HPP_DEFAULT_DISPATCHER.vkCreateInstance(&info, nullptr, vk);
    vk_error_set_vkresult(&retval, res);

    VULKAN_HPP_DEFAULT_DISPATCHER.init(vk::Instance(*vk));

    return retval;
}

vk_error VulkanUtils::get_dev_ext(struct vk_physical_device *phy_dev, struct vk_device *dev, VkQueueFlags qflags,
                                  VkDeviceQueueCreateInfo queue_info[], uint32_t *queue_info_count,
                                  const char *ext_names[], uint32_t ext_count) {
    vk_error retval = VK_ERROR_NONE;
    VkResult res;

    *dev = (struct vk_device) {0};

    uint32_t max_queue_count = *queue_info_count;
    *queue_info_count = 0;

    uint32_t max_family_queues = 0;
    for (uint32_t i = 0; i < phy_dev->queue_family_count; ++i)
        if (max_family_queues < phy_dev->queue_families[i].queueCount)
            max_family_queues = phy_dev->queue_families[i].queueCount;
    float *queue_priorities;
    int tsize = 0;
    if (max_family_queues == 0) tsize = 1;
    queue_priorities = (float *) malloc((max_family_queues + static_cast<uint32_t>(tsize)) * (sizeof(float)));
    memset(queue_priorities, 0, (max_family_queues + static_cast<uint32_t>(tsize)) * (sizeof(float)));

    for (uint32_t i = 0; i < phy_dev->queue_family_count && i < max_queue_count; ++i) {
        if ((phy_dev->queue_families[i].queueFlags & qflags) == 0)
            continue;

        queue_info[(*queue_info_count)++] = (VkDeviceQueueCreateInfo) {
                .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .queueFamilyIndex = i,
                .queueCount = phy_dev->queue_families[i].queueCount,
                .pQueuePriorities = queue_priorities,
        };
    }

    if (*queue_info_count == 0) {
        free(queue_priorities);
        queue_priorities = nullptr;
        vk_error_set_vkresult(&retval, VK_ERROR_FEATURE_NOT_PRESENT);
        return retval;
    }

    VkDeviceCreateInfo dev_info{};
    dev_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    dev_info.queueCreateInfoCount = *queue_info_count;
    dev_info.pQueueCreateInfos = queue_info;
    dev_info.enabledExtensionCount = ext_count;
    dev_info.ppEnabledExtensionNames = ext_names;
    dev_info.pEnabledFeatures = &phy_dev->features;

    res = VULKAN_HPP_DEFAULT_DISPATCHER.vkCreateDevice(phy_dev->physical_device, &dev_info, nullptr, &dev->device);
    vk_error_set_vkresult(&retval, res);

    free(queue_priorities);
    queue_priorities = nullptr;
    return retval;
}

vk_error VulkanUtils::create_surface(VkInstance vk, VkSurfaceKHR *surface, struct app_os_window *os_window) {
    vk_error retval = VK_ERROR_NONE;
    VkResult res;
#if defined(VK_USE_PLATFORM_WIN32_KHR)
    VkWin32SurfaceCreateInfoKHR createInfo;
    createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    createInfo.pNext = nullptr;
    createInfo.flags = 0;
    createInfo.hinstance = os_window->connection;
    createInfo.hwnd = os_window->window;

    res = VULKAN_HPP_DEFAULT_DISPATCHER.vkCreateWin32SurfaceKHR(vk, &createInfo, NULL, surface);
#elif defined(VK_USE_PLATFORM_XCB_KHR)
    VkXcbSurfaceCreateInfoKHR createInfo;
    createInfo.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
    createInfo.pNext = nullptr;
    createInfo.flags = 0;
    createInfo.connection = os_window->connection;
    createInfo.window = os_window->xcb_window;

    res = VULKAN_HPP_DEFAULT_DISPATCHER.vkCreateXcbSurfaceKHR(vk, &createInfo, NULL, surface);
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
    VkWaylandSurfaceCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
    createInfo.pNext = nullptr;
    createInfo.flags = 0;
    createInfo.display = os_window->wl_display;
    createInfo.surface = os_window->wl_surface;

    res = VULKAN_HPP_DEFAULT_DISPATCHER.vkCreateWaylandSurfaceKHR(vk, &createInfo, nullptr, surface);
#endif
    vk_error_set_vkresult(&retval, res);
    return retval;
}

vk_error VulkanUtils::get_swapchain(VkInstance /* vk */, struct vk_physical_device *phy_dev, struct vk_device *dev,
                                    struct vk_swapchain *swapchain, struct app_os_window *os_window,
                                    uint32_t thread_count,
                                    VkPresentModeKHR *present_mode) {
    vk_error retval = VK_ERROR_NONE;
    VkResult res;

    VkSwapchainKHR oldSwapchain = swapchain->swapchain;

    res = VULKAN_HPP_DEFAULT_DISPATCHER.vkGetPhysicalDeviceSurfaceCapabilitiesKHR(phy_dev->physical_device,
                                                                                  swapchain->surface,
                                                                                  &swapchain->surface_caps);
    vk_error_set_vkresult(&retval, res);
    if (res)
        return retval;

    uint32_t image_count = swapchain->surface_caps.minImageCount + thread_count - 1;
    if (swapchain->surface_caps.maxImageCount < image_count && swapchain->surface_caps.maxImageCount != 0)
        image_count = swapchain->surface_caps.maxImageCount;

    uint32_t surface_format_count = 1;
    res = VULKAN_HPP_DEFAULT_DISPATCHER.vkGetPhysicalDeviceSurfaceFormatsKHR(phy_dev->physical_device,
                                                                             swapchain->surface, &surface_format_count,
                                                                             nullptr);
    vk_error_set_vkresult(&retval, res);
    if (res < 0)
        return retval;
    if (surface_format_count < 1) {
        retval.error.type = VK_ERROR_ERRNO;
        vk_error_printf(&retval, "surface_format_count < 1\n");
        return retval;
    }

    VkSurfaceFormatKHR surface_format[184];
    if (surface_format_count >= 184) surface_format_count = 184 - 1;

    res = VULKAN_HPP_DEFAULT_DISPATCHER.vkGetPhysicalDeviceSurfaceFormatsKHR(phy_dev->physical_device,
                                                                             swapchain->surface, &surface_format_count,
                                                                             &surface_format[0]);

    vk_error_set_vkresult(&retval, res);
    if (res < 0)
        return retval;

    swapchain->surface_format = surface_format[0];

    if (surface_format_count > 1) {
        uint32_t suported_format_srgb = VK_FORMAT_B8G8R8A8_SRGB;
        int found_srgb = -1;
        uint32_t suported_format_linear = VK_FORMAT_B8G8R8A8_UNORM;
        int found_linear = -1;

        for (int i = 0; i < surface_format_count; i++) {
            if (surface_format[i].format == suported_format_srgb)found_srgb = i;
            if (surface_format[i].format == suported_format_linear)found_linear = i;
        }

        if (found_linear >= 0)swapchain->surface_format = surface_format[found_linear];
        else if (found_srgb >= 0)swapchain->surface_format = surface_format[found_srgb];
    }

    if (surface_format_count == 1 && swapchain->surface_format.format == VK_FORMAT_UNDEFINED)
        swapchain->surface_format.format = VK_FORMAT_R8G8B8_UNORM;

    swapchain->present_modes_count = kMaxPresentModes;

    res = VULKAN_HPP_DEFAULT_DISPATCHER.vkGetPhysicalDeviceSurfacePresentModesKHR(phy_dev->physical_device,
                                                                                  swapchain->surface,
                                                                                  &swapchain->present_modes_count,
                                                                                  swapchain->present_modes);
    vk_error_set_vkresult(&retval, res);
    if (res >= 0) {
        bool tret = false;
        for (uint32_t i = 0; i < swapchain->present_modes_count; ++i) {
            if (swapchain->present_modes[i] == *present_mode) {
                tret = true;
                break;
            }
        }
        if (!tret) {
            bool a = false, b = false, c = false;
            for (uint32_t i = 0; i < swapchain->present_modes_count; ++i) {
                if (swapchain->present_modes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR) {
                    c = true;
                }
                if (swapchain->present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
                    b = true;
                }
                if (swapchain->present_modes[i] == VK_PRESENT_MODE_FIFO_KHR) {
                    a = true;
                }
            }
            if (a)*present_mode = VK_PRESENT_MODE_FIFO_KHR;
            else if (b)*present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
            else if (c)*present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR;

        }
    }

    VkExtent2D swapchainExtent;
    VkImageFormatProperties format_properties;
    res = VULKAN_HPP_DEFAULT_DISPATCHER.vkGetPhysicalDeviceImageFormatProperties(phy_dev->physical_device,
                                                                                 swapchain->surface_format.format,
                                                                                 VK_IMAGE_TYPE_2D,
                                                                                 VK_IMAGE_TILING_OPTIMAL,
                                                                                 VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, 0,
                                                                                 &format_properties);
    if (res == VK_SUCCESS
        && (format_properties.maxExtent.width >= swapchain->surface_caps.currentExtent.width &&
            format_properties.maxExtent.height >= swapchain->surface_caps.currentExtent.height)
        || (swapchain->surface_caps.currentExtent.width == 0xFFFFFFFF)
            ) {
        if (swapchain->surface_caps.currentExtent.width == 0xFFFFFFFF) {
            swapchainExtent.width = static_cast<uint32_t>(os_window->app_data.iResolution[0]);
            swapchainExtent.height = static_cast<uint32_t>(os_window->app_data.iResolution[1]);

            if (swapchainExtent.width < swapchain->surface_caps.minImageExtent.width) {
                swapchainExtent.width = swapchain->surface_caps.minImageExtent.width;
            } else if (swapchainExtent.width > swapchain->surface_caps.maxImageExtent.width) {
                swapchainExtent.width = swapchain->surface_caps.maxImageExtent.width;
            }

            if (swapchainExtent.height < swapchain->surface_caps.minImageExtent.height) {
                swapchainExtent.height = swapchain->surface_caps.minImageExtent.height;
            } else if (swapchainExtent.height > swapchain->surface_caps.maxImageExtent.height) {
                swapchainExtent.height = swapchain->surface_caps.maxImageExtent.height;
            }
        } else {
            swapchainExtent = swapchain->surface_caps.currentExtent;
            os_window->app_data.iResolution[0] = static_cast<int>(swapchain->surface_caps.currentExtent.width);
            os_window->app_data.iResolution[1] = static_cast<int>(swapchain->surface_caps.currentExtent.height);
        }
    } else {
        printf("Error: too large resolution, currentExtent width, height: %lu, %lu; iResolution.xy: %lu, %lu; maxExtent width, height: %lu, %lu \n",
               (unsigned long) swapchain->surface_caps.currentExtent.width,
               (unsigned long) swapchain->surface_caps.currentExtent.height,
               (unsigned long) os_window->app_data.iResolution[0], (unsigned long) os_window->app_data.iResolution[1],
               (unsigned long) format_properties.maxExtent.width, (unsigned long) format_properties.maxExtent.height);
        os_window->app_data.iResolution[0] = static_cast<int>(swapchain->surface_caps.currentExtent.width);
        os_window->app_data.iResolution[1] = static_cast<int>(swapchain->surface_caps.currentExtent.height);
        if (format_properties.maxExtent.width < swapchain->surface_caps.currentExtent.width)
            os_window->app_data.iResolution[0] = static_cast<int>(format_properties.maxExtent.width);
        if (format_properties.maxExtent.height < swapchain->surface_caps.currentExtent.height)
            os_window->app_data.iResolution[1] = static_cast<int>(format_properties.maxExtent.height);
        swapchainExtent.width = static_cast<uint32_t>(os_window->app_data.iResolution[0]);
        swapchainExtent.height = static_cast<uint32_t>(os_window->app_data.iResolution[1]);
    }

    if (os_window->app_data.iResolution[0] <= 0 || os_window->app_data.iResolution[1] <= 0) {
        os_window->is_minimized = true;
        return VK_ERROR_NONE;
    } else {
        os_window->is_minimized = false;
    }

    VkSwapchainCreateInfoKHR swapchain_info{};
    swapchain_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchain_info.pNext = nullptr;
    swapchain_info.flags = 0; // bug https://github.com/KhronosGroup/Vulkan-ValidationLayers/issues/4274
    swapchain_info.surface = swapchain->surface;
    swapchain_info.minImageCount = image_count;
    swapchain_info.imageFormat = swapchain->surface_format.format;
    swapchain_info.imageColorSpace = swapchain->surface_format.colorSpace;
    swapchain_info.imageExtent.width = swapchainExtent.width;
    swapchain_info.imageExtent.height = swapchainExtent.height;
    swapchain_info.imageArrayLayers = 1;
    swapchain_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchain_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchain_info.preTransform = swapchain->surface_caps.currentTransform;
    swapchain_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchain_info.presentMode = *present_mode;
    swapchain_info.clipped = true;
    swapchain_info.oldSwapchain = oldSwapchain;

    if (swapchain->surface_caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) {
        swapchain_info.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    }

    uint32_t *presentable_queues = nullptr;
    uint32_t presentable_queue_count = 0;

    retval = get_presentable_queues(phy_dev, dev, swapchain->surface, &presentable_queues, &presentable_queue_count);
    if (!vk_error_is_success(&retval) || presentable_queue_count == 0)
        return retval;
    free(presentable_queues);

    res = VULKAN_HPP_DEFAULT_DISPATCHER.vkCreateSwapchainKHR(dev->device, &swapchain_info, nullptr,
                                                             &swapchain->swapchain);
    vk_error_set_vkresult(&retval, res);

    if (oldSwapchain != VK_NULL_HANDLE) {
        VULKAN_HPP_DEFAULT_DISPATCHER.vkDestroySwapchainKHR(dev->device, oldSwapchain, nullptr);
    }

    return retval;
}

void VulkanUtils::free_swapchain(VkInstance vk, struct vk_device *dev, struct vk_swapchain *swapchain) {
    VULKAN_HPP_DEFAULT_DISPATCHER.vkDestroySwapchainKHR(dev->device, swapchain->swapchain, nullptr);
    VULKAN_HPP_DEFAULT_DISPATCHER.vkDestroySurfaceKHR(vk, swapchain->surface, nullptr);

    *swapchain = (struct vk_swapchain) {};
}

VkImage *VulkanUtils::get_swapchain_images(struct vk_device *dev, struct vk_swapchain *swapchain, uint32_t *count) {
    vk_error retval = VK_ERROR_NONE;
    VkResult res;

    uint32_t image_count;
    res = VULKAN_HPP_DEFAULT_DISPATCHER.vkGetSwapchainImagesKHR(dev->device, swapchain->swapchain, &image_count,
                                                                nullptr);
    vk_error_set_vkresult(&retval, res);
    if (res < 0) {
        vk_error_printf(&retval, "Failed to count the number of images in swapchain\n");
        return nullptr;
    }

    VkImage *images = (VkImage *) malloc(image_count * sizeof *images);
    if (images == nullptr) {
        printf("Out of memory\n");
        return nullptr;
    }

    res = VULKAN_HPP_DEFAULT_DISPATCHER.vkGetSwapchainImagesKHR(dev->device, swapchain->swapchain, &image_count,
                                                                images);
    vk_error_set_vkresult(&retval, res);
    if (res < 0) {
        vk_error_printf(&retval, "Failed to get the images in swapchain\n");
        return nullptr;
    }

    if (count)
        *count = image_count;
    return images;
}

vk_error VulkanUtils::create_images(struct vk_physical_device *phy_dev, struct vk_device *dev,
                                    struct vk_image *images, uint32_t image_count) {
    uint32_t successful = 0;
    vk_error retval = VK_ERROR_NONE;
    VkResult res;

    for (uint32_t i = 0; i < image_count; ++i) {
        images[i].image = nullptr;
        images[i].image_mem = nullptr;
        images[i].view = nullptr;
        images[i].sampler = nullptr;

        VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL;
        VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
        VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
        if (images[i].will_be_initialized || images[i].host_visible) {
            images[i].usage = (VkImageUsageFlagBits) (
                    images[i].usage & (VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT));
            layout = VK_IMAGE_LAYOUT_PREINITIALIZED;
            tiling = VK_IMAGE_TILING_LINEAR;
        } else if (images[i].multisample) {
            VkImageFormatProperties format_properties;
            res = VULKAN_HPP_DEFAULT_DISPATCHER.vkGetPhysicalDeviceImageFormatProperties(phy_dev->physical_device,
                                                                                         images[i].format,
                                                                                         VK_IMAGE_TYPE_2D,
                                                                                         tiling, images[i].usage, 0,
                                                                                         &format_properties);
            vk_error_sub_set_vkresult(&retval, res);
            if (res == 0) {
                for (uint32_t s = VK_SAMPLE_COUNT_16_BIT; s != 0; s >>= 1)
                    if ((format_properties.sampleCounts & s)) {
                        samples = (VkSampleCountFlagBits) s;
                        break;
                    }
            }
        }

        uint32_t mipLevels = 1;
        if (images[i].mipmaps) {
            mipLevels =
                    static_cast<uint32_t>(log(std::max(images[i].extent.width, images[i].extent.height)) / log(2)) + 1;
        }

        bool shared = images[i].sharing_queue_count > 1;
        struct VkImageCreateInfo image_info{};
        image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image_info.imageType = VK_IMAGE_TYPE_2D;
        image_info.format = images[i].format;
        image_info.extent = {images[i].extent.width, images[i].extent.height, 1};
        image_info.mipLevels = mipLevels;
        image_info.arrayLayers = 1;
        image_info.samples = samples;
        image_info.tiling = tiling;
        image_info.usage = images[i].usage;
        image_info.sharingMode = shared ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE;
        image_info.queueFamilyIndexCount = shared ? images[i].sharing_queue_count : 0;
        image_info.pQueueFamilyIndices = shared ? images[i].sharing_queues : nullptr;
        image_info.initialLayout = layout;

        res = VULKAN_HPP_DEFAULT_DISPATCHER.vkCreateImage(dev->device, &image_info, nullptr, &images[i].image);
        vk_error_sub_set_vkresult(&retval, res);
        if (res)
            continue;

        VkMemoryRequirements mem_req{};
        VULKAN_HPP_DEFAULT_DISPATCHER.vkGetImageMemoryRequirements(dev->device, images[i].image, &mem_req);
        uint32_t mem_index = find_suitable_memory(phy_dev, dev, &mem_req,
                                                  images[i].host_visible ?
                                                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT :
                                                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (mem_index >= phy_dev->memories.memoryTypeCount)
            continue;

        VkMemoryAllocateInfo mem_info{};
        mem_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        mem_info.allocationSize = mem_req.size;
        mem_info.memoryTypeIndex = mem_index;

        res = VULKAN_HPP_DEFAULT_DISPATCHER.vkAllocateMemory(dev->device, &mem_info, nullptr, &images[i].image_mem);
        vk_error_sub_set_vkresult(&retval, res);
        if (res)
            continue;

        res = VULKAN_HPP_DEFAULT_DISPATCHER.vkBindImageMemory(dev->device, images[i].image, images[i].image_mem, 0);
        vk_error_sub_set_vkresult(&retval, res);
        if (res)
            continue;

        if (images[i].make_view) {
            VkImageViewCreateInfo view_info{};
            view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            view_info.image = images[i].image;
            view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
            view_info.format = images[i].format;
            view_info.components = {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B,
                                    VK_COMPONENT_SWIZZLE_A,};
            view_info.subresourceRange.aspectMask = static_cast<VkImageAspectFlags>(
                    (images[i].usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) == 0 ?
                    VK_IMAGE_ASPECT_COLOR_BIT :
                    VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);
            view_info.subresourceRange.baseMipLevel = 0;
            view_info.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
            view_info.subresourceRange.baseArrayLayer = 0;
            view_info.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

            res = VULKAN_HPP_DEFAULT_DISPATCHER.vkCreateImageView(dev->device, &view_info, nullptr, &images[i].view);
            vk_error_sub_set_vkresult(&retval, res);
            if (res)
                continue;
        }

        if ((images[i].usage & VK_IMAGE_USAGE_SAMPLED_BIT)) {
            VkSamplerCreateInfo sampler_info{};
            sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
            sampler_info.magFilter = images[i].linear ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
            sampler_info.minFilter = images[i].linear ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
            sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            sampler_info.addressModeU = images[i].repeat_mode;
            sampler_info.addressModeV = images[i].repeat_mode;
            sampler_info.addressModeW = images[i].repeat_mode;
            sampler_info.anisotropyEnable = images[i].anisotropyEnable && phy_dev->features.samplerAnisotropy; // false
            sampler_info.maxAnisotropy = phy_dev->properties.limits.maxSamplerAnisotropy;
            sampler_info.minLod = 0;
            sampler_info.maxLod = 1;

            if (images[i].mipmaps) {
                sampler_info.maxLod = (float) mipLevels;
                sampler_info.mipLodBias = 0;
                sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
                sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
            }

            res = VULKAN_HPP_DEFAULT_DISPATCHER.vkCreateSampler(dev->device, &sampler_info, nullptr,
                                                                &images[i].sampler);
            vk_error_sub_set_vkresult(&retval, res);
            if (res)
                continue;
        }

        ++successful;
    }

    vk_error_set_vkresult(&retval, successful == image_count ? VK_SUCCESS : VK_INCOMPLETE);
    return retval;
}

vk_error VulkanUtils::create_buffers(struct vk_physical_device *phy_dev, struct vk_device *dev,
                                     struct vk_buffer *buffers, uint32_t buffer_count) {
    uint32_t successful = 0;
    vk_error retval = VK_ERROR_NONE;
    VkResult res;

    for (uint32_t i = 0; i < buffer_count; ++i) {
        buffers[i].buffer = nullptr;
        buffers[i].buffer_mem = nullptr;
        buffers[i].view = nullptr;

        bool shared = buffers[i].sharing_queue_count > 1;

        VkBufferCreateInfo buffer_info{};
        buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_info.size = buffers[i].size * sizeof(float);
        buffer_info.usage = buffers[i].usage;
        buffer_info.sharingMode = shared ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE;
        buffer_info.queueFamilyIndexCount = shared ? buffers[i].sharing_queue_count : 0;
        buffer_info.pQueueFamilyIndices = shared ? buffers[i].sharing_queues : nullptr;

        res = VULKAN_HPP_DEFAULT_DISPATCHER.vkCreateBuffer(dev->device, &buffer_info, nullptr, &buffers[i].buffer);
        vk_error_sub_set_vkresult(&retval, res);
        if (res)
            continue;

        VkMemoryRequirements mem_req{};
        VULKAN_HPP_DEFAULT_DISPATCHER.vkGetBufferMemoryRequirements(dev->device, buffers[i].buffer, &mem_req);
        uint32_t mem_index = find_suitable_memory(phy_dev, dev, &mem_req,
                                                  buffers[i].host_visible ?
                                                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT :
                                                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (mem_index >= phy_dev->memories.memoryTypeCount)
            continue;

        VkMemoryAllocateInfo mem_info{};
        mem_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        mem_info.allocationSize = mem_req.size;
        mem_info.memoryTypeIndex = mem_index;

        res = VULKAN_HPP_DEFAULT_DISPATCHER.vkAllocateMemory(dev->device, &mem_info, nullptr, &buffers[i].buffer_mem);
        vk_error_sub_set_vkresult(&retval, res);
        if (res)
            continue;

        res = VULKAN_HPP_DEFAULT_DISPATCHER.vkBindBufferMemory(dev->device, buffers[i].buffer, buffers[i].buffer_mem,
                                                               0);
        vk_error_sub_set_vkresult(&retval, res);
        if (res)
            continue;

        if (buffers[i].make_view) {
            if ((buffers[i].usage & VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT) ||
                (buffers[i].usage & VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT)) {
                VkBufferViewCreateInfo view_info{};
                view_info.sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO;
                view_info.buffer = buffers[i].buffer;
                view_info.format = buffers[i].format;
                view_info.offset = 0;
                view_info.range = VK_WHOLE_SIZE;

                res = VULKAN_HPP_DEFAULT_DISPATCHER.vkCreateBufferView(dev->device, &view_info, nullptr,
                                                                       &buffers[i].view);
                vk_error_sub_set_vkresult(&retval, res);
                if (res)
                    continue;
            }
        }

        ++successful;
    }

    vk_error_set_vkresult(&retval, successful == buffer_count ? VK_SUCCESS : VK_INCOMPLETE);
    return retval;
}

vk_error VulkanUtils::load_shaders(struct vk_device *dev,
                                   struct vk_shader *shaders, uint32_t shader_count) {
    uint32_t successful = 0;
    vk_error retval = VK_ERROR_NONE;
    vk_error err;

    for (uint32_t i = 0; i < shader_count; ++i) {
        err = load_shader_spirv_file(dev, shaders[i].spirv_file, &shaders[i].shader);
        vk_error_sub_merge(&retval, &err);
        if (!vk_error_is_success(&err))
            continue;

        ++successful;
    }

    vk_error_set_vkresult(&retval, successful == shader_count ? VK_SUCCESS : VK_INCOMPLETE);
    return retval;
}


vk_error VulkanUtils::get_presentable_queues(struct vk_physical_device *phy_dev, struct vk_device *dev,
                                             VkSurfaceKHR surface, uint32_t **presentable_queues,
                                             uint32_t *presentable_queue_count) {
    vk_error retval = VK_ERROR_NONE;
    VkResult res;

    *presentable_queues = (uint32_t *) malloc(dev->command_pool_count * sizeof **presentable_queues);
    if (*presentable_queues == nullptr) {
        vk_error_set_errno(&retval, errno);
        return retval;
    }
    *presentable_queue_count = 0;

    for (uint32_t i = 0; i < dev->command_pool_count; ++i) {
        VkBool32 supports = false;
        res = VULKAN_HPP_DEFAULT_DISPATCHER.vkGetPhysicalDeviceSurfaceSupportKHR(phy_dev->physical_device, i, surface,
                                                                                 &supports);
        vk_error_sub_set_vkresult(&retval, res);
        if (res || !supports)
            continue;

        (*presentable_queues)[(*presentable_queue_count)++] = i;
    }

    if (*presentable_queue_count == 0) {
        free(*presentable_queues);
        *presentable_queues = nullptr;
    }

    vk_error_set_vkresult(&retval, *presentable_queue_count == 0 ? VK_ERROR_INCOMPATIBLE_DRIVER : VK_SUCCESS);

    return retval;
}

VkFormat VulkanUtils::get_supported_depth_stencil_format(struct vk_physical_device *phy_dev) {
    VkFormat depth_formats[] = {
            VK_FORMAT_D32_SFLOAT_S8_UINT,
            VK_FORMAT_D32_SFLOAT,
            VK_FORMAT_D24_UNORM_S8_UINT,
            VK_FORMAT_X8_D24_UNORM_PACK32,
            VK_FORMAT_D16_UNORM,
    };
    VkFormat selected_format = VK_FORMAT_UNDEFINED;

    for (size_t i = 0; i < sizeof depth_formats / sizeof *depth_formats; ++i) {
        VkFormatProperties format_properties;
        VULKAN_HPP_DEFAULT_DISPATCHER.vkGetPhysicalDeviceFormatProperties(phy_dev->physical_device, depth_formats[i],
                                                                          &format_properties);
        if ((format_properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)) {
            selected_format = depth_formats[i];
            break;
        }
    }

    return selected_format;
}

void VulkanUtils::free_images(struct vk_device *dev, struct vk_image *images, uint32_t image_count) {
    VULKAN_HPP_DEFAULT_DISPATCHER.vkDeviceWaitIdle(dev->device);

    for (uint32_t i = 0; i < image_count; ++i) {
        if (images[i].view)VULKAN_HPP_DEFAULT_DISPATCHER.vkDestroyImageView(dev->device, images[i].view, nullptr);
        VULKAN_HPP_DEFAULT_DISPATCHER.vkDestroyImage(dev->device, images[i].image, nullptr);
        VULKAN_HPP_DEFAULT_DISPATCHER.vkFreeMemory(dev->device, images[i].image_mem, nullptr);
        if (images[i].sampler)VULKAN_HPP_DEFAULT_DISPATCHER.vkDestroySampler(dev->device, images[i].sampler, nullptr);
    }
}

void VulkanUtils::free_buffers(struct vk_device *dev, struct vk_buffer *buffers, uint32_t buffer_count) {
    VULKAN_HPP_DEFAULT_DISPATCHER.vkDeviceWaitIdle(dev->device);

    for (uint32_t i = 0; i < buffer_count; ++i) {
        VULKAN_HPP_DEFAULT_DISPATCHER.vkDestroyBufferView(dev->device, buffers[i].view, nullptr);
        VULKAN_HPP_DEFAULT_DISPATCHER.vkDestroyBuffer(dev->device, buffers[i].buffer, nullptr);
        VULKAN_HPP_DEFAULT_DISPATCHER.vkFreeMemory(dev->device, buffers[i].buffer_mem, nullptr);
    }
}

void VulkanUtils::free_shaders(struct vk_device *dev, struct vk_shader *shaders, uint32_t shader_count) {
    VULKAN_HPP_DEFAULT_DISPATCHER.vkDeviceWaitIdle(dev->device);

    for (uint32_t i = 0; i < shader_count; ++i)
        free_shader(dev, shaders[i].shader);
}

void VulkanUtils::free_graphics_buffers(struct vk_device *dev, struct vk_graphics_buffers *graphics_buffers,
                                        uint32_t graphics_buffer_count,
                                        VkRenderPass render_pass) {
    VULKAN_HPP_DEFAULT_DISPATCHER.vkDeviceWaitIdle(dev->device);

    for (uint32_t i = 0; i < graphics_buffer_count; ++i) {
        free_images(dev, &graphics_buffers[i].depth, 1);
        VULKAN_HPP_DEFAULT_DISPATCHER.vkDestroyImageView(dev->device, graphics_buffers[i].color_view, nullptr);

        VULKAN_HPP_DEFAULT_DISPATCHER.vkDestroyFramebuffer(dev->device, graphics_buffers[i].framebuffer, nullptr);
    }

    VULKAN_HPP_DEFAULT_DISPATCHER.vkDestroyRenderPass(dev->device, render_pass, nullptr);
}


vk_error VulkanUtils::make_graphics_layouts(struct vk_device *dev, struct vk_layout *layouts, uint32_t layout_count,
                                            bool w_img_pattern, uint32_t *img_pattern, uint32_t img_pattern_size) {
    uint32_t successful = 0;
    vk_error retval = VK_ERROR_NONE;
    VkResult res;

    for (uint32_t i = 0; i < layout_count; ++i) {
        struct vk_layout *layout = &layouts[i];
        struct vk_resources *resources = layout->resources;

        layout->set_layout = nullptr;
        layout->pipeline_layout = nullptr;

        VkDescriptorSetLayoutBinding *set_layout_bindings;
        if (w_img_pattern) {
            set_layout_bindings = (VkDescriptorSetLayoutBinding *) malloc(
                    (img_pattern_size + resources->buffer_count) * sizeof(VkDescriptorSetLayoutBinding));
        } else {
            set_layout_bindings = (VkDescriptorSetLayoutBinding *) malloc(
                    (resources->image_count + resources->buffer_count) * sizeof(VkDescriptorSetLayoutBinding));
        }
        uint32_t binding_count = 0;
        uint32_t tidx = 0;
        for (uint32_t j = 0; j < (w_img_pattern ? img_pattern_size : resources->image_count); ++j) {
            if ((resources->images[(w_img_pattern ? tidx : j)].usage &
                 (VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)) == 0)
                continue;
            tidx = 0;
            if (w_img_pattern) {
                for (int tj = 0; tj < j; tj++)tidx += img_pattern[tj];
            }
            set_layout_bindings[binding_count] = {};
            set_layout_bindings[binding_count].binding = w_img_pattern ? tidx : binding_count;
            set_layout_bindings[binding_count].descriptorType =
                    resources->images[(w_img_pattern ? tidx : j)].usage & VK_IMAGE_USAGE_SAMPLED_BIT ?
                    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER :
                    VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            set_layout_bindings[binding_count].descriptorCount = (w_img_pattern ? img_pattern[j] : 1);
            set_layout_bindings[binding_count].stageFlags = resources->images[j].stage;

            ++binding_count;
        }

        for (uint32_t j = 0; j < resources->buffer_count; ++j) {
            if ((resources->buffers[j].usage &
                 (VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)) == 0)
                continue;
            if (w_img_pattern)tidx++;
            set_layout_bindings[binding_count] = {};
            set_layout_bindings[binding_count].binding = w_img_pattern ? tidx : binding_count;
            set_layout_bindings[binding_count].descriptorType =
                    resources->buffers[j].usage & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT ?
                    VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER :
                    VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            set_layout_bindings[binding_count].descriptorCount = 1;
            set_layout_bindings[binding_count].stageFlags = resources->buffers[j].stage;

            ++binding_count;
        }

        VkDescriptorSetLayoutCreateInfo set_layout_info{};
        set_layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        set_layout_info.bindingCount = binding_count;
        set_layout_info.pBindings = set_layout_bindings;

        res = VULKAN_HPP_DEFAULT_DISPATCHER.vkCreateDescriptorSetLayout(dev->device, &set_layout_info, nullptr,
                                                                        &layout->set_layout);
        vk_error_sub_set_vkresult(&retval, res);
        if (res) {
            free(set_layout_bindings);
            set_layout_bindings = nullptr;
            continue;
        }

        VkPipelineLayoutCreateInfo pipeline_layout_info{};
        pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipeline_layout_info.setLayoutCount = 1;
        pipeline_layout_info.pSetLayouts = &layout->set_layout;
        pipeline_layout_info.pushConstantRangeCount = resources->push_constant_count;
        pipeline_layout_info.pPushConstantRanges = resources->push_constants;

        res = VULKAN_HPP_DEFAULT_DISPATCHER.vkCreatePipelineLayout(dev->device, &pipeline_layout_info, nullptr,
                                                                   &layout->pipeline_layout);
        vk_error_sub_set_vkresult(&retval, res);
        if (res) {
            free(set_layout_bindings);
            set_layout_bindings = nullptr;
            continue;
        }

        free(set_layout_bindings);
        set_layout_bindings = nullptr;
        ++successful;
    }

    vk_error_set_vkresult(&retval, successful == layout_count ? VK_SUCCESS : VK_INCOMPLETE);
    return retval;
}

vk_error
VulkanUtils::make_graphics_pipelines(struct vk_device *dev, struct vk_pipeline *pipelines, uint32_t pipeline_count,
                                     bool is_blend) {
    uint32_t successful = 0;
    vk_error retval = VK_ERROR_NONE;
    VkResult res;

    for (uint32_t i = 0; i < pipeline_count; ++i) {
        struct vk_pipeline *pipeline = &pipelines[i];
        struct vk_layout *layout = pipeline->layout;
        struct vk_resources *resources = layout->resources;

        pipeline->pipeline = nullptr;
        pipeline->set_pool = nullptr;

        VkGraphicsPipelineCreateInfo pipeline_info{};

        bool has_tessellation_shader = false;
        VkPipelineShaderStageCreateInfo *stage_info;
        stage_info = (VkPipelineShaderStageCreateInfo *) malloc(
                resources->shader_count * sizeof(VkPipelineShaderStageCreateInfo));
        for (uint32_t j = 0; j < resources->shader_count; ++j) {
            struct vk_shader *shader = &resources->shaders[j];

            stage_info[j] = {};
            stage_info[j].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            stage_info[j].stage = shader->stage;
            stage_info[j].module = shader->shader;
            stage_info[j].pName = "main";

            if (shader->stage == VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT ||
                shader->stage == VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)
                has_tessellation_shader = true;
        }

        VkPipelineViewportStateCreateInfo viewport_state{};
        viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewport_state.viewportCount = 1;
        viewport_state.scissorCount = 1;

        VkPipelineRasterizationStateCreateInfo rasterization_state{};
        rasterization_state.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterization_state.polygonMode = VK_POLYGON_MODE_FILL;
        rasterization_state.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterization_state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterization_state.lineWidth = 1;

        VkPipelineMultisampleStateCreateInfo multisample_state{};
        multisample_state.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisample_state.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineDepthStencilStateCreateInfo depth_stencil_state{};
        depth_stencil_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depth_stencil_state.depthTestEnable = true;
        depth_stencil_state.depthWriteEnable = true;
        depth_stencil_state.depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL;

        VkPipelineColorBlendAttachmentState color_blend_attachments[1] = {};
        color_blend_attachments[0].blendEnable = is_blend;
        color_blend_attachments[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT
                                                    | VK_COLOR_COMPONENT_G_BIT
                                                    | VK_COLOR_COMPONENT_B_BIT
                                                    | VK_COLOR_COMPONENT_A_BIT;
        if (is_blend) {
            color_blend_attachments[0].srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
            color_blend_attachments[0].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            color_blend_attachments[0].colorBlendOp = VK_BLEND_OP_ADD;
            color_blend_attachments[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            color_blend_attachments[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
            color_blend_attachments[0].alphaBlendOp = VK_BLEND_OP_ADD;
        }

        VkPipelineColorBlendStateCreateInfo color_blend_state{};
        color_blend_state.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        color_blend_state.attachmentCount = 1;
        color_blend_state.pAttachments = color_blend_attachments;

        VkDynamicState dynamic_states[2] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

        VkPipelineDynamicStateCreateInfo dynamic_state{};
        dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamic_state.dynamicStateCount = 2;
        dynamic_state.pDynamicStates = dynamic_states;

        pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipeline_info.flags = VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT;
        pipeline_info.stageCount = resources->shader_count;
        pipeline_info.pStages = stage_info;
        pipeline_info.pVertexInputState = &pipeline->vertex_input_state;
        pipeline_info.pInputAssemblyState = &pipeline->input_assembly_state;
        pipeline_info.pTessellationState = has_tessellation_shader ? &pipeline->tessellation_state : nullptr;
        pipeline_info.pViewportState = &viewport_state;
        pipeline_info.pRasterizationState = &rasterization_state;
        pipeline_info.pMultisampleState = &multisample_state;
        pipeline_info.pDepthStencilState = &depth_stencil_state;
        pipeline_info.pColorBlendState = &color_blend_state;
        pipeline_info.pDynamicState = &dynamic_state;
        pipeline_info.layout = layout->pipeline_layout;
        pipeline_info.renderPass = resources->render_pass;
        pipeline_info.subpass = 0;
        pipeline_info.basePipelineIndex = 0;

        res = VULKAN_HPP_DEFAULT_DISPATCHER.vkCreateGraphicsPipelines(dev->device, nullptr, 1, &pipeline_info, nullptr,
                                                                      &pipeline->pipeline);
        vk_error_sub_set_vkresult(&retval, res);
        if (res) {
            free(stage_info);
            stage_info = nullptr;
            continue;
        }

        uint32_t image_sampler_count = 0;
        uint32_t storage_image_count = 0;
        uint32_t uniform_buffer_count = 0;
        uint32_t storage_buffer_count = 0;

        for (uint32_t j = 0; j < resources->image_count; ++j) {
            if ((resources->images[j].usage & VK_IMAGE_USAGE_SAMPLED_BIT))
                ++image_sampler_count;
            else if ((resources->images[j].usage & VK_IMAGE_USAGE_SAMPLED_BIT))
                ++storage_image_count;
        }

        for (uint32_t j = 0; j < resources->buffer_count; ++j) {
            if ((resources->buffers[j].usage & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT))
                ++uniform_buffer_count;
            else if ((resources->buffers[j].usage & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT))
                ++storage_buffer_count;
        }

        uint32_t pool_size_count = 0;
        VkDescriptorPoolSize pool_sizes[4]{};
        if (image_sampler_count > 0)
            pool_sizes[pool_size_count++] = (VkDescriptorPoolSize) {
                    .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .descriptorCount = pipeline->thread_count * image_sampler_count,
            };

        if (storage_image_count > 0)
            pool_sizes[pool_size_count++] = (VkDescriptorPoolSize) {
                    .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                    .descriptorCount = pipeline->thread_count * storage_image_count,
            };

        if (uniform_buffer_count > 0)
            pool_sizes[pool_size_count++] = (VkDescriptorPoolSize) {
                    .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                    .descriptorCount = pipeline->thread_count * uniform_buffer_count,
            };

        if (storage_buffer_count > 0)
            pool_sizes[pool_size_count++] = (VkDescriptorPoolSize) {
                    .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                    .descriptorCount = pipeline->thread_count * storage_image_count,
            };

        VkDescriptorPoolCreateInfo set_info{};
        set_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        set_info.maxSets = pipeline->thread_count;
        set_info.poolSizeCount = pool_size_count;
        set_info.pPoolSizes = pool_sizes;

        res = VULKAN_HPP_DEFAULT_DISPATCHER.vkCreateDescriptorPool(dev->device, &set_info, nullptr,
                                                                   &pipeline->set_pool);
        vk_error_sub_set_vkresult(&retval, res);
        if (res) {
            free(stage_info);
            stage_info = nullptr;
            continue;
        }

        free(stage_info);
        stage_info = nullptr;
        ++successful;
    }

    vk_error_set_vkresult(&retval, successful == pipeline_count ? VK_SUCCESS : VK_INCOMPLETE);
    return retval;
}

void VulkanUtils::free_layouts(struct vk_device *dev, struct vk_layout *layouts, uint32_t layout_count) {
    VULKAN_HPP_DEFAULT_DISPATCHER.vkDeviceWaitIdle(dev->device);

    for (uint32_t i = 0; i < layout_count; ++i) {
        VULKAN_HPP_DEFAULT_DISPATCHER.vkDestroyPipelineLayout(dev->device, layouts[i].pipeline_layout, nullptr);
        VULKAN_HPP_DEFAULT_DISPATCHER.vkDestroyDescriptorSetLayout(dev->device, layouts[i].set_layout, nullptr);
    }
}

void VulkanUtils::free_pipelines(struct vk_device *dev, struct vk_pipeline *pipelines, uint32_t pipeline_count) {
    VULKAN_HPP_DEFAULT_DISPATCHER.vkDeviceWaitIdle(dev->device);

    for (uint32_t i = 0; i < pipeline_count; ++i) {
        VULKAN_HPP_DEFAULT_DISPATCHER.vkDestroyPipeline(dev->device, pipelines[i].pipeline, nullptr);
        VULKAN_HPP_DEFAULT_DISPATCHER.vkDestroyDescriptorPool(dev->device, pipelines[i].set_pool, nullptr);
    }
}


static vk_error
create_render_pass(struct vk_device *dev, VkFormat color_format, VkFormat depth_format, VkRenderPass *render_pass,
                   enum vk_render_pass_load_op keeps_contents, enum vk_make_depth_buffer has_depth) {
    vk_error retval = VK_ERROR_NONE;
    VkResult res;

    VkAttachmentDescription render_pass_attachments[2]{};
    render_pass_attachments[0].format = color_format;
    render_pass_attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    render_pass_attachments[0].loadOp = keeps_contents ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_CLEAR;
    render_pass_attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    render_pass_attachments[0].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    render_pass_attachments[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    render_pass_attachments[1].format = depth_format;
    render_pass_attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
    //render_pass_attachments[1].loadOp = keeps_contents?VK_ATTACHMENT_LOAD_OP_LOAD:VK_ATTACHMENT_LOAD_OP_CLEAR;
    render_pass_attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    render_pass_attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    render_pass_attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    render_pass_attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    render_pass_attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    render_pass_attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference render_pass_attachment_references[2]{};
    render_pass_attachment_references[0].attachment = 0;
    render_pass_attachment_references[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    render_pass_attachment_references[1].attachment = 1;
    render_pass_attachment_references[1].layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription render_pass_subpasses[1]{};
    render_pass_subpasses[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    render_pass_subpasses[0].colorAttachmentCount = 1;
    render_pass_subpasses[0].pColorAttachments = &render_pass_attachment_references[0];
    render_pass_subpasses[0].pDepthStencilAttachment = has_depth ? &render_pass_attachment_references[1] : nullptr;

    VkRenderPassCreateInfo render_pass_info{};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_info.attachmentCount = has_depth ? UINT32_C(2) : UINT32_C(1);
    render_pass_info.pAttachments = render_pass_attachments;
    render_pass_info.subpassCount = UINT32_C(1);
    render_pass_info.pSubpasses = render_pass_subpasses;

    res = VULKAN_HPP_DEFAULT_DISPATCHER.vkCreateRenderPass(dev->device, &render_pass_info, nullptr, render_pass);
    vk_error_set_vkresult(&retval, res);

    return retval;
}

vk_error
VulkanUtils::create_offscreen_buffers(struct vk_physical_device *phy_dev, struct vk_device *dev, VkFormat format,
                                      struct vk_offscreen_buffers *offscreen_buffers, uint32_t offscreen_buffer_count,
                                      VkRenderPass *render_pass,
                                      enum vk_render_pass_load_op keeps_contents, enum vk_make_depth_buffer has_depth,
                                      bool linear) {
    uint32_t successful = 0;
    vk_error retval = VK_ERROR_NONE;
    VkResult res;
    vk_error err;

    VkImageFormatProperties format_properties;
    res = VULKAN_HPP_DEFAULT_DISPATCHER.vkGetPhysicalDeviceImageFormatProperties(phy_dev->physical_device, format,
                                                                                 VK_IMAGE_TYPE_2D,
                                                                                 VK_IMAGE_TILING_OPTIMAL,
                                                                                 VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                                                                 VK_IMAGE_USAGE_SAMPLED_BIT,
                                                                                 0,
                                                                                 &format_properties);
    vk_error_sub_set_vkresult(&retval, res);
    if (res != VK_SUCCESS)return retval;

    for (uint32_t i = 0; i < offscreen_buffer_count; ++i) {
        offscreen_buffers[i].color = {};
        offscreen_buffers[i].depth = {};
        offscreen_buffers[i].framebuffer = nullptr;
        if (format_properties.maxExtent.width < offscreen_buffers[i].surface_size.width) {
            offscreen_buffers[i].surface_size.width = format_properties.maxExtent.width;
        }
        if (format_properties.maxExtent.height < offscreen_buffers[i].surface_size.height) {
            offscreen_buffers[i].surface_size.height = format_properties.maxExtent.height;
        }
    }

    VkFormat depth_format = get_supported_depth_stencil_format(phy_dev);

    retval = create_render_pass(dev, format, depth_format, render_pass, keeps_contents, has_depth);
    if (!vk_error_is_success(&retval))
        return retval;

    for (uint32_t i = 0; i < offscreen_buffer_count; ++i) {
        offscreen_buffers[i].color = {};
        offscreen_buffers[i].color.format = format;
        offscreen_buffers[i].color.extent = offscreen_buffers[i].surface_size;
        offscreen_buffers[i].color.usage = (VkImageUsageFlagBits) (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                                                   VK_IMAGE_USAGE_SAMPLED_BIT);
        offscreen_buffers[i].color.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        offscreen_buffers[i].color.make_view = true;
        offscreen_buffers[i].color.anisotropyEnable = true;
        offscreen_buffers[i].color.repeat_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        offscreen_buffers[i].color.mipmaps = false;
        offscreen_buffers[i].color.linear = linear;

        err = create_images(phy_dev, dev, &offscreen_buffers[i].color, 1);
        vk_error_sub_merge(&retval, &err);
        if (!vk_error_is_success(&err))
            continue;

        if (has_depth) {
            offscreen_buffers[i].depth = {};
            offscreen_buffers[i].depth.format = depth_format;
            offscreen_buffers[i].depth.extent = offscreen_buffers[i].surface_size;
            offscreen_buffers[i].depth.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
            offscreen_buffers[i].depth.make_view = true;
            offscreen_buffers[i].depth.anisotropyEnable = true;
            offscreen_buffers[i].depth.repeat_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            offscreen_buffers[i].depth.mipmaps = false;
            offscreen_buffers[i].depth.linear = false;

            err = create_images(phy_dev, dev, &offscreen_buffers[i].depth, 1);
            vk_error_sub_merge(&retval, &err);
            if (!vk_error_is_success(&err))
                continue;
        }

        VkImageView framebuffer_attachments[2] = {
                offscreen_buffers[i].color.view,
                offscreen_buffers[i].depth.view,
        };

        VkFramebufferCreateInfo framebuffer_info{};
        framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebuffer_info.renderPass = *render_pass;
        framebuffer_info.attachmentCount = has_depth ? UINT32_C(2) : UINT32_C(1);
        framebuffer_info.pAttachments = framebuffer_attachments;
        framebuffer_info.width = offscreen_buffers[i].surface_size.width;
        framebuffer_info.height = offscreen_buffers[i].surface_size.height;
        framebuffer_info.layers = 1;

        res = VULKAN_HPP_DEFAULT_DISPATCHER.vkCreateFramebuffer(dev->device, &framebuffer_info, nullptr,
                                                                &offscreen_buffers[i].framebuffer);
        vk_error_sub_set_vkresult(&retval, res);
        if (res)
            continue;

        ++successful;
    }

    vk_error_set_vkresult(&retval, successful == offscreen_buffer_count ? VK_SUCCESS : VK_INCOMPLETE);
    return retval;
}

vk_error
VulkanUtils::create_graphics_buffers(struct vk_physical_device *phy_dev, struct vk_device *dev, VkFormat format,
                                     struct vk_graphics_buffers *graphics_buffers, uint32_t graphics_buffer_count,
                                     VkRenderPass *render_pass,
                                     enum vk_render_pass_load_op keeps_contents, enum vk_make_depth_buffer has_depth) {
    uint32_t successful = 0;
    vk_error retval = VK_ERROR_NONE;
    VkResult res;
    vk_error err;

    VkImageFormatProperties format_properties;
    res = VULKAN_HPP_DEFAULT_DISPATCHER.vkGetPhysicalDeviceImageFormatProperties(phy_dev->physical_device, format,
                                                                                 VK_IMAGE_TYPE_2D,
                                                                                 VK_IMAGE_TILING_OPTIMAL,
                                                                                 VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                                                                 VK_IMAGE_USAGE_SAMPLED_BIT,
                                                                                 0,
                                                                                 &format_properties);
    vk_error_sub_set_vkresult(&retval, res);
    if (res != VK_SUCCESS)return retval;

    for (uint32_t i = 0; i < graphics_buffer_count; ++i) {
        graphics_buffers[i].color_view = nullptr;
        graphics_buffers[i].depth = (struct vk_image) {};
        graphics_buffers[i].framebuffer = nullptr;
        if (format_properties.maxExtent.width < graphics_buffers[i].surface_size.width) {
            graphics_buffers[i].surface_size.width = format_properties.maxExtent.width;
        }
        if (format_properties.maxExtent.height < graphics_buffers[i].surface_size.height) {
            graphics_buffers[i].surface_size.height = format_properties.maxExtent.height;
        }
    }

    VkFormat depth_format = get_supported_depth_stencil_format(phy_dev);;

    retval = create_render_pass(dev, format, depth_format, render_pass, keeps_contents, has_depth);
    if (!vk_error_is_success(&retval))
        return retval;

    for (uint32_t i = 0; i < graphics_buffer_count; ++i) {
        VkImageViewCreateInfo view_info{};
        view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.image = graphics_buffers[i].swapchain_image;
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = format;
        view_info.components = {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B,
                                VK_COMPONENT_SWIZZLE_A,};
        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view_info.subresourceRange.baseMipLevel = 0;
        view_info.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

        res = VULKAN_HPP_DEFAULT_DISPATCHER.vkCreateImageView(dev->device, &view_info, nullptr,
                                                              &graphics_buffers[i].color_view);
        vk_error_sub_set_vkresult(&retval, res);
        if (res)
            continue;

        if (has_depth) {
            graphics_buffers[i].depth = {};
            graphics_buffers[i].depth.format = depth_format;
            graphics_buffers[i].depth.extent = graphics_buffers[i].surface_size;
            graphics_buffers[i].depth.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
            graphics_buffers[i].depth.make_view = true;
            graphics_buffers[i].depth.will_be_initialized = false;
            graphics_buffers[i].depth.multisample = false;
            graphics_buffers[i].depth.anisotropyEnable = true;
            graphics_buffers[i].depth.repeat_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            graphics_buffers[i].depth.mipmaps = false;
            graphics_buffers[i].depth.linear = false;

            err = create_images(phy_dev, dev, &graphics_buffers[i].depth, 1);
            vk_error_sub_merge(&retval, &err);
            if (!vk_error_is_success(&err))
                continue;
        }

        //TODO suspicious
        VkImageView framebuffer_attachments[2] = {
                graphics_buffers[i].color_view,
                graphics_buffers[i].depth.view,
        };

        VkFramebufferCreateInfo framebuffer_info{};
        framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebuffer_info.renderPass = *render_pass;
        framebuffer_info.attachmentCount = has_depth ? UINT32_C(2) : UINT32_C(1);
        framebuffer_info.pAttachments = framebuffer_attachments;
        framebuffer_info.width = graphics_buffers[i].surface_size.width;
        framebuffer_info.height = graphics_buffers[i].surface_size.height;
        framebuffer_info.layers = 1;

        res = VULKAN_HPP_DEFAULT_DISPATCHER.vkCreateFramebuffer(dev->device, &framebuffer_info, nullptr,
                                                                &graphics_buffers[i].framebuffer);
        vk_error_sub_set_vkresult(&retval, res);
        if (res)
            continue;

        ++successful;
    }

    vk_error_set_vkresult(&retval, successful == graphics_buffer_count ? VK_SUCCESS : VK_INCOMPLETE);
    return retval;
}

void VulkanUtils::free_offscreen_buffers(struct vk_device *dev, struct vk_offscreen_buffers *offscreen_buffers,
                                         uint32_t graphics_buffer_count,
                                         VkRenderPass render_pass) {
    VULKAN_HPP_DEFAULT_DISPATCHER.vkDeviceWaitIdle(dev->device);

    for (uint32_t i = 0; i < graphics_buffer_count; ++i) {
        free_images(dev, &offscreen_buffers[i].color, 1);
        free_images(dev, &offscreen_buffers[i].depth, 1);

        VULKAN_HPP_DEFAULT_DISPATCHER.vkDestroyFramebuffer(dev->device, offscreen_buffers[i].framebuffer, nullptr);
    }

    VULKAN_HPP_DEFAULT_DISPATCHER.vkDestroyRenderPass(dev->device, render_pass, nullptr);
}

void VulkanUtils::get_local_time(struct my_time_struct *my_time) {
    struct timeval te{};
    gettimeofday(&te, nullptr);
    time_t T = time(nullptr);
    struct tm tm = *localtime(&T);
    long long milliseconds = te.tv_sec * 1000LL + te.tv_usec / 1000;
    my_time->msec = (int) (milliseconds % (1000));
    my_time->sec = tm.tm_sec;
    my_time->min = tm.tm_min;
    my_time->hour = tm.tm_hour;
    my_time->day = tm.tm_mday;
    my_time->month = tm.tm_mon + 1;
    my_time->year = tm.tm_year + 1900;
}

double VulkanUtils::get_time_ticks() {
    struct timespec ts{};
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    double ticks = (static_cast<double>(ts.tv_sec * 1000)) + (static_cast<double>(ts.tv_nsec)) / 1000000.0;
    return ticks;
}

//return total time of pause pressed
float VulkanUtils::pres_pause(bool pause) {
    static bool lpause = false;
    static float pause_time = 0;
    static double before_time = 0;
    double now_time = get_time_ticks();
    if (pause && (!lpause)) {
        lpause = true;
        before_time = now_time;
    } else {
        if (!pause && (lpause)) {
            lpause = false;
            pause_time += static_cast<float>((now_time - before_time) / 1000.f);
        }
    }
    return pause_time;
}

float VulkanUtils::update_fps_delta() {
    static double before_time = 0;
    if ((int) before_time == 0)before_time = get_time_ticks();

    double now_time = get_time_ticks();

    if ((long) before_time == (long) now_time) { //this happend on IMMEDIATE and MAILBOX
        return 1.0f / 9999.0f;
    }
    double tdelta = (now_time - before_time);
    if (tdelta <= 0)tdelta = 1;
    float delta = (float) tdelta / 1000.0f;
    if (delta > 1) {
        delta = 1;
    }
    if (delta <= 0) {
        delta = 1.0f / 9999.0f;
    }
    before_time = now_time;
    return delta;
}

// cross-platform sleep function
void VulkanUtils::sleep_ms(int milliseconds) {
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

void VulkanUtils::FPS_LOCK(int fps) {
    static double before_time = 0;
    if (before_time == 0)before_time = get_time_ticks();
    double now_time = get_time_ticks();
    if (before_time == now_time)return;
    if (fps <= 0)fps = 1;
    double wdelta = ((double) (1.0 / (double) fps) * (double) 1000.0);
    double tdelta = (now_time - before_time);
    if (tdelta <= 0)tdelta = 1;
    double rdelta = wdelta - tdelta;
    if (rdelta <= 0) {
        before_time = now_time;
        return;
    }
    sleep_ms((int) rdelta);
    before_time = get_time_ticks();
}