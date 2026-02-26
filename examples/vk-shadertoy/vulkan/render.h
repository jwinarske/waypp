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

#ifndef EXAMPLES_VK_SHADERTOY_VULKAN_RENDER_H_
#define EXAMPLES_VK_SHADERTOY_VULKAN_RENDER_H_

#include "common.h"

#include "vk_error_print.h"

class VulkanUtils;

class VulkanRender {
 public:
  VulkanRender();

  ~VulkanRender() = default;

  static int get_essentials(vk_render_essentials* essentials,
                            vk_physical_device* phy_dev,
                            vk_device* dev,
                            vk_swapchain* swapchain);

  static void cleanup_essentials(vk_render_essentials const* essentials,
                                 vk_device const* dev);

  static VkResult start(vk_render_essentials* essentials,
                        vk_device* dev,
                        vk_swapchain* swapchain,
                        VkImageLayout to_layout,
                        uint32_t* image_index);

  static vk_error fill_object(vk_device* dev,
                              VkDeviceMemory to,
                              void* from,
                              size_t size,
                              const char* object,
                              const char* name);

  static vk_error fill_buffer(vk_device* dev,
                              vk_buffer* to,
                              void* from,
                              size_t size,
                              const char* name);

  static vk_error fill_image(vk_device* dev,
                             vk_image* to,
                             void* from,
                             size_t size,
                             const char* name);

  static vk_error copy_object_start(vk_device* /* dev */,
                                    vk_render_essentials* essentials,
                                    const char* object,
                                    const char* name);

  static vk_error copy_object_end(const vk_device* dev,
                                  vk_render_essentials* essentials);

  static vk_error copy_buffer(vk_device* dev,
                              vk_render_essentials* essentials,
                              const vk_buffer* to,
                              vk_buffer* from,
                              size_t size,
                              const char* name);

  static vk_error copy_image(vk_device* dev,
                             vk_render_essentials* essentials,
                             vk_image* to,
                             VkImageLayout to_layout,
                             vk_image* from,
                             VkImageLayout from_layout,
                             VkImageCopy* region,
                             const char* name);

  static vk_error copy_buffer_to_image(vk_device* dev,
                                       vk_render_essentials* essentials,
                                       vk_image* to,
                                       VkImageLayout to_layout,
                                       vk_buffer* from,
                                       VkBufferImageCopy* region,
                                       const char* name);

  static vk_error copy_image_to_buffer(vk_device* dev,
                                       vk_render_essentials* essentials,
                                       vk_buffer* to,
                                       vk_image* from,
                                       VkImageLayout from_layout,
                                       VkBufferImageCopy* region,
                                       const char* name);

  static vk_error transition_images(vk_device* dev,
                                    vk_render_essentials* essentials,
                                    vk_image* images,
                                    uint32_t image_count,
                                    VkImageLayout from,
                                    VkImageLayout to,
                                    VkImageAspectFlags aspect,
                                    const char* name);

  static vk_error create_staging_buffer(vk_physical_device* phy_dev,
                                        vk_device* dev,
                                        vk_buffer* staging,
                                        uint8_t* contents,
                                        size_t size,
                                        const char* name);

  static vk_error transition_images_mipmaps(vk_physical_device* phy_dev,
                                            vk_device* dev,
                                            vk_render_essentials* essentials,
                                            vk_image* image,
                                            VkImageAspectFlags aspect,
                                            const char* name);

  static vk_error update_texture(vk_physical_device* phy_dev,
                                 vk_device* dev,
                                 vk_render_essentials* essentials,
                                 vk_image* image,
                                 VkImageLayout base_layout,
                                 uint8_t* contents,
                                 const char* name);

  static vk_error init_texture(vk_physical_device* phy_dev,
                               vk_device* dev,
                               vk_render_essentials* essentials,
                               vk_image* image,
                               VkImageLayout layout,
                               uint8_t* contents,
                               const char* name);

  static vk_error init_buffer(vk_physical_device* phy_dev,
                              vk_device* dev,
                              vk_render_essentials* essentials,
                              vk_buffer* buffer,
                              void* contents,
                              const char* name);

  static int finish(vk_render_essentials* essentials,
                    vk_device* dev,
                    vk_swapchain* swapchain,
                    VkImageLayout from_layout,
                    uint32_t image_index,
                    VkSemaphore wait_sem,
                    VkSemaphore signal_sem);
};

#endif  // EXAMPLES_VK_SHADERTOY_VULKAN_RENDER_H_
