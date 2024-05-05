
#pragma once

#define S1(x) #x
#define S2(x) S1(x)
#define LOCATION __FILE__ " : " S2(__LINE__)

#define CHECK_VK_RESULT(x)                                 \
  do {                                                     \
    vk::resultCheck(static_cast<vk::Result>(x), LOCATION); \
  } while (0)

static uint32_t find_memory(const VkPhysicalDeviceMemoryProperties &mem_props, VkMemoryRequirements &mem_req,
                            VkMemoryPropertyFlags properties) {
    for (uint32_t i = 0; i < mem_props.memoryTypeCount; ++i) {
        if ((mem_req.memoryTypeBits & 1 << i) == 0)
            continue;
        if (mem_props.memoryHeaps[mem_props.memoryTypes[i].heapIndex].size < mem_req.size)
            continue;
        if ((mem_props.memoryTypes[i].propertyFlags & properties) == properties)
            return i;
    }
    return mem_props.memoryTypeCount;
}
