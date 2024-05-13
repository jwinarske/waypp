
#ifndef _LAUNCHER_VULKAN_UTILS_H_
#define _LAUNCHER_VULKAN_UTILS_H_

#include "common.h"

class VulkanUtils {
public:
    struct my_time_struct {
        int msec;
        int sec;
        int min;
        int hour;
        int day;
        int month;
        int year;
    };

    VulkanUtils();

    ~VulkanUtils();

    static void exit(VkInstance vk);

    static vk_error
    enumerate_devices(VkInstance vk, VkSurfaceKHR *surface, struct vk_physical_device *devs, uint32_t *idx,
                      bool use_idx);

    static vk_error
    get_commands(struct vk_physical_device *phy_dev, struct vk_device *dev, VkDeviceQueueCreateInfo queue_info[],
                 uint32_t queue_info_count, uint32_t create_count);

    static void cleanup(struct vk_device *dev);

    static vk_error load_shader(struct vk_device *dev, const uint32_t *code, VkShaderModule *shader, size_t size);

    static vk_error load_shader_spirv_file(struct vk_device *dev, const char *spirv_file, VkShaderModule *shader);

    static void free_shader(struct vk_device *dev, VkShaderModule shader);

    static uint32_t find_suitable_memory(struct vk_physical_device *phy_dev, struct vk_device *dev,
                                         VkMemoryRequirements *mem_req, VkMemoryPropertyFlags properties);

    static vk_error init_ext(VkInstance *vk, const char *ext_names[], uint32_t ext_count);

    static vk_error get_dev_ext(struct vk_physical_device *phy_dev, struct vk_device *dev, VkQueueFlags qflags,
                                VkDeviceQueueCreateInfo queue_info[], uint32_t *queue_info_count,
                                const char *ext_names[], uint32_t ext_count);

    static inline vk_error init(VkInstance *vk) {
        const char *extension_names[] = {
                VK_KHR_SURFACE_EXTENSION_NAME,
#if defined(VK_USE_PLATFORM_WIN32_KHR)
                VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#elif defined(VK_USE_PLATFORM_XCB_KHR)
                VK_KHR_XCB_SURFACE_EXTENSION_NAME,
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
                VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME,
#endif
        };
        VULKAN_HPP_DEFAULT_DISPATCHER.init();
        return init_ext(vk, extension_names, sizeof extension_names / sizeof *extension_names);
    }

    static inline vk_error get_dev(struct vk_physical_device *phy_dev, struct vk_device *dev, VkQueueFlags qflags,
                                   VkDeviceQueueCreateInfo queue_info[], uint32_t *queue_info_count) {
        const char *extension_names[] = {
                VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        };
        return get_dev_ext(phy_dev, dev, qflags, queue_info, queue_info_count, extension_names,
                           sizeof extension_names / sizeof *extension_names);
    }

// gcc 11 has Wstringop-overflow warning here, but this is GCC bug look like
// look https://stackoverflow.com/questions/69426070/gcc-11-order-of-arguments-triggers-false-positive-wstringop-overflow-is-this-bu
    static vk_error
    setup(struct vk_physical_device *phy_dev, struct vk_device *dev, VkQueueFlags qflags, uint32_t create_count) {
        VkDeviceQueueCreateInfo queue_info[kMaxQueueFamily];
        uint32_t queue_info_count = 0;

        queue_info_count = phy_dev->queue_family_count;
        vk_error res = get_dev(phy_dev, dev, qflags, queue_info, &queue_info_count);
        if (vk_error_is_success(&res)) {
            if (create_count <= queue_info[0].queueCount)create_count = 0; //0=create one cmd_buffer per Queue
            res = get_commands(phy_dev, dev, queue_info, queue_info_count, create_count);
        }
        return res;

    }

    static vk_error create_surface(VkInstance vk, VkSurfaceKHR *surface, struct app_os_window *os_window);

    static vk_error get_swapchain(VkInstance vk, struct vk_physical_device *phy_dev, struct vk_device *dev,
                                  struct vk_swapchain *swapchain, struct app_os_window *os_window,
                                  uint32_t thread_count,
                                  VkPresentModeKHR *present_mode);

    static void free_swapchain(VkInstance vk, struct vk_device *dev, struct vk_swapchain *swapchain);

    static VkImage *get_swapchain_images(struct vk_device *dev, struct vk_swapchain *swapchain, uint32_t *count);

    static vk_error create_images(struct vk_physical_device *phy_dev, struct vk_device *dev,
                                  struct vk_image *images, uint32_t image_count);

    static vk_error create_buffers(struct vk_physical_device *phy_dev, struct vk_device *dev,
                                   struct vk_buffer *buffers, uint32_t buffer_count);

    static vk_error load_shaders(struct vk_device *dev,
                                 struct vk_shader *shaders, uint32_t shader_count);

    static vk_error get_presentable_queues(struct vk_physical_device *phy_dev, struct vk_device *dev,
                                           VkSurfaceKHR surface, uint32_t **presentable_queues,
                                           uint32_t *presentable_queue_count);

    static VkFormat get_supported_depth_stencil_format(struct vk_physical_device *phy_dev);

    static void free_images(struct vk_device *dev, struct vk_image *images, uint32_t image_count);

    static void free_buffers(struct vk_device *dev, struct vk_buffer *buffers, uint32_t buffer_count);

    static void free_shaders(struct vk_device *dev, struct vk_shader *shaders, uint32_t shader_count);

    static void free_graphics_buffers(struct vk_device *dev, struct vk_graphics_buffers *graphics_buffers,
                                      uint32_t graphics_buffer_count,
                                      VkRenderPass render_pass);

    static vk_error
    make_graphics_layouts(struct vk_device *dev, struct vk_layout *layouts, uint32_t layout_count, bool w_img_pattern,
                          uint32_t *img_pattern, uint32_t img_pattern_size);

    static vk_error
    make_graphics_pipelines(struct vk_device *dev, struct vk_pipeline *pipelines, uint32_t pipeline_count,
                            bool is_blend);

    static void free_layouts(struct vk_device *dev, struct vk_layout *layouts, uint32_t layout_count);

    static void free_pipelines(struct vk_device *dev, struct vk_pipeline *pipelines, uint32_t pipeline_count);

    static vk_error create_offscreen_buffers(struct vk_physical_device *phy_dev, struct vk_device *dev, VkFormat format,
                                             struct vk_offscreen_buffers *offscreen_buffers,
                                             uint32_t offscreen_buffer_count,
                                             VkRenderPass *render_pass,
                                             enum vk_render_pass_load_op keeps_contents,
                                             enum vk_make_depth_buffer has_depth,
                                             bool linear);

    static vk_error create_graphics_buffers(struct vk_physical_device *phy_dev, struct vk_device *dev, VkFormat format,
                                            struct vk_graphics_buffers *graphics_buffers,
                                            uint32_t graphics_buffer_count,
                                            VkRenderPass *render_pass,
                                            enum vk_render_pass_load_op keeps_contents,
                                            enum vk_make_depth_buffer has_depth);

    static void free_offscreen_buffers(struct vk_device *dev, struct vk_offscreen_buffers *offscreen_buffers,
                                       uint32_t offscreen_buffer_count,
                                       VkRenderPass render_pass);

    static void get_local_time(struct my_time_struct *my_time);

    static double get_time_ticks();

    static float pres_pause(bool pause);

    static float update_fps_delta();

    static void sleep_ms(int milliseconds);

    static void FPS_LOCK(int fps);
};

#endif //_LAUNCHER_VULKAN_UTILS_H_