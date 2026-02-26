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

#include "shader_toy.h"

#include "textures.h"

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

#define d VULKAN_HPP_DEFAULT_DISPATCHER

ShaderToy::ShaderToy() = default;

int ShaderToy::init(const int width,
                    const int height,
                    wl_display* wl_display,
                    wl_surface* wl_surface,
                    uint32_t dev_index,
                    const bool use_gpu_idx,
                    const bool debug,
                    const bool reload_shaders,
                    const VkPresentModeKHR present_mode) {
  os_window_ = {};

  dev_index_ = dev_index;
  use_gpu_idx_ = use_gpu_idx;
  os_window_.enable_debug = debug;
  os_window_.reload_shaders_on_resize = reload_shaders;
  os_window_.present_mode = present_mode;

  os_window_.app_data.iResolution[0] = width;
  os_window_.app_data.iResolution[1] = height;
  os_window_.wl_display = wl_display;
  os_window_.wl_surface = wl_surface;

  resize_size_[0] = static_cast<uint32_t>(os_window_.app_data.iResolution[0]);
  resize_size_[1] = static_cast<uint32_t>(os_window_.app_data.iResolution[1]);
  strncpy(os_window_.name, "Vulkan Shadertoy launcher", kAppNameStrLen);

  auto retval = EXIT_FAILURE;

  vk_error res = VulkanUtils::init(&vk_);
  if (!vk_error_is_success(&res)) {
    vk_error_printf(&res, "Could not initialize Vulkan\n");
    return retval;
  }

  res = create_surface(vk_, &swapchain_.surface, &os_window_);
  if (vk_error_is_error(&res)) {
    vk_error_printf(&res, "Could not create wl_surface.\n");
    exit_cleanup(vk_, nullptr, nullptr);
    return retval;
  }

  res = enumerate_devices(vk_, &swapchain_.surface, &phy_dev_, &dev_index,
                          use_gpu_idx);
  if (vk_error_is_error(&res)) {
    vk_error_printf(&res, "Could not enumerate devices\n");
    d.vkDestroySurfaceKHR(vk_, swapchain_.surface, nullptr);
    exit_cleanup(vk_, nullptr, nullptr);
    return retval;
  }

  res = setup(&phy_dev_, &dev_, VK_QUEUE_GRAPHICS_BIT,
              1 + OFFSCREEN_BUFFERS);  // cmd buffers alloc
  if (vk_error_is_error(&res)) {
    vk_error_printf(
        &res, "Could not setup logical device, command pools and queues\n");
    exit_cleanup(vk_, &dev_, &swapchain_);
    return retval;
  }

  swapchain_.swapchain = nullptr;
  res = get_swapchain(vk_, &phy_dev_, &dev_, &swapchain_, &os_window_, 1,
                      &os_window_.present_mode);
  if (vk_error_is_error(&res)) {
    vk_error_printf(&res, "Could not create wl_surface and swapchain\n");
    exit_cleanup(vk_, &dev_, &swapchain_);
    return retval;
  }

  render_loop_init(&phy_dev_, &dev_, &swapchain_, &os_window_);

  return 0;
}

bool ShaderToy::update_iKeyboard_texture(vk_physical_device* phy_dev,
                                         vk_device* dev,
                                         vk_render_essentials* essentials,
                                         render_data* render_data) {
  auto retval = VK_ERROR_NONE;
  if (!keyboard_draw_)
    return true;
  if (!essentials->first_render) {
    const auto res = d.vkWaitForFences(dev->device, 1, &essentials->exec_fence,
                                       true, 1000000000);
    vk_error_set_vkresult(&retval, res);
    if (res) {
      vk_error_printf(&retval, "Wait for fence failed\n");
      return false;
    }
  }

  retval = update_texture(
      phy_dev, dev, essentials,
      &render_data->images[IMAGE_TEXTURES + OFFSCREEN_BUFFERS],
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, keyboard_texture_, "iKeyboard");
  if (!vk_error_is_success(&retval))
    return false;

  return true;
}

