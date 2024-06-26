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

// Danil, 2021+ Vulkan shader launcher, self
// https://github.com/danilw/vulkan-shadertoy-launcher The MIT License

#ifndef EXAMPLES_VK_SHADERTOY_VULKAN_UTILS_H_
#define EXAMPLES_VK_SHADERTOY_VULKAN_UTILS_H_

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

  static vk_error enumerate_devices(VkInstance vk,
                                    VkSurfaceKHR* surface,
                                    vk_physical_device* devs,
                                    uint32_t* idx,
                                    bool use_idx);

  static vk_error get_commands(vk_physical_device* phy_dev,
                               vk_device* dev,
                               VkDeviceQueueCreateInfo queue_info[],
                               uint32_t queue_info_count,
                               uint32_t create_count);

  static void cleanup(vk_device* dev);

  static vk_error load_shader(vk_device* dev,
                              const uint32_t* code,
                              VkShaderModule* shader,
                              size_t size);

  static vk_error load_shader_spirv_file(vk_device* dev,
                                         const char* spirv_file,
                                         VkShaderModule* shader);

  static void free_shader(vk_device* dev, VkShaderModule shader);

  static uint32_t find_suitable_memory(vk_physical_device* phy_dev,
                                       vk_device* dev,
                                       VkMemoryRequirements* mem_req,
                                       VkMemoryPropertyFlags properties);

  static vk_error init_ext(VkInstance* vk,
                           const char* ext_names[],
                           uint32_t ext_count);

  static vk_error get_dev_ext(vk_physical_device* phy_dev,
                              vk_device* dev,
                              VkQueueFlags qflags,
                              VkDeviceQueueCreateInfo queue_info[],
                              uint32_t* queue_info_count,
                              const char* ext_names[],
                              uint32_t ext_count);

  static vk_error init(VkInstance* vk, VkDevice const* device = nullptr) {
    const char* extension_names[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME,
    };
    VULKAN_HPP_DEFAULT_DISPATCHER.init(*vk, *device);
    return init_ext(vk, extension_names,
                    sizeof extension_names / sizeof *extension_names);
  }

  static vk_error get_dev(vk_physical_device* phy_dev,
                          vk_device* dev,
                          VkQueueFlags qflags,
                          VkDeviceQueueCreateInfo queue_info[],
                          uint32_t* queue_info_count) {
    const char* extension_names[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };
    return get_dev_ext(phy_dev, dev, qflags, queue_info, queue_info_count,
                       extension_names,
                       sizeof extension_names / sizeof *extension_names);
  }

  static vk_error setup(vk_physical_device* phy_dev,
                        vk_device* dev,
                        const VkQueueFlags qflags,
                        uint32_t create_count) {
    VkDeviceQueueCreateInfo queue_info[kMaxQueueFamily];
    uint32_t queue_info_count = 0;

    queue_info_count = phy_dev->queue_family_count;
    vk_error res = get_dev(phy_dev, dev, qflags, queue_info, &queue_info_count);
    if (vk_error_is_success(&res)) {
      if (create_count <= queue_info[0].queueCount)
        create_count = 0;  // 0=create one cmd_buffer per Queue
      res = get_commands(phy_dev, dev, queue_info, queue_info_count,
                         create_count);
    }
    return res;
  }

  static vk_error create_surface(VkInstance vk,
                                 VkSurfaceKHR* surface,
                                 app_os_window* os_window);

  static vk_error get_swapchain(VkInstance vk,
                                vk_physical_device* phy_dev,
                                vk_device* dev,
                                vk_swapchain* swapchain,
                                app_os_window* os_window,
                                uint32_t thread_count,
                                VkPresentModeKHR* present_mode);

  static void free_swapchain(VkInstance vk,
                             vk_device* dev,
                             vk_swapchain* swapchain);

  static VkImage* get_swapchain_images(vk_device* dev,
                                       vk_swapchain* swapchain,
                                       uint32_t* count);

  static vk_error create_images(vk_physical_device* phy_dev,
                                vk_device* dev,
                                vk_image* images,
                                uint32_t image_count);

  static vk_error create_buffers(vk_physical_device* phy_dev,
                                 vk_device* dev,
                                 vk_buffer* buffers,
                                 uint32_t buffer_count);

  static vk_error load_shaders(vk_device* dev,
                               vk_shader* shaders,
                               uint32_t shader_count);

  static vk_error get_presentable_queues(vk_physical_device* phy_dev,
                                         vk_device* dev,
                                         VkSurfaceKHR surface,
                                         uint32_t** presentable_queues,
                                         uint32_t* presentable_queue_count);

  static VkFormat get_supported_depth_stencil_format(
      vk_physical_device* phy_dev);

  static void free_images(vk_device* dev,
                          vk_image* images,
                          uint32_t image_count);

  static void free_buffers(vk_device* dev,
                           vk_buffer* buffers,
                           uint32_t buffer_count);

  static void free_shaders(vk_device* dev,
                           vk_shader* shaders,
                           uint32_t shader_count);

  static void free_graphics_buffers(vk_device* dev,
                                    vk_graphics_buffers* graphics_buffers,
                                    uint32_t graphics_buffer_count,
                                    VkRenderPass render_pass);

  static vk_error make_graphics_layouts(vk_device* dev,
                                        vk_layout* layouts,
                                        uint32_t layout_count,
                                        bool w_img_pattern,
                                        const uint32_t* img_pattern,
                                        uint32_t img_pattern_size);

  static vk_error make_graphics_pipelines(vk_device* dev,
                                          vk_pipeline* pipelines,
                                          uint32_t pipeline_count,
                                          bool is_blend);

  static void free_layouts(vk_device* dev,
                           vk_layout* layouts,
                           uint32_t layout_count);

  static void free_pipelines(vk_device* dev,
                             vk_pipeline* pipelines,
                             uint32_t pipeline_count);

  static vk_error create_offscreen_buffers(
      vk_physical_device* phy_dev,
      vk_device* dev,
      VkFormat format,
      vk_offscreen_buffers* offscreen_buffers,
      uint32_t offscreen_buffer_count,
      VkRenderPass* render_pass,
      vk_render_pass_load_op keeps_contents,
      vk_make_depth_buffer has_depth,
      bool linear);

  static vk_error create_graphics_buffers(vk_physical_device* phy_dev,
                                          vk_device* dev,
                                          VkFormat format,
                                          vk_graphics_buffers* graphics_buffers,
                                          uint32_t graphics_buffer_count,
                                          VkRenderPass* render_pass,
                                          vk_render_pass_load_op keeps_contents,
                                          vk_make_depth_buffer has_depth);

  static void free_offscreen_buffers(vk_device* dev,
                                     vk_offscreen_buffers* offscreen_buffers,
                                     uint32_t offscreen_buffer_count,
                                     VkRenderPass render_pass);

  static void get_local_time(my_time_struct* my_time);

  static double get_time_ticks();

  static float pres_pause(bool pause);

  static float update_fps_delta();

  static void sleep_ms(int milliseconds);

  static void FPS_LOCK(int fps);
};

#endif  // EXAMPLES_VK_SHADERTOY_VULKAN_UTILS_H_