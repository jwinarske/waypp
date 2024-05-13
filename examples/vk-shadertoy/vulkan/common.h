
#ifndef _LAUNCHER_VULKAN_COMMON_H_
#define _LAUNCHER_VULKAN_COMMON_H_

#define S1(x) #x
#define S2(x) S1(x)
#define LOCATION __FILE__ " : " S2(__LINE__)

#define CHECK_VK_RESULT(x)                                 \
  do {                                                     \
    vk::resultCheck(static_cast<vk::Result>(x), LOCATION); \
  } while (0)


#define VULKAN_HPP_NO_EXCEPTIONS 1
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_to_string.hpp>
#include <vulkan/vulkan_static_assertions.hpp>

#include "vulkan/vk_struct.h"
#include "vulkan/vk_error_print.h"


#endif // _LAUNCHER_VULKAN_COMMON_H_
