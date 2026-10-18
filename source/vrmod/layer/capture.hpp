#pragma once
#include <evr/vulkan.hpp>
#include <fstream>
#include <vector>
// Diagnostic single-frame readback, only requested for transfer-readable game swapchains.
// The incoming present semaphores are consumed exactly once by the readback submit.
inline bool capturePresent(evr::Vulkan &v, VkQueue queue, VkImage image, VkFormat format, uint32_t width,
                           uint32_t height, const VkPresentInfoKHR *present, const std::string &path, bool &consumed)
{
    VkBuffer buffer{};
    VkDeviceMemory memory{};
    VkCommandPool pool{};
    VkFence fence{};
    bool submitted = false;
    consumed = false;
    auto cleanup = [&]() {
        if (submitted)
            v.vkQueueWaitIdle(queue);
        if (fence)
            v.vkDestroyFence(v.device, fence, nullptr);
        if (pool)
            v.vkDestroyCommandPool(v.device, pool, nullptr);
        if (buffer)
            v.vkDestroyBuffer(v.device, buffer, nullptr);
        if (memory)
            v.vkFreeMemory(v.device, memory, nullptr);
    };
    try
    {
        VkDeviceSize bytes = VkDeviceSize(width) * height * 4;
        VkBufferCreateInfo bc{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bc.size = bytes;
        bc.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        evr::vkCheck(v.vkCreateBuffer(v.device, &bc, nullptr, &buffer), "capture buffer");
        VkMemoryRequirements req{};
        v.vkGetBufferMemoryRequirements(v.device, buffer, &req);
        memory = v.allocate(req, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        evr::vkCheck(v.vkBindBufferMemory(v.device, buffer, memory, 0), "capture bind");
        VkCommandPoolCreateInfo pc{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        pc.queueFamilyIndex = v.family;
        evr::vkCheck(v.vkCreateCommandPool(v.device, &pc, nullptr, &pool), "capture pool");
        VkCommandBufferAllocateInfo cb{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        cb.commandPool = pool;
        cb.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cb.commandBufferCount = 1;
        VkCommandBuffer command{};
        evr::vkCheck(v.vkAllocateCommandBuffers(v.device, &cb, &command), "capture command");
        VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        evr::vkCheck(v.vkBeginCommandBuffer(command, &begin), "capture begin");
        VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        b.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
        b.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        b.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        b.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.image = image;
        b.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        v.vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0,
                               nullptr, 0, nullptr, 1, &b);
        VkBufferImageCopy copy{};
        copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        copy.imageExtent = {width, height, 1};
        v.vkCmdCopyImageToBuffer(command, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buffer, 1, &copy);
        b.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        b.dstAccessMask = 0;
        b.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        b.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        v.vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0,
                               nullptr, 0, nullptr, 1, &b);
        evr::vkCheck(v.vkEndCommandBuffer(command), "capture end");
        VkFenceCreateInfo fc{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        evr::vkCheck(v.vkCreateFence(v.device, &fc, nullptr, &fence), "capture fence");
        std::vector<VkPipelineStageFlags> stages(present->waitSemaphoreCount, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
        VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submit.waitSemaphoreCount = present->waitSemaphoreCount;
        submit.pWaitSemaphores = present->pWaitSemaphores;
        submit.pWaitDstStageMask = stages.data();
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &command;
        evr::vkCheck(v.vkQueueSubmit(queue, 1, &submit, fence), "capture submit");
        submitted = consumed = true;
        evr::vkCheck(v.vkWaitForFences(v.device, 1, &fence, VK_TRUE, 5000000000), "capture fence wait");
        void *data{};
        evr::vkCheck(v.vkMapMemory(v.device, memory, 0, bytes, 0, &data), "capture map");
        auto *p = static_cast<unsigned char *>(data);
        std::vector<unsigned char> rgb(size_t(width) * height * 3);
        bool bgra = format == VK_FORMAT_B8G8R8A8_UNORM || format == VK_FORMAT_B8G8R8A8_SRGB;
        for (size_t i = 0; i < rgb.size() / 3; i++)
        {
            rgb[3 * i] = p[4 * i + (bgra ? 2 : 0)];
            rgb[3 * i + 1] = p[4 * i + 1];
            rgb[3 * i + 2] = p[4 * i + (bgra ? 0 : 2)];
        }
        v.vkUnmapMemory(v.device, memory);
        std::ofstream out(path, std::ios::binary);
        out << "P6\n" << width << ' ' << height << "\n255\n";
        out.write(reinterpret_cast<const char *>(rgb.data()), rgb.size());
        if (!out)
            throw std::runtime_error("capture file write failed");
        cleanup();
        return true;
    }
    catch (...)
    {
        cleanup();
        return false;
    }
}