vk_error ShaderToy::allocate_render_data(vk_physical_device* phy_dev,
                                         vk_device* dev,
                                         vk_swapchain* swapchain,
                                         vk_render_essentials* essentials,
                                         render_data* render_data,
                                         bool reload_shaders) {
  static bool load_once = false;
  auto retval = VK_ERROR_NONE;
  if (!load_once) {
    render_data->buffers[BUFFER_VERTICES] = (struct vk_buffer){
        .format = VK_FORMAT_UNDEFINED,
        .size = sizeof render_data->objects.vertices,
        .usage = static_cast<VkBufferUsageFlagBits>(
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
            VK_BUFFER_USAGE_TRANSFER_DST_BIT),
        .stage = VK_SHADER_STAGE_VERTEX_BIT,
        .make_view = false,
        .host_visible = false,
        .sharing_queues = nullptr,
        .sharing_queue_count = 0,
        .buffer = VK_NULL_HANDLE,
        .buffer_mem = VK_NULL_HANDLE,
        .view = VK_NULL_HANDLE,
    };

    render_data->buffers[BUFFER_INDICES] = (struct vk_buffer){
        .format = VK_FORMAT_UNDEFINED,
        .size = sizeof render_data->objects.indices,
        .usage = static_cast<VkBufferUsageFlagBits>(
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
            VK_BUFFER_USAGE_TRANSFER_DST_BIT),
        .stage = VK_SHADER_STAGE_VERTEX_BIT,
        .make_view = false,
        .host_visible = false,
        .sharing_queues = nullptr,
        .sharing_queue_count = 0,
        .buffer = VK_NULL_HANDLE,
        .buffer_mem = VK_NULL_HANDLE,
        .view = VK_NULL_HANDLE,
    };

    retval = create_buffers(phy_dev, dev, render_data->buffers, 2);
    if (!vk_error_is_success(&retval)) {
      vk_error_printf(
          &retval,
          "Failed to create vertex, index and transformation buffers\n");
      return retval;
    }
  }
  if (!load_once) {
    render_data->objects.vertices[0].pos[0] = 3.001f;
    render_data->objects.vertices[0].pos[1] = 1.001f;
    render_data->objects.vertices[0].pos[2] = 0.0f;

    render_data->objects.vertices[1].pos[0] = -1.001f;
    render_data->objects.vertices[1].pos[1] = -3.001f;
    render_data->objects.vertices[1].pos[2] = 0.0f;

    render_data->objects.vertices[2].pos[0] = -1.001f;
    render_data->objects.vertices[2].pos[1] = 1.001f;
    render_data->objects.vertices[2].pos[2] = 0.0f;

    render_data->objects.indices[0] = UINT16_C(0);
    render_data->objects.indices[1] = UINT16_C(1);
    render_data->objects.indices[2] = UINT16_C(2);

    retval = init_buffer(phy_dev, dev, essentials,
                         &render_data->buffers[BUFFER_VERTICES],
                         render_data->objects.vertices, "vertex");
    if (!vk_error_is_success(&retval))
      return retval;
    retval = init_buffer(phy_dev, dev, essentials,
                         &render_data->buffers[BUFFER_INDICES],
                         render_data->objects.indices, "index");
    if (!vk_error_is_success(&retval))
      return retval;

    for (uint32_t i = 0; i < IMAGE_TEXTURES; i++) {
      char txt[255] = {0};
      sprintf(txt, "textures/%d.png", i + 1);
#ifdef USE_stb_image
      retval = init_texture_file(phy_dev, dev, essentials,
                                 &render_data->images[i], txt, USE_MIPMAPS);
      if (!vk_error_is_success(&retval))
        retval = texture_empty(phy_dev, dev, essentials,
                               &render_data->images[i], 1, 1);
#else
      retval = texture_empty(phy_dev, dev, essentials, &render_data->images[i],
                             1, 1);
      if (!vk_error_is_success(&retval))
        return retval;
#endif
    }
    for (uint32_t i = IMAGE_TEXTURES; i < IMAGE_TEXTURES + OFFSCREEN_BUFFERS;
         i++) {
      retval = texture_empty(phy_dev, dev, essentials, &render_data->images[i],
                             1, 1);
      if (!vk_error_is_success(&retval))
        return retval;
    }
    retval = texture_empty(
        phy_dev, dev, essentials,
        &render_data->images[IMAGE_TEXTURES + OFFSCREEN_BUFFERS], 256,
        3);  // iKeyboard
    if (!vk_error_is_success(&retval))
      return retval;
  }
  if ((!load_once) || (reload_shaders)) {
    render_data->shaders[SHADER_MAIN_VERTEX] = (struct vk_shader){
        .spirv_file = "shaders/spv/main.vert.spv",
        .stage = VK_SHADER_STAGE_VERTEX_BIT,
        .shader = VK_NULL_HANDLE,
    };
    render_data->shaders[SHADER_MAIN_FRAGMENT] = (struct vk_shader){
        .spirv_file = "shaders/spv/main.frag.spv",
        .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
        .shader = VK_NULL_HANDLE,
    };
    char txt[OFFSCREEN_BUFFERS][255] = {{0}};
    for (uint32_t i = 0; i < OFFSCREEN_BUFFERS * 2; i += 2) {
      render_data->shaders[i + 2] = (struct vk_shader){
          .spirv_file = "shaders/spv/buf.vert.spv",
          .stage = VK_SHADER_STAGE_VERTEX_BIT,
          .shader = VK_NULL_HANDLE,
      };
      if (i > 0) {
        sprintf(txt[i / 2], "shaders/spv/buf%d.frag.spv", i / 2);
      } else {
        sprintf(txt[i / 2], "shaders/spv/buf.frag.spv");
      }
      render_data->shaders[i + 2 + 1] = (struct vk_shader){
          .spirv_file = txt[i / 2],
          .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
          .shader = VK_NULL_HANDLE,
      };
    }
#ifdef YARIV_SHADER
    retval = vk_load_shader_yariv(
        dev, (const uint32_t*)vs_code,
        &render_data->shaders[SHADER_MAIN_VERTEX].shader, sizeof(vs_code));
    if (!vk_error_is_success(&retval)) {
      vk_error_printf(&retval, "Could not load the shaders\n");
      return retval;
    }
    retval = vk_load_shader_yariv(
        dev, (const uint32_t*)fs_code,
        &render_data->shaders[SHADER_MAIN_FRAGMENT].shader, sizeof(fs_code));
    if (!vk_error_is_success(&retval)) {
      vk_error_printf(&retval, "Could not load the shaders\n");
      return retval;
    }
    for (uint32_t i = 0; i < OFFSCREEN_BUFFERS * 2; i += 2) {
      retval = vk_load_shader_yariv(dev, (const uint32_t*)buf_vs_code,
                                    &render_data->shaders[i + 2].shader,
                                    sizeof(buf_vs_code));
      if (!vk_error_is_success(&retval)) {
        vk_error_printf(&retval, "Could not load the shaders\n");
        return retval;
      }
      retval = vk_load_shader_yariv(
          dev, (const uint32_t*)(yariv_shaders[i / 2]),
          &render_data->shaders[i + 2 + 1].shader, yariv_shaders_size[i / 2]);
      if (!vk_error_is_success(&retval)) {
        vk_error_printf(&retval, "Could not load the shaders\n");
        return retval;
      }
    }
#else
    retval = load_shaders(dev, render_data->shaders, 2 + OFFSCREEN_BUFFERS * 2);
    if (!vk_error_is_success(&retval)) {
      vk_error_printf(&retval, "Could not load the shaders\n");
      return retval;
    }
#endif
  }
#if defined(VK_USE_PLATFORM_WAYLAND_KHR)
  struct VkExtent2D init_size = {
      .width = resize_size_[0],
      .height = resize_size_[1],
  };
#else
  struct VkExtent2D init_size = {
      .width = swapchain->surface_caps.currentExtent.width,
      .height = swapchain->surface_caps.currentExtent.height,
  };
#endif
  render_data->main_gbuffers = static_cast<vk_graphics_buffers*>(
      malloc(essentials->image_count * sizeof *render_data->main_gbuffers));
  for (uint32_t i = 0; i < essentials->image_count; ++i) {
    render_data->main_gbuffers[i] = (struct vk_graphics_buffers){
        .surface_size = init_size,
        .swapchain_image = essentials->images[i],
        .color_view = VK_NULL_HANDLE,
        .depth = {},
        .framebuffer = VK_NULL_HANDLE,
    };
  }

#ifdef NO_RESIZE_BUF
  if (!load_once) {
#endif
    render_data->buf_obuffers = static_cast<vk_offscreen_buffers*>(
        malloc(2 * (sizeof(*render_data->buf_obuffers)) * OFFSCREEN_BUFFERS));
    for (uint32_t i = 0; i < 2 * OFFSCREEN_BUFFERS; i++)
      render_data->buf_obuffers[i] = (struct vk_offscreen_buffers){
#if defined(CUSTOM_BUF_SIZE) && defined(NO_RESIZE_BUF)
          .surface_size = (struct VkExtent2D)CUSTOM_BUF_SIZE,
          .color = {},
          .depth = {},
          .framebuffer = VK_NULL_HANDLE,
#else
        .surface_size = init_size,
        .color = {},
        .depth = {},
        .framebuffer = VK_NULL_HANDLE,
#endif
      };
#ifdef NO_RESIZE_BUF
  }
#endif

  // 8bit BGRA for main_image VK_FORMAT_B8G8R8A8_UNORM
  if (swapchain->surface_format.format != VK_FORMAT_B8G8R8A8_UNORM) {
    main_image_srgb_ = true;
  }
  retval = create_graphics_buffers(
      phy_dev, dev, swapchain->surface_format.format,
      render_data->main_gbuffers, essentials->image_count,
      &render_data->main_render_pass, VK_C_CLEAR, VK_WITHOUT_DEPTH);
  if (!vk_error_is_success(&retval)) {
    vk_error_printf(&retval, "Could not create graphics buffers\n");
    return retval;
  }

#ifdef NO_RESIZE_BUF
  if (!load_once) {
#endif
    for (uint32_t i = 0; i < OFFSCREEN_BUFFERS; i++) {
      // 32-bit format RGBA for buffers VK_FORMAT_R32G32B32A32_SFLOAT
      retval = create_offscreen_buffers(
          phy_dev, dev, VK_FORMAT_R32G32B32A32_SFLOAT,
          &render_data->buf_obuffers[i * 2], 2,
          &render_data->buf_render_pass[i], VK_C_CLEAR, VK_WITHOUT_DEPTH, true);
      if (!vk_error_is_success(&retval)) {
        vk_error_printf(&retval, "Could not create off-screen buffers\n");
        return retval;
      }
    }
#ifdef NO_RESIZE_BUF
  }
#endif

  vk_image** image_pointer;
  image_pointer = static_cast<vk_image**>(
      malloc(1 * sizeof(vk_image*) *
             (IMAGE_TEXTURES + OFFSCREEN_BUFFERS + iKeyboard)));
  for (uint32_t i = 0; i < IMAGE_TEXTURES + OFFSCREEN_BUFFERS + iKeyboard;
       i++) {
    image_pointer[i] = &render_data->images[i];
  }

  VkPushConstantRange push_constant_range = {
      .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
      .offset = 0,
      .size = sizeof render_data->push_constants,
  };

  /*******************
   * BUF PART *
   *******************/
#ifdef NO_RESIZE_BUF
  if (!load_once) {
#endif
    for (uint32_t i = 0; i < OFFSCREEN_BUFFERS; i++) {
      /* Layouts */

      struct vk_resources resources = {
          .images = *image_pointer,
          .image_count = IMAGE_TEXTURES + OFFSCREEN_BUFFERS + iKeyboard,
          .buffers = render_data->buffers,
          .buffer_count = 2,
          .shaders = &render_data->shaders[SHADER_MAIN_FRAGMENT + 1 + i * 2],
          .shader_count = 2,
          .push_constants = &push_constant_range,
          .push_constant_count = 1,
          .render_pass = render_data->buf_render_pass[i],
      };
      render_data->buf_layout[i] = (struct vk_layout){
          .resources = &resources,
          .set_layout = VK_NULL_HANDLE,
          .pipeline_layout = VK_NULL_HANDLE,
      };
      uint32_t img_patern[3] = {IMAGE_TEXTURES, OFFSCREEN_BUFFERS, iKeyboard};
      retval = make_graphics_layouts(dev, &render_data->buf_layout[i], 1, true,
                                     img_patern, 3);
      if (!vk_error_is_success(&retval)) {
        vk_error_printf(
            &retval,
            "BUF: Could not create descriptor set or pipeline layouts\n");
        return retval;
      }

      /* Pipeline */
      VkVertexInputBindingDescription vertex_binding = {
          .binding = 0,
          .stride = sizeof *render_data->objects.vertices,
          .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
      };

      VkVertexInputAttributeDescription vertex_attributes[1] = {
          {
              .location = 0,
              .binding = 0,
              .format = VK_FORMAT_R32G32B32_SFLOAT,
              .offset = 0,
          },
      };
      render_data->buf_pipeline[i] = (struct vk_pipeline){
          .layout = &render_data->buf_layout[i],
          .vertex_input_state =
              {
                  .sType =
                      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
                  .pNext = nullptr,
                  .flags = 0,
                  .vertexBindingDescriptionCount = 1,
                  .pVertexBindingDescriptions = &vertex_binding,
                  .vertexAttributeDescriptionCount = 1,
                  .pVertexAttributeDescriptions = vertex_attributes,
              },
          .input_assembly_state =
              {
                  .sType =
                      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
                  .pNext = nullptr,
                  .flags = 0,
                  .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
                  .primitiveRestartEnable = VK_FALSE,
              },
          .tessellation_state =
              {
                  .sType =
                      VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO,
                  .pNext = nullptr,
                  .flags = 0,
                  .patchControlPoints = 0,
              },
          .thread_count = 1,
          .pipeline = VK_NULL_HANDLE,
          .set_pool = VK_NULL_HANDLE,
      };

      retval =
          make_graphics_pipelines(dev, &render_data->buf_pipeline[i], 1, false);
      if (!vk_error_is_success(&retval)) {
        vk_error_printf(&retval, "BUF: Could not create graphics pipeline\n");
        return retval;
      }

      /* Descriptor Set */
      VkDescriptorSetAllocateInfo set_info = {
          .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
          .pNext = nullptr,
          .descriptorPool = render_data->buf_pipeline[i].set_pool,
          .descriptorSetCount = 1,
          .pSetLayouts = &render_data->buf_layout[i].set_layout,
      };
      auto res = d.vkAllocateDescriptorSets(dev->device, &set_info,
                                            &render_data->buf_desc_set[i]);
      retval = VK_ERROR_NONE;
      vk_error_set_vkresult(&retval, res);
      if (res) {
        vk_error_printf(&retval,
                        "BUF: Could not allocate descriptor set from pool\n");
        return retval;
      }
    }

#ifdef NO_RESIZE_BUF
  }
#endif

  /*******************
   * MAIN_IMAGE PART *
   *******************/
  {
    /* Layouts */

    struct vk_resources resources = {
        .images = *image_pointer,
        .image_count = IMAGE_TEXTURES + OFFSCREEN_BUFFERS + iKeyboard,
        .buffers = render_data->buffers,
        .buffer_count = 2,
        .shaders = &render_data->shaders[SHADER_MAIN_VERTEX],
        .shader_count = 2,
        .push_constants = &push_constant_range,
        .push_constant_count = 1,
        .render_pass = render_data->main_render_pass,
    };
    render_data->main_layout = (struct vk_layout){
        .resources = &resources,
        .set_layout = VK_NULL_HANDLE,
        .pipeline_layout = VK_NULL_HANDLE,
    };
    uint32_t img_pattern[3] = {IMAGE_TEXTURES, OFFSCREEN_BUFFERS, iKeyboard};
    retval = make_graphics_layouts(dev, &render_data->main_layout, 1, true,
                                   img_pattern, 3);
    if (!vk_error_is_success(&retval)) {
      vk_error_printf(&retval,
                      "Could not create descriptor set or pipeline layouts\n");
      return retval;
    }

    /* Pipeline */
    VkVertexInputBindingDescription vertex_binding = {
        .binding = 0,
        .stride = sizeof *render_data->objects.vertices,
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    };

    VkVertexInputAttributeDescription vertex_attributes[1] = {
        {
            .location = 0,
            .binding = 0,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = 0,
        },
    };
    render_data->main_pipeline = (struct vk_pipeline){
        .layout = &render_data->main_layout,
        .vertex_input_state =
            {
                .sType =
                    VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .vertexBindingDescriptionCount = 1,
                .pVertexBindingDescriptions = &vertex_binding,
                .vertexAttributeDescriptionCount = 1,
                .pVertexAttributeDescriptions = vertex_attributes,
            },
        .input_assembly_state =
            {
                .sType =
                    VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
                .primitiveRestartEnable = VK_FALSE,
            },
        .tessellation_state =
            {
                .sType =
                    VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .patchControlPoints = 0,
            },
        .thread_count = 1,
        .pipeline = VK_NULL_HANDLE,
        .set_pool = VK_NULL_HANDLE,
    };

    retval =
        make_graphics_pipelines(dev, &render_data->main_pipeline, 1, false);
    if (!vk_error_is_success(&retval)) {
      vk_error_printf(&retval, "Could not create graphics pipeline\n");
      return retval;
    }

    /* Descriptor Set */
    VkDescriptorSetAllocateInfo set_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = nullptr,
        .descriptorPool = render_data->main_pipeline.set_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &render_data->main_layout.set_layout,
    };
    auto res = d.vkAllocateDescriptorSets(dev->device, &set_info,
                                          &render_data->main_desc_set);
    retval = VK_ERROR_NONE;
    vk_error_set_vkresult(&retval, res);
    if (res) {
      vk_error_printf(&retval, "Could not allocate descriptor set from pool\n");
      return retval;
    }
  }

  load_once = true;
  free(image_pointer);

  return retval;
}

