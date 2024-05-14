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

// Danil, 2021+ Vulkan shader launcher, self https://github.com/danilw/vulkan-shadertoy-launcher
// The MIT License

#ifndef EXAMPLES_VK_SHADERTOY_VULKAN_VK_STRUCT_H_
#define EXAMPLES_VK_SHADERTOY_VULKAN_VK_STRUCT_H_

static constexpr uint32_t kMaxQueueFamily = UINT32_C(10);
static constexpr uint32_t kMaxPresentModes = UINT32_C(4);
static constexpr uint32_t kAppNameStrLen = UINT32_C(80);

// numbers buffers <*.frag> files
// number of buffers to create, any number(>0), if you need 0 use https://github.com/danilw/vulkan-shader-launcher
// names shaders/spv/<file>.spv look files names in that folder
static constexpr uint32_t OFFSCREEN_BUFFERS = UINT32_C(4);

// number of images(>0)
// names textures/<X>.png X start from 1
static constexpr uint32_t IMAGE_TEXTURES = UINT32_C(4);

// linear or mipmap for textures
static constexpr bool USE_MIPMAPS = true;

// do not edit, it just to see where keyboard texture used
static constexpr uint32_t iKeyboard = UINT32_C(1);


struct vk_physical_device {
    VkPhysicalDevice physical_device;
    VkPhysicalDeviceProperties properties;
    VkPhysicalDeviceFeatures features;
    VkPhysicalDeviceMemoryProperties memories;
    VkQueueFamilyProperties queue_families[kMaxQueueFamily];
    uint32_t queue_family_count;
    bool queue_families_incomplete;
};

struct vk_commands {
    VkQueueFlags qflags;

    VkCommandPool pool;
    VkQueue *queues;
    uint32_t queue_count;
    VkCommandBuffer *buffers;
    uint32_t buffer_count;
};

struct vk_device {
    VkDevice device;
    struct vk_commands *command_pools;
    uint32_t command_pool_count;
};

struct vk_swapchain {
    VkSurfaceKHR surface;
    VkSwapchainKHR swapchain;
    VkSurfaceFormatKHR surface_format;
    VkSurfaceCapabilitiesKHR surface_caps;
    VkPresentModeKHR present_modes[kMaxPresentModes];
    uint32_t present_modes_count;
};

struct vk_image {
    VkFormat format;
    VkExtent2D extent;
    VkImageUsageFlagBits usage;
    VkShaderStageFlagBits stage;
    bool make_view;
    bool will_be_initialized;
    bool host_visible;
    bool multisample;
    uint32_t *sharing_queues;
    uint32_t sharing_queue_count;
    VkImage image;
    VkDeviceMemory image_mem;
    VkImageView view;
    VkSampler sampler;
    bool anisotropyEnable;
    VkSamplerAddressMode repeat_mode;
    bool mipmaps;
    bool linear;
};

struct vk_buffer {
    VkFormat format;
    uint32_t size;
    VkBufferUsageFlagBits usage;
    VkShaderStageFlagBits stage;
    bool make_view;
    bool host_visible;
    uint32_t *sharing_queues;
    uint32_t sharing_queue_count;
    VkBuffer buffer;
    VkDeviceMemory buffer_mem;
    VkBufferView view;
};

struct vk_shader {
    const char *spirv_file;
    VkShaderStageFlagBits stage;
    VkShaderModule shader;
};

struct vk_graphics_buffers {
    VkExtent2D surface_size;
    VkImage swapchain_image;
    VkImageView color_view;
    struct vk_image depth;
    VkFramebuffer framebuffer;
};

struct vk_render_essentials {
    VkImage *images;
    uint32_t image_count;
    VkQueue present_queue;
    VkCommandBuffer cmd_buffer;

    VkSemaphore sem_post_acquire;
    VkSemaphore sem_pre_submit;

    VkFence exec_fence;
    bool first_render;
};

struct vk_resources {
    struct vk_image *images;
    uint32_t image_count;
    struct vk_buffer *buffers;
    uint32_t buffer_count;
    struct vk_shader *shaders;
    uint32_t shader_count;
    VkPushConstantRange *push_constants;
    uint32_t push_constant_count;
    VkRenderPass render_pass;
};

struct vk_layout {
    struct vk_resources *resources;
    VkDescriptorSetLayout set_layout;
    VkPipelineLayout pipeline_layout;
};

