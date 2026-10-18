// Tiny host process used to verify the layer's optional creation preparation/fallback.
// It creates no XR instance itself, so the single-instance Mock runtime is sufficient.
#include <evr/vulkan.hpp>
#include <iostream>
using namespace evr;
int main()
{
    try
    {
        {
        Vulkan host;
        VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
        app.pApplicationName = "EVR graphics preflight fixture";
        app.apiVersion = VK_API_VERSION_1_2;
        VkInstanceCreateInfo instanceInfo{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
        instanceInfo.pApplicationInfo = &app;
        VkInstance instance{};
        vkCheck(host.vkCreateInstance(&instanceInfo, nullptr, &instance), "fixture original instance creation");
        std::cout<<"fixture instance "<<instance<<std::endl;
        host.owned = true; host.bindInstance(instance);
        uint32_t count = 0;
        vkCheck(host.vkEnumeratePhysicalDevices(instance, &count, nullptr), "fixture GPU count");
        std::vector<VkPhysicalDevice> gpus(count);
        vkCheck(host.vkEnumeratePhysicalDevices(instance, &count, gpus.data()), "fixture GPU enumeration");
        if (gpus.empty()) throw std::runtime_error("fixture GPU absent");
        VkPhysicalDevice physical = gpus[0];
        std::cout<<"fixture GPU "<<physical<<std::endl;
        host.vkGetPhysicalDeviceQueueFamilyProperties(physical, &count, nullptr);
        std::vector<VkQueueFamilyProperties> queues(count);
        host.vkGetPhysicalDeviceQueueFamilyProperties(physical, &count, queues.data());
        uint32_t family = 0;
        while (family < count && !(queues[family].queueFlags & VK_QUEUE_GRAPHICS_BIT)) ++family;
        if (family == count) throw std::runtime_error("fixture graphics queue absent");
        float priority = .75f;
        VkDeviceQueueCreateInfo queue{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
        queue.queueFamilyIndex = family; queue.queueCount = 1; queue.pQueuePriorities = &priority;
        VkPhysicalDeviceVulkan12Features features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
        features.timelineSemaphore = VK_TRUE;
        VkDeviceCreateInfo info{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
        info.pNext = &features; info.queueCreateInfoCount = 1; info.pQueueCreateInfos = &queue;
        VkDevice device{};
        vkCheck(host.vkCreateDevice(physical, &info, nullptr, &device), "fixture original device creation");
        host.bindDevice(device, physical, family);
        if (info.pNext != &features || info.enabledExtensionCount || instanceInfo.enabledExtensionCount ||
            features.timelineSemaphore != VK_TRUE || features.pNext || queue.pQueuePriorities != &priority || priority != .75f)
            throw std::runtime_error("layer changed caller-owned creation inputs");
        VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bufferInfo.size = 64; bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        VkBuffer buffer{};
        vkCheck(host.vkCreateBuffer(device, &bufferInfo, nullptr, &buffer), "fixture device usable after preparation/fallback");
        host.vkDestroyBuffer(device, buffer, nullptr);
        auto createSemaphore = reinterpret_cast<PFN_vkCreateSemaphore>(host.vkGetDeviceProcAddr(device, "vkCreateSemaphore"));
        auto destroySemaphore = reinterpret_cast<PFN_vkDestroySemaphore>(host.vkGetDeviceProcAddr(device, "vkDestroySemaphore"));
        VkSemaphoreTypeCreateInfo type{VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO};
        type.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
        VkSemaphoreCreateInfo semaphoreInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        semaphoreInfo.pNext = &type;
        VkSemaphore semaphores[2]{};
        vkCheck(createSemaphore(device, &semaphoreInfo, nullptr, &semaphores[0]), "fixture timeline semaphore");
        semaphoreInfo.pNext = nullptr;
        vkCheck(createSemaphore(device, &semaphoreInfo, nullptr, &semaphores[1]), "fixture binary semaphore");
        uint64_t values[]{5, 0}; // The binary entry is ignored by Vulkan, never a timeline dependency.
        VkTimelineSemaphoreSubmitInfo timeline{VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO};
        timeline.signalSemaphoreValueCount = 2; timeline.pSignalSemaphoreValues = values;
        VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submit.pNext = &timeline; submit.signalSemaphoreCount = 2; submit.pSignalSemaphores = semaphores;
        vkCheck(host.vkQueueSubmit(host.queue, 1, &submit, VK_NULL_HANDLE), "fixture signal submission");
        timeline.signalSemaphoreValueCount = 0;
        timeline.waitSemaphoreValueCount = 2; timeline.pWaitSemaphoreValues = values;
        VkPipelineStageFlags stages[]{VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT};
        submit.signalSemaphoreCount = 0; submit.waitSemaphoreCount = 2;
        submit.pWaitSemaphores = semaphores; submit.pWaitDstStageMask = stages;
        vkCheck(host.vkQueueSubmit(host.queue, 1, &submit, VK_NULL_HANDLE), "fixture wait submission");
        vkCheck(host.vkQueueWaitIdle(host.queue), "fixture queue completed");
        for (auto semaphore : semaphores) destroySemaphore(device, semaphore, nullptr);
        if (auto expected = std::getenv("EVR_PREPARE_SESSION_EXPECTED"))
        {
            auto layer = GetModuleHandleW(L"evr_diagnostic.dll");
            using Run = int (*)(VkInstance, VkPhysicalDevice, VkDevice, uint32_t);
            auto run = layer ? reinterpret_cast<Run>(GetProcAddress(layer, "evrTestPreparedSession")) : nullptr;
            if (!run || run(instance, physical, device, family) != std::atoi(expected))
                throw std::runtime_error("prepared layer session fixture result");
            vkCheck(host.vkDeviceWaitIdle(device), "host survives prepared layer session");
        }
        }
        std::cout << "PASS original caller inputs preserved; device usable; explicit teardown complete\n";
        return 0;
    }
    catch (const std::exception &e) { std::cerr << "FAIL " << e.what() << '\n'; return 1; }
}