void ShaderToy::free_render_data(struct vk_device* dev,
                                 struct vk_render_essentials* essentials,
                                 struct render_data* render_data) {
  d.vkDeviceWaitIdle(dev->device);

  free_pipelines(dev, &render_data->main_pipeline, 1);
  free_layouts(dev, &render_data->main_layout, 1);
  free_pipelines(dev, render_data->buf_pipeline, OFFSCREEN_BUFFERS);
  free_layouts(dev, render_data->buf_layout, OFFSCREEN_BUFFERS);
  free_images(dev, render_data->images,
              IMAGE_TEXTURES + OFFSCREEN_BUFFERS + iKeyboard);

  free_buffers(dev, render_data->buffers, 2);
  free_shaders(dev, render_data->shaders, 2 + OFFSCREEN_BUFFERS * 2);

  for (uint32_t i = 0; i < OFFSCREEN_BUFFERS; i++) {
    free_offscreen_buffers(dev, &render_data->buf_obuffers[i * 2], 2,
                           render_data->buf_render_pass[i]);
  }
  free_graphics_buffers(dev, render_data->main_gbuffers,
                        essentials->image_count, render_data->main_render_pass);

  free(render_data->main_gbuffers);
  free(render_data->buf_obuffers);
}

// TO DO FREE BZUF FENCE LOOP
void ShaderToy::exit_cleanup_render_loop(vk_device* dev,
                                         vk_render_essentials* essentials,
                                         render_data* render_data,
                                         VkSemaphore wait_buf_sem,
                                         VkSemaphore wait_main_sem,
                                         VkFence offscreen_fence) {
  d.vkDeviceWaitIdle(dev->device);
  if (offscreen_fence != nullptr)
    d.vkDestroyFence(dev->device, offscreen_fence, nullptr);
  if (wait_main_sem != nullptr)
    d.vkDestroySemaphore(dev->device, wait_main_sem, nullptr);
  if (wait_buf_sem != nullptr)
    d.vkDestroySemaphore(dev->device, wait_buf_sem, nullptr);
  free_render_data(dev, essentials, render_data);
  cleanup_essentials(essentials, dev);
}

void ShaderToy::render_loop_init(vk_physical_device* phy_dev,
                                 vk_device* dev,
                                 vk_swapchain* swapchain,
                                 app_os_window* os_window) {
  auto retval = VK_ERROR_NONE;
  static bool once = false;

  auto res = get_essentials(&essentials_, phy_dev, dev, swapchain);
  if (res) {
    cleanup_essentials(&essentials_, dev);
    return;
  }

  if (!once) {
    uint32_t* presentable_queues = nullptr;
    uint32_t presentable_queue_count = 0;

    retval =
        get_presentable_queues(phy_dev, dev, swapchain->surface,
                               &presentable_queues, &presentable_queue_count);
    if (!vk_error_is_success(&retval) || presentable_queue_count == 0) {
      spdlog::error(
          "No presentable queue families.  You should have got this error in "
          "vk_render_get_essentials before.\n");
      free(presentable_queues);
      cleanup_essentials(&essentials_, dev);
      return;
    }

    for (uint32_t i = 0; i < OFFSCREEN_BUFFERS; i++) {
      offscreen_queue_[i] =
          dev->command_pools[presentable_queues[0]]
              .queues[0];  // used only one presentable queue always
      offscreen_cmd_buffer_[i] =
          dev->command_pools[presentable_queues[0]].buffers[1 + i];
    }

    free(presentable_queues);
  }

  retval =
      allocate_render_data(phy_dev, dev, swapchain, &essentials_, &render_data_,
                           os_window->reload_shaders_on_resize);
  if (!vk_error_is_success(&retval)) {
    free_render_data(dev, &essentials_, &render_data_);
    cleanup_essentials(&essentials_, dev);
    return;
  }
#ifdef NO_RESIZE_BUF
  if (!once) {
#endif
    for (uint32_t i = 0; i < OFFSCREEN_BUFFERS * 2; i++) {
      retval = transition_images(
          dev, &essentials_, &render_data_.buf_obuffers[i].color, 1,
          VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
          VK_IMAGE_ASPECT_COLOR_BIT, "off-screen color");
      if (!vk_error_is_success(&retval)) {
        free_render_data(dev, &essentials_, &render_data_);
        cleanup_essentials(&essentials_, dev);
        return;
      }
    }
#ifdef NO_RESIZE_BUF
  }
#endif
  if (!once) {
    constexpr VkSemaphoreCreateInfo sem_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
    };
    VkResult vk_res =
        d.vkCreateSemaphore(dev->device, &sem_info, nullptr, &wait_buf_sem_);
    vk_error_set_vkresult(&retval, vk_res);
    if (vk_res) {
      vk_error_printf(&retval, "Failed to create wait-render semaphore\n");
      exit_cleanup_render_loop(dev, &essentials_, &render_data_, wait_buf_sem_,
                               wait_main_sem_, offscreen_fence_);
      return;
    }
    vk_res =
        d.vkCreateSemaphore(dev->device, &sem_info, nullptr, &wait_main_sem_);
    vk_error_set_vkresult(&retval, vk_res);
    if (vk_res) {
      vk_error_printf(&retval,
                      "Failed to create wait-post-process semaphore\n");
      exit_cleanup_render_loop(dev, &essentials_, &render_data_, wait_buf_sem_,
                               wait_main_sem_, offscreen_fence_);
      return;
    }

    VkFenceCreateInfo fence_info = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
    };
    vk_res =
        d.vkCreateFence(dev->device, &fence_info, nullptr, &offscreen_fence_);
    vk_error_set_vkresult(&retval, vk_res);
    if (vk_res) {
      vk_error_printf(&retval, "Failed to create fence\n");
      exit_cleanup_render_loop(dev, &essentials_, &render_data_, wait_buf_sem_,
                               wait_main_sem_, offscreen_fence_);
      return;
    }
  }
  once = true;
  os_window->prepared = true;
  os_window->resize_event = false;
}