struct vk_pipeline {
    struct vk_layout *layout;
    VkPipelineVertexInputStateCreateInfo vertex_input_state;
    VkPipelineInputAssemblyStateCreateInfo input_assembly_state;
    VkPipelineTessellationStateCreateInfo tessellation_state;
    uint32_t thread_count;
    VkPipeline pipeline;
    VkDescriptorPool set_pool;
};

enum vk_render_pass_load_op {
    VK_C_CLEAR = 0,
    VK_KEEP = 1,
};

enum vk_make_depth_buffer {
    VK_WITHOUT_DEPTH = 0,
    VK_WITH_DEPTH = 1,
};

struct vk_offscreen_buffers {
    VkExtent2D surface_size;
    struct vk_image color;
    struct vk_image depth;
    VkFramebuffer framebuffer;
};


struct app_data_struct {
    int iResolution[2]; //resolution
    double iMouse[2]; //mouse in window, it always updated (not like iMouse on shadertoy)
    int iMouse_lclick[2]; //mouse left click pos (its -[last pos] when left mosue not clicked)
    int iMouse_rclick[2]; //mouse right click pos (its -[last pos] when right mosue not clicked)
    bool iMouse_click[2]; //is mouse button clicked(left/right)
    float iTime; //time
    float iTimeDelta; //time delta
    int iFrame; //frames

    bool pause; //pause clicked
    bool quit; //quit clicked/happend
    bool drawdebug; //draw debug info, key press
};

struct app_os_window {
#if defined(VK_USE_PLATFORM_WIN32_KHR)
    HINSTANCE connection;
    HWND window;
    POINT minsize;
#elif defined(VK_USE_PLATFORM_XCB_KHR)
    Display *display;
    xcb_connection_t *connection;
    xcb_screen_t *screen;
    xcb_window_t xcb_window;
    xcb_intern_atom_reply_t *atom_wm_delete_window;
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
    struct wl_display *wl_display;
    struct wl_registry *registry;
    struct wl_compositor *compositor;
    struct wl_surface *wl_surface;
    struct xdg_wm_base *shell;
    struct wl_seat *seat;
    struct wl_pointer *pointer;
    struct wl_keyboard *keyboard;
    struct xdg_surface *xdg_surface;
    struct xdg_toplevel *xdg_toplevel;
    bool configured;
#endif
    char name[kAppNameStrLen];

    bool prepared; // is vk setup prepared
    bool is_minimized; //window controled events
    bool resize_event; //window controled events
    bool fps_lock; //key pressed event
    bool reload_shaders_on_resize; //launch option
    bool enable_debug; //launch option

    bool pause_refresh; //used only in Windows, on pause when key pressed refresh

    VkPresentModeKHR present_mode;
    struct app_data_struct app_data;
};

struct shaders_push_constants {
    float iMouse[4];
    float iDate[4];
    int iMouse_lr[2];
    float iResolution[2];
    int debugdraw; // look function check_hotkeys
    int pCustom; //custom data
    float iTime;
    float iTimeDelta;
    int iFrame;
};

enum {
    BUFFER_VERTICES = 0,
    BUFFER_INDICES = 1,
};
enum {
    SHADER_MAIN_VERTEX = 0,
    SHADER_MAIN_FRAGMENT = 1,
};

struct render_data {
    struct objects {
        struct vertex {
            float pos[3];
        } vertices[3];

        uint16_t indices[3];
    } objects;

    struct shaders_push_constants push_constants;

    struct vk_image images[IMAGE_TEXTURES + OFFSCREEN_BUFFERS + iKeyboard];
    struct vk_buffer buffers[2];
    struct vk_shader shaders[2 + OFFSCREEN_BUFFERS * 2];
    struct vk_graphics_buffers *main_gbuffers;
    struct vk_offscreen_buffers *buf_obuffers;

    VkRenderPass buf_render_pass[OFFSCREEN_BUFFERS];
    struct vk_layout buf_layout[OFFSCREEN_BUFFERS];
    struct vk_pipeline buf_pipeline[OFFSCREEN_BUFFERS];
    VkDescriptorSet buf_desc_set[OFFSCREEN_BUFFERS];

    VkRenderPass main_render_pass;
    struct vk_layout main_layout;
    struct vk_pipeline main_pipeline;
    VkDescriptorSet main_desc_set;
};


#endif // EXAMPLES_VK_SHADERTOY_VULKAN_VK_STRUCT_H_