void ShaderToy::exit_cleanup(VkInstance vk,
                             struct vk_device* dev,
                             struct vk_swapchain* swapchain) {
  if (swapchain)
    free_swapchain(vk, dev, swapchain);
  if (dev)
    cleanup(dev);
  exit(vk);
}

bool ShaderToy::on_window_resize(struct vk_physical_device* phy_dev,
                                 struct vk_device* dev,
                                 struct vk_render_essentials* essentials,
                                 struct vk_swapchain* swapchain,
                                 struct render_data* render_data,
                                 struct app_os_window* os_window) {
  vk_error res = VK_ERROR_NONE;

  if (!os_window->prepared)
    return true;

  d.vkDeviceWaitIdle(dev->device);
  os_window->prepared = false;

  resize_size_[0] = static_cast<uint32_t>(os_window->app_data.iResolution[0]);
  resize_size_[1] = static_cast<uint32_t>(os_window->app_data.iResolution[1]);

  free_pipelines(dev, &render_data->main_pipeline, 1);
  free_graphics_buffers(dev, render_data->main_gbuffers,
                        essentials->image_count, render_data->main_render_pass);
  free_layouts(dev, &render_data->main_layout, 1);

#ifndef NO_RESIZE_BUF
  free_pipelines(dev, render_data->buf_pipeline, OFFSCREEN_BUFFERS);
  for (uint32_t i = 0; i < OFFSCREEN_BUFFERS; i++) {
    free_offscreen_buffers(dev, &render_data->buf_obuffers[i * 2], 2,
                           render_data->buf_render_pass[i]);
  }
  free_layouts(dev, render_data->buf_layout, OFFSCREEN_BUFFERS);
#endif

  if (os_window->reload_shaders_on_resize)
    free_shaders(dev, render_data->shaders, 2 + OFFSCREEN_BUFFERS * 2);

  cleanup_essentials(essentials, dev);

  free(render_data->main_gbuffers);

#ifndef NO_RESIZE_BUF
  free(render_data->buf_obuffers);
#endif

  res = get_swapchain(vk_, phy_dev, dev, swapchain, os_window, 1,
                      &os_window->present_mode);
  if (vk_error_is_error(&res)) {
    vk_error_printf(&res, "Could not create wl_surface and swapchain\n");
    exit_cleanup(vk_, dev, swapchain);
    return false;
  }

  render_loop_init(phy_dev, dev, swapchain, os_window);

  return true;
}

void ShaderToy::update_params(struct app_data_struct* app_data, bool fps_lock) {
  if (fps_lock) {
    FPS_LOCK(30);
  }
  float delta = update_fps_delta();
  if (!app_data->pause) {
    app_data->iTime += delta;
  }
  app_data->iFrame++;
  app_data->iTimeDelta = delta;
}

void ShaderToy::set_push_constants(struct app_os_window* os_window) {
  my_time_struct my_time{};
  get_local_time(&my_time);
  const float day_sec = static_cast<float>(my_time.msec) / 1000.0f +
                        static_cast<float>(my_time.sec) +
                        static_cast<float>(my_time.min * 60) +
                        static_cast<float>(my_time.hour * 3600);
  last_iMousel_clicked_[0] = last_iMousel_clicked_[1];
  last_iMousel_clicked_[1] = os_window->app_data.iMouse_click[0];

  render_data_.push_constants.iResolution[0] =
      static_cast<float>(os_window->app_data.iResolution[0]);
  render_data_.push_constants.iResolution[1] =
      static_cast<float>(os_window->app_data.iResolution[1]);
  render_data_.push_constants.iTime = os_window->app_data.iTime;
  render_data_.push_constants.iTimeDelta = os_window->app_data.iTimeDelta;
  render_data_.push_constants.iFrame = os_window->app_data.iFrame;
  render_data_.push_constants.iMouse[0] =
      static_cast<float>(os_window->app_data.iMouse[0]);
  render_data_.push_constants.iMouse[1] =
      static_cast<float>(os_window->app_data.iMouse[1]);
  render_data_.push_constants.iMouse[2] =
      static_cast<float>(os_window->app_data.iMouse_lclick[0]);
  render_data_.push_constants.iMouse[3] = static_cast<float>(
      (last_iMousel_clicked_[0]) ? -abs(os_window->app_data.iMouse_lclick[1])
                                 : os_window->app_data.iMouse_lclick[1]);
  render_data_.push_constants.iMouse_lr[0] =
      static_cast<int>(os_window->app_data.iMouse_click[0]);
  render_data_.push_constants.iMouse_lr[1] =
      static_cast<int>(os_window->app_data.iMouse_click[1]);
  render_data_.push_constants.iDate[0] = static_cast<float>(my_time.year);
  render_data_.push_constants.iDate[1] = static_cast<float>(my_time.month);
  render_data_.push_constants.iDate[2] = static_cast<float>(my_time.day);
  render_data_.push_constants.iDate[3] = day_sec;
  render_data_.push_constants.debugdraw =
      static_cast<int>(os_window->app_data.drawdebug);
  render_data_.push_constants.pCustom =
      (os_window->app_data.pause ? 1 : 0) + (main_image_srgb_ ? 10 : 0);
}

void ShaderToy::update_push_constants_window_size(app_os_window* os_window) {
  render_data_.push_constants.iMouse[0] =
      static_cast<float>(os_window->app_data.iMouse[0]);
  render_data_.push_constants.iMouse[1] =
      static_cast<float>(os_window->app_data.iMouse[1]);
  render_data_.push_constants.iMouse[2] =
      static_cast<float>(os_window->app_data.iMouse_lclick[0]);
  render_data_.push_constants.iMouse[3] = static_cast<float>(
      (last_iMousel_clicked_[0]) ? -abs(os_window->app_data.iMouse_lclick[1])
                                 : os_window->app_data.iMouse_lclick[1]),
  render_data_.push_constants.iResolution[0] =
      static_cast<float>(os_window->app_data.iResolution[0]);
  render_data_.push_constants.iResolution[1] =
      static_cast<float>(os_window->app_data.iResolution[1]);
}

#define sign(x) (((x) > 0) ? 1 : (((x) < 0) ? -1 : 0))

void ShaderToy::update_push_constants_local_size(const float width,
                                                 const float height) {
  render_data_.push_constants.iMouse[0] =
      static_cast<float>(((render_data_.push_constants.iMouse[0] /
                           render_data_.push_constants.iResolution[1]) -
                          0.5 * (render_data_.push_constants.iResolution[0] /
                                 render_data_.push_constants.iResolution[1])) *
                             height +
                         0.5 * width);
  render_data_.push_constants.iMouse[1] =
      static_cast<float>(((render_data_.push_constants.iMouse[1] /
                           render_data_.push_constants.iResolution[1]) -
                          0.5) *
                             height +
                         0.5 * height);
  render_data_.push_constants.iMouse[2] =
      static_cast<float>(sign(render_data_.push_constants.iMouse[2]) *
                         (((std::fabs(render_data_.push_constants.iMouse[2]) /
                            render_data_.push_constants.iResolution[1]) -
                           0.5 * (render_data_.push_constants.iResolution[0] /
                                  render_data_.push_constants.iResolution[1])) *
                              height +
                          0.5 * width));
  render_data_.push_constants.iMouse[3] =
      static_cast<float>(sign(render_data_.push_constants.iMouse[3]) *
                         (((std::fabs(render_data_.push_constants.iMouse[3]) /
                            render_data_.push_constants.iResolution[1]) -
                           0.5) *
                              height +
                          0.5 * height));
  render_data_.push_constants.iResolution[0] = width;
  render_data_.push_constants.iResolution[1] = height;
}

bool ShaderToy::render_loop_buf(struct vk_physical_device* /* phy_dev */,
                                struct vk_device* dev,
                                struct vk_render_essentials* essentials,
                                struct render_data* render_data,
                                VkCommandBuffer cmd_buffer,
                                int render_index,
                                int buffer_index,
                                struct app_data_struct* /* app_data */) {
  auto retval = VK_ERROR_NONE;
  VkResult res;

  if ((!essentials->first_render) && (buffer_index == 0)) {
    res = d.vkWaitForFences(dev->device, 1, &essentials->exec_fence, true,
                            1000000000);
    vk_error_set_vkresult(&retval, res);
    if (res) {
      vk_error_printf(&retval, "Wait for fence failed\n");
      return false;
    }
  }
#ifdef NO_RESIZE_BUF
  update_push_constants_local_size(
      render_data->buf_obuffers[render_index + buffer_index * 2]
          .surface_size.width,
      render_data->buf_obuffers[render_index + buffer_index * 2]
          .surface_size.height);
#endif
  d.vkResetCommandBuffer(cmd_buffer, 0);
  constexpr VkCommandBufferBeginInfo begin_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .pNext = nullptr,
      .flags = 0,
      .pInheritanceInfo = nullptr,
  };
  res = d.vkBeginCommandBuffer(cmd_buffer, &begin_info);
  vk_error_set_vkresult(&retval, res);
  if (res) {
    vk_error_printf(&retval,
                    "BUF: Couldn't even begin recording a command buffer\n");
    return false;
  };
  VkImageMemoryBarrier image_barrier = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      .pNext = nullptr,
      .srcAccessMask = VK_ACCESS_MEMORY_READ_BIT,
      .dstAccessMask = VK_ACCESS_MEMORY_WRITE_BIT,
      .oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = render_data->buf_obuffers[render_index + buffer_index * 2]
                   .color.image,
      .subresourceRange =
          {
              .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
              .baseMipLevel = 0,
              .levelCount = 1,
              .baseArrayLayer = 0,
              .layerCount = 1,
          },
  };

  d.vkCmdPipelineBarrier(cmd_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, 0, 0, nullptr, 0,
                         nullptr, 1, &image_barrier);

  VkClearValue clear_values = {
      .color =
          {
              .float32 = {0.0, 0.0, 0.0, 0.0},
          },
  };
  const VkRenderPassBeginInfo pass_info = {
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
      .pNext = nullptr,
      .renderPass = render_data->buf_render_pass[buffer_index],
      .framebuffer = render_data->buf_obuffers[render_index + buffer_index * 2]
                         .framebuffer,
      .renderArea =
          {
              .offset =
                  {
                      .x = 0,
                      .y = 0,
                  },
              .extent =
                  render_data->buf_obuffers[render_index + buffer_index * 2]
                      .surface_size,
          },
      .clearValueCount = 1,
      .pClearValues = &clear_values,
  };

  d.vkCmdBeginRenderPass(cmd_buffer, &pass_info, VK_SUBPASS_CONTENTS_INLINE);
  d.vkCmdBindPipeline(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      render_data->buf_pipeline[buffer_index].pipeline);

  int render_index_t[OFFSCREEN_BUFFERS];
  for (uint32_t i = 0; i < OFFSCREEN_BUFFERS; i++) {
    if (static_cast<int>(i) < buffer_index) {
      render_index_t[i] = render_index + static_cast<int>(i) * 2;
    } else {
      render_index_t[i] = render_index - 1 + static_cast<int>(i) * 2;
      if (render_index_t[i] < static_cast<int>(i) * 2)
        render_index_t[i] = 1 + static_cast<int>(i) * 2;
    }
  }

  VkDescriptorImageInfo
      set_write_image_info[IMAGE_TEXTURES + OFFSCREEN_BUFFERS + iKeyboard] = {};
  for (uint32_t i = 0; i < IMAGE_TEXTURES; i++) {
    set_write_image_info[i] = (VkDescriptorImageInfo){
        .sampler = render_data->images[i].sampler,
        .imageView = render_data->images[i].view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
  }
  for (uint32_t i = 0; i < OFFSCREEN_BUFFERS; i++) {
    set_write_image_info[IMAGE_TEXTURES + i] = (VkDescriptorImageInfo){
        .sampler = render_data->buf_obuffers[render_index_t[i]].color.sampler,
        .imageView = render_data->buf_obuffers[render_index_t[i]].color.view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
  }
  set_write_image_info[IMAGE_TEXTURES + OFFSCREEN_BUFFERS] =
      (VkDescriptorImageInfo){
          .sampler =
              render_data->images[IMAGE_TEXTURES + OFFSCREEN_BUFFERS].sampler,
          .imageView =
              render_data->images[IMAGE_TEXTURES + OFFSCREEN_BUFFERS].view,
          .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      };

  VkWriteDescriptorSet set_write[3] = {
      {
          .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
          .pNext = nullptr,
          .dstSet = render_data->buf_desc_set[buffer_index],
          .dstBinding = 0,
          .dstArrayElement = 0,
          .descriptorCount = IMAGE_TEXTURES,
          .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
          .pImageInfo = &set_write_image_info[0],
          .pBufferInfo = nullptr,
          .pTexelBufferView = nullptr,
      },
      {
          .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
          .pNext = nullptr,
          .dstSet = render_data->buf_desc_set[buffer_index],
          .dstBinding = IMAGE_TEXTURES,
          .dstArrayElement = 0,
          .descriptorCount = OFFSCREEN_BUFFERS,
          .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
          .pImageInfo = &set_write_image_info[IMAGE_TEXTURES],
          .pBufferInfo = nullptr,
          .pTexelBufferView = nullptr,
      },
      {
          .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
          .pNext = nullptr,
          .dstSet = render_data->buf_desc_set[buffer_index],
          .dstBinding = IMAGE_TEXTURES + OFFSCREEN_BUFFERS,
          .dstArrayElement = 0,
          .descriptorCount = iKeyboard,
          .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
          .pImageInfo =
              &set_write_image_info[IMAGE_TEXTURES + OFFSCREEN_BUFFERS],
          .pBufferInfo = nullptr,
          .pTexelBufferView = nullptr,
      },
  };
  d.vkUpdateDescriptorSets(dev->device, 3, set_write, 0, nullptr);

  d.vkCmdBindDescriptorSets(
      cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
      render_data->buf_layout[buffer_index].pipeline_layout, 0, 1,
      &render_data->buf_desc_set[buffer_index], 0, nullptr);
  VkDeviceSize vertices_offset = 0;
  d.vkCmdBindVertexBuffers(cmd_buffer, 0, 1,
                           &render_data->buffers[BUFFER_VERTICES].buffer,
                           &vertices_offset);
  d.vkCmdBindIndexBuffer(cmd_buffer,
                         render_data->buffers[BUFFER_INDICES].buffer, 0,
                         VK_INDEX_TYPE_UINT16);

  VkViewport viewport = {
      .x = 0,
      .y = 0,
      .width = static_cast<float>(
          render_data->buf_obuffers[render_index + buffer_index * 2]
              .surface_size.width),
      .height = static_cast<float>(
          render_data->buf_obuffers[render_index + buffer_index * 2]
              .surface_size.height),
      .minDepth = 0,
      .maxDepth = 1,
  };
  d.vkCmdSetViewport(cmd_buffer, 0, 1, &viewport);

  VkRect2D scissor = {
      .offset =
          {
              .x = 0,
              .y = 0,
          },
      .extent = render_data->buf_obuffers[render_index + buffer_index * 2]
                    .surface_size,
  };
  d.vkCmdSetScissor(cmd_buffer, 0, 1, &scissor);

  d.vkCmdPushConstants(
      cmd_buffer, render_data->buf_layout[buffer_index].pipeline_layout,
      VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof render_data->push_constants,
      &render_data->push_constants);

  d.vkCmdDrawIndexed(cmd_buffer, 3, 1, 0, 0, 0);
  d.vkCmdEndRenderPass(cmd_buffer);

  image_barrier = (VkImageMemoryBarrier){
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      .pNext = nullptr,
      .srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT,
      .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
      .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = render_data->buf_obuffers[render_index + buffer_index * 2]
                   .color.image,
      .subresourceRange =
          {
              .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
              .baseMipLevel = 0,
              .levelCount = 1,
              .baseArrayLayer = 0,
              .layerCount = 1,
          },
  };

  d.vkCmdPipelineBarrier(cmd_buffer, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
                         VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0,
                         nullptr, 1, &image_barrier);
  d.vkEndCommandBuffer(cmd_buffer);
  return true;
}

bool ShaderToy::render_loop_draw(vk_physical_device* phy_dev,
                                 vk_device* dev,
                                 vk_swapchain* swapchain,
                                 app_os_window* os_window) {
  if (!os_window->prepared)
    return true;
  static int render_index = 0;
  VkResult res;
  auto retval = VK_ERROR_NONE;

  set_push_constants(os_window);
  if (!update_iKeyboard_texture(phy_dev, dev, &essentials_, &render_data_))
    return false;

  for (uint32_t i = 0; i < OFFSCREEN_BUFFERS; i++) {
    if (!render_loop_buf(phy_dev, dev, &essentials_, &render_data_,
                         offscreen_cmd_buffer_[i], render_index,
                         static_cast<int>(i), &os_window->app_data)) {
      spdlog::error("Error on rendering buffers");
      return false;
    }
    update_push_constants_window_size(os_window);

    if (i == 0) {  // wait main screen
      if (!first_submission_) {
        res = d.vkWaitForFences(dev->device, 1, &offscreen_fence_, true,
                                1000000000);
        vk_error_set_vkresult(&retval, res);
        if (res) {
          vk_error_printf(&retval, "Wait for main fence failed\n");
          return false;
        }
      }

      VkPipelineStageFlags wait_sem_stages[1] = {
          VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT};
      VkSubmitInfo submit_info = {
          .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
          .pNext = nullptr,
          .waitSemaphoreCount = first_submission_ ? UINT32_C(0) : UINT32_C(1),
          .pWaitSemaphores = &wait_main_sem_,
          .pWaitDstStageMask = wait_sem_stages,
          .commandBufferCount = 1,
          .pCommandBuffers = &offscreen_cmd_buffer_[i],
          .signalSemaphoreCount = 1,
          .pSignalSemaphores = &wait_buf_sem_,
      };
      res = d.vkResetFences(dev->device, 1, &offscreen_fence_);
      vk_error_set_vkresult(&retval, res);
      if (res) {
        vk_error_printf(&retval, "Failed to reset fence\n");
        return false;
      }
      d.vkQueueSubmit(offscreen_queue_[i], 1, &submit_info, offscreen_fence_);
      first_submission_ = false;
    } else {  // wait last buf/shader in loop, if multi VkQueue supported
      res = d.vkWaitForFences(dev->device, 1, &offscreen_fence_, true,
                              1000000000);
      vk_error_set_vkresult(&retval, res);
      if (res) {
        vk_error_printf(&retval, "Wait for buf fence failed\n");
        return false;
      }

      VkPipelineStageFlags wait_sem_stages[1] = {
          VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT};
      VkSubmitInfo submit_info = {
          .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
          .pNext = nullptr,
          .waitSemaphoreCount = 1,
          .pWaitSemaphores = &wait_buf_sem_,
          .pWaitDstStageMask = wait_sem_stages,
          .commandBufferCount = 1,
          .pCommandBuffers = &offscreen_cmd_buffer_[i],
          .signalSemaphoreCount = 1,
          .pSignalSemaphores = &wait_main_sem_,  // used main sem
      };
      res = d.vkResetFences(dev->device, 1, &offscreen_fence_);
      vk_error_set_vkresult(&retval, res);
      if (res) {
        vk_error_printf(&retval, "Failed to reset fence\n");
        return false;
      }
      d.vkQueueSubmit(offscreen_queue_[i], 1, &submit_info, offscreen_fence_);
      VkSemaphore tmp_sem = wait_buf_sem_;
      wait_buf_sem_ = wait_main_sem_;
      wait_main_sem_ = tmp_sem;
    }
  }

  uint32_t image_index;

  int result = start(&essentials_, dev, swapchain,
                     VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, &image_index);
  if (result == VK_ERROR_OUT_OF_DATE_KHR) {
    os_window->resize_event = true;
    result = 0;
    first_submission_ = true;
    return true;
  } else if (result == VK_ERROR_SURFACE_LOST_KHR) {
    d.vkDestroySurfaceKHR(vk_, swapchain->surface, nullptr);
    retval = create_surface(vk_, &swapchain->surface, os_window);
    if (!vk_error_is_success(&retval))
      return false;
    os_window->resize_event = true;
    result = 0;
    first_submission_ = true;
    return true;
  }
  if (result)
    return false;

  VkClearValue clear_values = {
      .color =
          {
              .float32 = {0.0f, 0.0f, 0.0f, 0.0f},
          },
  };
  VkRenderPassBeginInfo pass_info = {
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
      .pNext = nullptr,
      .renderPass = render_data_.main_render_pass,
      .framebuffer = render_data_.main_gbuffers[image_index].framebuffer,
      .renderArea =
          {
              .offset =
                  {
                      .x = 0,
                      .y = 0,
                  },
              .extent = render_data_.main_gbuffers[image_index].surface_size,
          },
      .clearValueCount = 1,
      .pClearValues = &clear_values,
  };

  d.vkCmdBeginRenderPass(essentials_.cmd_buffer, &pass_info,
                         VK_SUBPASS_CONTENTS_INLINE);
  d.vkCmdBindPipeline(essentials_.cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      render_data_.main_pipeline.pipeline);

  VkDescriptorImageInfo
      set_write_image_info[IMAGE_TEXTURES + OFFSCREEN_BUFFERS + iKeyboard] = {};
  for (uint32_t i = 0; i < IMAGE_TEXTURES; i++) {
    set_write_image_info[i] = (VkDescriptorImageInfo){
        .sampler = render_data_.images[i].sampler,
        .imageView = render_data_.images[i].view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
  }
  for (uint32_t i = 0; i < OFFSCREEN_BUFFERS; i++) {
    set_write_image_info[IMAGE_TEXTURES + i] = (VkDescriptorImageInfo){
        .sampler =
            render_data_
                .buf_obuffers[i * 2 + static_cast<uint32_t>(render_index)]
                .color.sampler,
        .imageView =
            render_data_
                .buf_obuffers[i * 2 + static_cast<uint32_t>(render_index)]
                .color.view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
  }
  set_write_image_info[IMAGE_TEXTURES + OFFSCREEN_BUFFERS] =
      (VkDescriptorImageInfo){
          .sampler =
              render_data_.images[IMAGE_TEXTURES + OFFSCREEN_BUFFERS].sampler,
          .imageView =
              render_data_.images[IMAGE_TEXTURES + OFFSCREEN_BUFFERS].view,
          .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      };

  VkWriteDescriptorSet set_write[3] = {
      {
          .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
          .pNext = nullptr,
          .dstSet = render_data_.main_desc_set,
          .dstBinding = 0,
          .dstArrayElement = 0,
          .descriptorCount = IMAGE_TEXTURES,
          .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
          .pImageInfo = &set_write_image_info[0],
          .pBufferInfo = nullptr,
          .pTexelBufferView = nullptr,
      },
      {
          .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
          .pNext = nullptr,
          .dstSet = render_data_.main_desc_set,
          .dstBinding = IMAGE_TEXTURES,
          .dstArrayElement = 0,
          .descriptorCount = OFFSCREEN_BUFFERS,
          .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
          .pImageInfo = &set_write_image_info[IMAGE_TEXTURES],
          .pBufferInfo = nullptr,
          .pTexelBufferView = nullptr,
      },
      {
          .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
          .pNext = nullptr,
          .dstSet = render_data_.main_desc_set,
          .dstBinding = IMAGE_TEXTURES + OFFSCREEN_BUFFERS,
          .dstArrayElement = 0,
          .descriptorCount = iKeyboard,
          .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
          .pImageInfo =
              &set_write_image_info[IMAGE_TEXTURES + OFFSCREEN_BUFFERS],
          .pBufferInfo = nullptr,
          .pTexelBufferView = nullptr,
      },
  };
  d.vkUpdateDescriptorSets(dev->device, 3, set_write, 0, nullptr);

  d.vkCmdBindDescriptorSets(essentials_.cmd_buffer,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            render_data_.main_layout.pipeline_layout, 0, 1,
                            &render_data_.main_desc_set, 0, nullptr);

  VkDeviceSize vertices_offset = 0;
  d.vkCmdBindVertexBuffers(essentials_.cmd_buffer, 0, 1,
                           &render_data_.buffers[BUFFER_VERTICES].buffer,
                           &vertices_offset);
  d.vkCmdBindIndexBuffer(essentials_.cmd_buffer,
                         render_data_.buffers[BUFFER_INDICES].buffer, 0,
                         VK_INDEX_TYPE_UINT16);

  VkViewport viewport = {
      .x = 0,
      .y = 0,
      .width = static_cast<float>(
          render_data_.main_gbuffers[image_index].surface_size.width),
      .height = static_cast<float>(
          render_data_.main_gbuffers[image_index].surface_size.height),
      .minDepth = 0,
      .maxDepth = 1,
  };
  d.vkCmdSetViewport(essentials_.cmd_buffer, 0, 1, &viewport);

  VkRect2D scissor = {
      .offset =
          {
              .x = 0,
              .y = 0,
          },
      .extent = render_data_.main_gbuffers[image_index].surface_size,
  };
  d.vkCmdSetScissor(essentials_.cmd_buffer, 0, 1, &scissor);

  d.vkCmdPushConstants(
      essentials_.cmd_buffer, render_data_.main_layout.pipeline_layout,
      VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof render_data_.push_constants,
      &render_data_.push_constants);

  // vkCmdDraw(essentials.cmd_buffer, 3, 1, 0, 0);
  d.vkCmdDrawIndexed(essentials_.cmd_buffer, 3, 1, 0, 0, 0);

  d.vkCmdEndRenderPass(essentials_.cmd_buffer);

  result = finish(&essentials_, dev, swapchain,
                  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, image_index,
                  wait_buf_sem_, wait_main_sem_);

  if (result == VK_ERROR_OUT_OF_DATE_KHR) {
    os_window->resize_event = true;
    result = 0;
  } else if (result == VK_ERROR_SURFACE_LOST_KHR) {
    d.vkDestroySurfaceKHR(vk_, swapchain->surface, nullptr);
    retval = create_surface(vk_, &swapchain->surface, os_window);
    if (!vk_error_is_success(&retval))
      return false;
    os_window->resize_event = true;
    result = 0;
  }

  if (result)
    return false;

  if (screenshot_once_) {
    screenshot_once_ = false;
    retval = make_screenshot(phy_dev, dev, swapchain, &essentials_,
                             &render_data_, image_index);
    if (!vk_error_is_success(&retval))
      return false;
  }

  update_params(&os_window->app_data, os_window->fps_lock);
  render_index = (render_index + 1) % 2;
  return true;
}

vk_error ShaderToy::transition_images_screenshot_swapchain_begin(
    vk_device* dev,
    vk_render_essentials* essentials,
    vk_image* srcImage,
    vk_image* dstImage) {
  vk_error retval = VK_ERROR_NONE;
  VkResult res;

  d.vkResetCommandBuffer(essentials->cmd_buffer, 0);

  VkCommandBufferBeginInfo begin_info{};
  begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  res = d.vkBeginCommandBuffer(essentials->cmd_buffer, &begin_info);
  vk_error_set_vkresult(&retval, res);
  if (res) {
    vk_error_printf(
        &retval,
        "Couldn't begin recording a command buffer to screenshot image\n");
    return retval;
  }

  VkImageMemoryBarrier image_barrier_dstImage{};
  image_barrier_dstImage.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  image_barrier_dstImage.srcAccessMask = 0;
  image_barrier_dstImage.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  image_barrier_dstImage.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  image_barrier_dstImage.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  image_barrier_dstImage.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  image_barrier_dstImage.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  image_barrier_dstImage.image = dstImage->image;
  image_barrier_dstImage.subresourceRange.aspectMask =
      VK_IMAGE_ASPECT_COLOR_BIT;
  image_barrier_dstImage.subresourceRange.baseMipLevel = 0;
  image_barrier_dstImage.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
  image_barrier_dstImage.subresourceRange.baseArrayLayer = 0;
  image_barrier_dstImage.subresourceRange.layerCount =
      VK_REMAINING_ARRAY_LAYERS;

  d.vkCmdPipelineBarrier(essentials->cmd_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                         nullptr, 1, &image_barrier_dstImage);

  VkImageMemoryBarrier image_barrier_srcImage{};
  image_barrier_srcImage.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  image_barrier_srcImage.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
  image_barrier_srcImage.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
  image_barrier_srcImage.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
  image_barrier_srcImage.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  image_barrier_srcImage.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  image_barrier_srcImage.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  image_barrier_srcImage.image = srcImage->image;
  image_barrier_srcImage.subresourceRange.aspectMask =
      VK_IMAGE_ASPECT_COLOR_BIT;
  image_barrier_srcImage.subresourceRange.baseMipLevel = 0;
  image_barrier_srcImage.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
  image_barrier_srcImage.subresourceRange.baseArrayLayer = 0;
  image_barrier_srcImage.subresourceRange.layerCount =
      VK_REMAINING_ARRAY_LAYERS;

  d.vkCmdPipelineBarrier(essentials->cmd_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                         nullptr, 1, &image_barrier_srcImage);

  d.vkEndCommandBuffer(essentials->cmd_buffer);

  res = d.vkResetFences(dev->device, 1, &essentials->exec_fence);
  vk_error_set_vkresult(&retval, res);
  if (res) {
    vk_error_printf(&retval, "Failed to reset fence\n");
    return retval;
  }

  VkSubmitInfo submit_info{};
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = &essentials->cmd_buffer;

  d.vkQueueSubmit(essentials->present_queue, 1, &submit_info,
                  essentials->exec_fence);
  res = d.vkWaitForFences(dev->device, 1, &essentials->exec_fence, true,
                          1000000000);
  vk_error_set_vkresult(&retval, res);
  if (res) {
    vk_error_printf(&retval, "Failed to reset fence\n");
    return retval;
  }

  return retval;
}

vk_error ShaderToy::transition_images_screenshot_swapchain_end(
    struct vk_device* dev,
    struct vk_render_essentials* essentials,
    struct vk_image* srcImage,
    struct vk_image* dstImage) {
  vk_error retval = VK_ERROR_NONE;
  VkResult res;

  d.vkResetCommandBuffer(essentials->cmd_buffer, 0);

  VkCommandBufferBeginInfo begin_info{};
  begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  res = d.vkBeginCommandBuffer(essentials->cmd_buffer, &begin_info);
  vk_error_set_vkresult(&retval, res);
  if (res) {
    vk_error_printf(
        &retval,
        "Couldn't begin recording a command buffer to screenshot image\n");
    return retval;
  }

  VkImageMemoryBarrier image_barrier_dstImage{};
  image_barrier_dstImage.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  image_barrier_dstImage.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  image_barrier_dstImage.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
  image_barrier_dstImage.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  image_barrier_dstImage.newLayout = VK_IMAGE_LAYOUT_GENERAL;
  image_barrier_dstImage.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  image_barrier_dstImage.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  image_barrier_dstImage.image = dstImage->image;
  image_barrier_dstImage.subresourceRange.aspectMask =
      VK_IMAGE_ASPECT_COLOR_BIT;
  image_barrier_dstImage.subresourceRange.baseMipLevel = 0;
  image_barrier_dstImage.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
  image_barrier_dstImage.subresourceRange.baseArrayLayer = 0;
  image_barrier_dstImage.subresourceRange.layerCount =
      VK_REMAINING_ARRAY_LAYERS;

  d.vkCmdPipelineBarrier(essentials->cmd_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                         nullptr, 1, &image_barrier_dstImage);

  VkImageMemoryBarrier image_barrier_srcImage{};
  image_barrier_srcImage.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  image_barrier_srcImage.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
  image_barrier_srcImage.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
  image_barrier_srcImage.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  image_barrier_srcImage.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
  image_barrier_srcImage.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  image_barrier_srcImage.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  image_barrier_srcImage.image = srcImage->image;
  image_barrier_srcImage.subresourceRange.aspectMask =
      VK_IMAGE_ASPECT_COLOR_BIT;
  image_barrier_srcImage.subresourceRange.baseMipLevel = 0;
  image_barrier_srcImage.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
  image_barrier_srcImage.subresourceRange.baseArrayLayer = 0;
  image_barrier_srcImage.subresourceRange.layerCount =
      VK_REMAINING_ARRAY_LAYERS;

  d.vkCmdPipelineBarrier(essentials->cmd_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                         nullptr, 1, &image_barrier_srcImage);

  d.vkEndCommandBuffer(essentials->cmd_buffer);

  res = d.vkResetFences(dev->device, 1, &essentials->exec_fence);
  vk_error_set_vkresult(&retval, res);
  if (res) {
    vk_error_printf(&retval, "Failed to reset fence\n");
    return retval;
  }

  VkSubmitInfo submit_info{};
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = &essentials->cmd_buffer;

  d.vkQueueSubmit(essentials->present_queue, 1, &submit_info,
                  essentials->exec_fence);
  res = d.vkWaitForFences(dev->device, 1, &essentials->exec_fence, true,
                          1000000000);
  vk_error_set_vkresult(&retval, res);
  if (res) {
    vk_error_printf(&retval, "Failed to reset fence\n");
    return retval;
  }

  return retval;
}

// RGBA BMP from https://en.wikipedia.org/wiki/BMP_file_format
unsigned char ShaderToy::ev(const int32_t v) {
  static uint32_t counter = 0;
  return static_cast<unsigned char>(v >> 8 * counter++ % 32);
}

void ShaderToy::write_bmp(const uint32_t w,
                          const uint32_t h,
                          const uint8_t* rgba) {
  static int scr_id = 0;

  if (!rgba) {
    return;
  }

  const auto img = static_cast<unsigned char*>(malloc(4 * w * h));
  memset(img, 0, 4 * w * h);

  for (unsigned int x = 0; x < w; x++) {
    for (unsigned int y = 0; y < h; y++) {
      img[(x + y * w) * 4 + 3] = rgba[(x + (h - 1 - y) * w) * 4 + 0];
      img[(x + y * w) * 4 + 2] = rgba[(x + (h - 1 - y) * w) * 4 + 1];
      img[(x + y * w) * 4 + 1] = rgba[(x + (h - 1 - y) * w) * 4 + 2];
      img[(x + y * w) * 4 + 0] = rgba[(x + (h - 1 - y) * w) * 4 + 3];
    }
  }

  const auto filesize = static_cast<int32_t>(108 + 14 + 4 * w * h);
  const unsigned char bmp_file_header[14] = {'B',
                                             'M',
                                             ev(filesize),
                                             ev(filesize),
                                             ev(filesize),
                                             ev(filesize),
                                             0,
                                             0,
                                             0,
                                             0,
                                             108 + 14,
                                             0,
                                             0,
                                             0};

  const unsigned char bmp_info_header[108] = {
      108,
      0,
      0,
      0,
      ev(static_cast<int32_t>(w)),
      ev(static_cast<int32_t>(w)),
      ev(static_cast<int32_t>(w)),
      ev(static_cast<int32_t>(w)),
      ev(-static_cast<int32_t>(h)),
      ev(-static_cast<int32_t>(h)),
      ev(-static_cast<int32_t>(h)),
      ev(-static_cast<int32_t>(h)),
      1,
      0,
      32,
      0,
      3,
      0,
      0,
      0,
      ev(static_cast<int32_t>(w * h * 4)),
      ev(static_cast<int32_t>(w * h * 4)),
      ev(static_cast<int32_t>(w * h * 4)),
      ev(static_cast<int32_t>(w * h * 4)),
      ev(0x0b13),
      ev(0x0b13),
      ev(0x0b13),
      ev(0x0b13),
      ev(0x0b13),
      ev(0x0b13),
      ev(0x0b13),
      ev(0x0b13),
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0xff,
      0,
      0,
      0xff,
      0,
      0,
      0xff,
      0,
      0,
      0xff,
      0,
      0,
      0,
      ev(0x57696E20),
      ev(0x57696E20),
      ev(0x57696E20),
      ev(0x57696E20),
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
  };

  std::ostringstream ss;
  ss << "screenshot_" << scr_id++ << ".bmp";
  const auto f = fopen(ss.str().c_str(), "wb");
  fwrite(bmp_file_header, 1, 14, f);
  fwrite(bmp_info_header, 1, 108, f);
  for (uint32_t i = 0; i < h; i++) {
    fwrite(img + (w * (h - i - 1) * 4), 4, w, f);
  }

  free(img);
  fclose(f);
}

vk_error ShaderToy::make_screenshot(vk_physical_device* phy_dev,
                                    vk_device* dev,
                                    vk_swapchain* swapchain,
                                    vk_render_essentials* essentials,
                                    render_data* render_data,
                                    uint32_t image_index) {
  auto retval = VK_ERROR_NONE;

  if (!essentials->first_render) {
    auto res = d.vkWaitForFences(dev->device, 1, &essentials->exec_fence, true,
                                 1000000000);
    vk_error_set_vkresult(&retval, res);
    if (res) {
      vk_error_printf(&retval, "Wait for fence failed\n");
      return retval;
    }
  }

  uint32_t support_format_list[4] = {
      VK_FORMAT_B8G8R8A8_SRGB, VK_FORMAT_B8G8R8A8_UNORM,
      VK_FORMAT_R8G8B8A8_SRGB, VK_FORMAT_R8G8B8A8_UNORM};
  bool supported = false;
  for (int i = 0; (i < 4) && (!supported); i++) {
    if (swapchain->surface_format.format == support_format_list[i])
      supported = true;
  }
  supported &= swapchain->surface_caps.supportedUsageFlags &
               VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

  if (!supported) {
    vk_error_printf(
        &retval,
        "Can not save screenshot, surface has unique format or not "
        "supported transfer %lu\n",
        static_cast<unsigned long>(swapchain->surface_format.format));
    return retval;
  }

  vk_image srcImage{};
  srcImage.image = essentials->images[image_index];

  vk_image dstImage{};
  dstImage.format = VK_FORMAT_R8G8B8A8_UNORM;  // VK_FORMAT_R8G8B8A8_SRGB
  dstImage.extent = render_data->main_gbuffers[image_index].surface_size;
  dstImage.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  dstImage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  dstImage.make_view = false;
  dstImage.host_visible = true;
  dstImage.anisotropyEnable = true;
  dstImage.repeat_mode =
      VK_SAMPLER_ADDRESS_MODE_REPEAT;  // VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER
                                       // //VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE
  dstImage.mipmaps = false;
  dstImage.linear = true;

  retval = create_images(phy_dev, dev, &dstImage, 1);
  if (!vk_error_is_success(&retval)) {
    vk_error_printf(&retval, "Failed to create dstImage for screenshot\n");
    return retval;
  }

  retval = transition_images_screenshot_swapchain_begin(dev, essentials,
                                                        &srcImage, &dstImage);
  if (!vk_error_is_success(&retval)) {
    return retval;
  }

  VkImageCopy imageCopyRegion{};
  imageCopyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  imageCopyRegion.srcSubresource.layerCount = 1;
  imageCopyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  imageCopyRegion.dstSubresource.layerCount = 1;
  imageCopyRegion.extent.width = dstImage.extent.width;
  imageCopyRegion.extent.height = dstImage.extent.height;
  imageCopyRegion.extent.depth = 1;

  retval = copy_image(dev, essentials, &dstImage,
                      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &srcImage,
                      VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, &imageCopyRegion,
                      "screenshot");
  if (!vk_error_is_success(&retval)) {
    vk_error_printf(&retval, "Failed to copy image for screenshot\n");
    return retval;
  }

  retval = transition_images_screenshot_swapchain_end(dev, essentials,
                                                      &srcImage, &dstImage);
  if (!vk_error_is_success(&retval)) {
    return retval;
  }

  VkImageSubresource subResource{};
  subResource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

  VkSubresourceLayout subResourceLayout;
  d.vkGetImageSubresourceLayout(dev->device, dstImage.image, &subResource,
                                &subResourceLayout);

  uint8_t* data;
  d.vkMapMemory(dev->device, dstImage.image_mem, 0, VK_WHOLE_SIZE, 0,
                reinterpret_cast<void**>(&data));
  data += subResourceLayout.offset;

  int color_order[3] = {0, 1, 2};

  if (swapchain->surface_format.format == VK_FORMAT_B8G8R8A8_SRGB ||
      swapchain->surface_format.format == VK_FORMAT_B8G8R8A8_UNORM) {
    color_order[0] = 2;
    color_order[1] = 1;
    color_order[2] = 0;
  }

  uint8_t* data_rgba;
  data_rgba = static_cast<uint8_t*>(
      malloc(4 * dstImage.extent.width * dstImage.extent.height));
  for (uint32_t y = 0; y < dstImage.extent.height; y++) {
    auto row = data;
    for (uint32_t x = 0; x < dstImage.extent.width; x++) {
      data_rgba[(x + y * dstImage.extent.width) * 4 + 0] =
          row[x * 4 + static_cast<uint32_t>(color_order[0])];
      data_rgba[(x + y * dstImage.extent.width) * 4 + 1] =
          row[x * 4 + static_cast<uint32_t>(color_order[1])];
      data_rgba[(x + y * dstImage.extent.width) * 4 + 2] =
          row[x * 4 + static_cast<uint32_t>(color_order[2])];
      data_rgba[(x + y * dstImage.extent.width) * 4 + 3] = row[x * 4 + 3];
    }
    data += subResourceLayout.rowPitch;
  }

  write_bmp(dstImage.extent.width, dstImage.extent.height, data_rgba);

  spdlog::info("screenshot done");

  free(data_rgba);
  d.vkUnmapMemory(dev->device, dstImage.image_mem);
  free_images(dev, &dstImage, 1);

  return retval;
}

void ShaderToy::draw_frame(const uint32_t /* time */) {
  render_loop_draw(&phy_dev_, &dev_, &swapchain_, &os_window_);
}
