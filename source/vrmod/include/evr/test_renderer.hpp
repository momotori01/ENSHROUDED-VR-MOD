#pragma once
#include <evr/openxr_client.hpp>
#include <filesystem>
namespace evr
{
struct Vertex
{
    Vec3 position, color;
};
class TestRenderer
{
    Vulkan &v;
    VkRenderPass pass{};
    VkPipelineLayout layout{};
    VkPipeline pipeline{};
    VkCommandPool pool{};
    VkCommandBuffer cmd{};
    VkFence fence{};
    VkBuffer vertices{};
    VkDeviceMemory vertexMemory{};
    uint32_t vertexCount = 0;
    struct Target
    {
        VkImage image{};
        VkImageView view{};
        VkFramebuffer framebuffer{};
        uint32_t width = 0, height = 0;
    };
    std::array<std::vector<Target>, 2> targets;
    VkFormat format;
    VkImageLayout finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  public:
    explicit TestRenderer(OpenXrClient &xr) : TestRenderer(xr.vk, xr.format, xr.targets) {}
    TestRenderer(Vulkan &vk, VkFormat colorFormat, const std::array<EyeTarget, 2> &eyeTargets,
                 VkImageLayout targetLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
        : v(vk), format(colorFormat), finalLayout(targetLayout)
    {
        try
        {
            init(eyeTargets);
        }
        catch (...)
        {
            destroy();
            throw;
        }
    }
    ~TestRenderer()
    {
        destroy();
    }
    void init(const std::array<EyeTarget, 2> &eyeTargets)
    {
        VkAttachmentDescription a{};
        a.format = format;
        a.samples = VK_SAMPLE_COUNT_1_BIT;
        a.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        a.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        a.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        a.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        a.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        a.finalLayout = finalLayout;
        VkAttachmentReference ar{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        VkSubpassDescription s{};
        s.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        s.colorAttachmentCount = 1;
        s.pColorAttachments = &ar;
        VkSubpassDependency dep{};
        dep.srcSubpass = VK_SUBPASS_EXTERNAL;
        dep.dstSubpass = 0;
        dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        VkRenderPassCreateInfo passInfo{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
        passInfo.attachmentCount = 1;
        passInfo.pAttachments = &a;
        passInfo.subpassCount = 1;
        passInfo.pSubpasses = &s;
        passInfo.dependencyCount = 1;
        passInfo.pDependencies = &dep;
        vkCheck(v.vkCreateRenderPass(v.device, &passInfo, nullptr, &pass), "render pass");
        for (int eye = 0; eye < 2; eye++)
            for (auto im : eyeTargets[eye].images)
            {
                Target t;
                t.image = im.image;
                t.width = eyeTargets[eye].width;
                t.height = eyeTargets[eye].height;
                targets[eye].push_back(t);
                auto &target = targets[eye].back();
                VkImageViewCreateInfo iv{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
                iv.image = t.image;
                iv.viewType = VK_IMAGE_VIEW_TYPE_2D;
                iv.format = format;
                iv.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
                vkCheck(v.vkCreateImageView(v.device, &iv, nullptr, &target.view), "eye image view");
                VkFramebufferCreateInfo fi{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
                fi.renderPass = pass;
                fi.attachmentCount = 1;
                fi.pAttachments = &target.view;
                fi.width = t.width;
                fi.height = t.height;
                fi.layers = 1;
                vkCheck(v.vkCreateFramebuffer(v.device, &fi, nullptr, &target.framebuffer), "eye framebuffer");
            }
        const uint32_t vertexShader[] =
#include "vert.spv"
            ;
        const uint32_t fragmentShader[] =
#include "frag.spv"
            ;
        VkShaderModule modules[2]{};
        VkShaderModuleCreateInfo sm{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        sm.codeSize = sizeof(vertexShader);
        sm.pCode = vertexShader;
        vkCheck(v.vkCreateShaderModule(v.device, &sm, nullptr, &modules[0]), "vertex shader");
        sm.codeSize = sizeof(fragmentShader);
        sm.pCode = fragmentShader;
        VkResult shaderResult = v.vkCreateShaderModule(v.device, &sm, nullptr, &modules[1]);
        if (shaderResult != VK_SUCCESS)
        {
            v.vkDestroyShaderModule(v.device, modules[0], nullptr);
            vkCheck(shaderResult, "fragment shader");
        }
        try
        {
            VkPushConstantRange push{VK_SHADER_STAGE_VERTEX_BIT, 0, 64};
            VkPipelineLayoutCreateInfo pl{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
            pl.pushConstantRangeCount = 1;
            pl.pPushConstantRanges = &push;
            vkCheck(v.vkCreatePipelineLayout(v.device, &pl, nullptr, &layout), "pipeline layout");
            VkPipelineShaderStageCreateInfo stages[2] = {{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO},
                                                         {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO}};
            for (int j = 0; j < 2; j++)
            {
                stages[j].stage = j ? VK_SHADER_STAGE_FRAGMENT_BIT : VK_SHADER_STAGE_VERTEX_BIT;
                stages[j].module = modules[j];
                stages[j].pName = "main";
            }
            VkVertexInputBindingDescription vb{0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX};
            VkVertexInputAttributeDescription attrs[2] = {{0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0},
                                                          {1, 0, VK_FORMAT_R32G32B32_SFLOAT, 12}};
            VkPipelineVertexInputStateCreateInfo vin{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
            vin.vertexBindingDescriptionCount = 1;
            vin.pVertexBindingDescriptions = &vb;
            vin.vertexAttributeDescriptionCount = 2;
            vin.pVertexAttributeDescriptions = attrs;
            VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
            ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
            vp.viewportCount = vp.scissorCount = 1;
            VkPipelineRasterizationStateCreateInfo rs{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
            rs.polygonMode = VK_POLYGON_MODE_FILL;
            rs.cullMode = VK_CULL_MODE_NONE;
            rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
            rs.lineWidth = 1;
            VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
            ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
            VkPipelineColorBlendAttachmentState ba{};
            ba.colorWriteMask = 15;
            VkPipelineColorBlendStateCreateInfo bs{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
            bs.attachmentCount = 1;
            bs.pAttachments = &ba;
            VkDynamicState dyn[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
            VkPipelineDynamicStateCreateInfo ds{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
            ds.dynamicStateCount = 2;
            ds.pDynamicStates = dyn;
            VkGraphicsPipelineCreateInfo gp{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
            gp.stageCount = 2;
            gp.pStages = stages;
            gp.pVertexInputState = &vin;
            gp.pInputAssemblyState = &ia;
            gp.pViewportState = &vp;
            gp.pRasterizationState = &rs;
            gp.pMultisampleState = &ms;
            gp.pColorBlendState = &bs;
            gp.pDynamicState = &ds;
            gp.layout = layout;
            gp.renderPass = pass;
            vkCheck(v.vkCreateGraphicsPipelines(v.device, VK_NULL_HANDLE, 1, &gp, nullptr, &pipeline),
                    "stereo test pipeline");
        }
        catch (...)
        {
            for (auto m : modules)
                v.vkDestroyShaderModule(v.device, m, nullptr);
            throw;
        }
        for (auto m : modules)
            v.vkDestroyShaderModule(v.device, m, nullptr);
        std::vector<Vertex> geometry;
        auto quad = [&](Vec3 a, Vec3 b, Vec3 c, Vec3 d, Vec3 color) {
            for (auto pt : {a, b, c, a, c, d})
                geometry.push_back({pt, color});
        };
        // A ring of colored pillars plus a tiled floor provides real binocular parallax.
        for (int j = 0; j < 16; j++)
        {
            float angle = j * 2 * pi / 16;
            Vec3 center{std::sin(angle) * (2.5f + (j % 3)), 1.5f, -std::cos(angle) * (2.5f + (j % 3))};
            float w = .22f, h = .65f;
            Vec3 color{.2f + .7f * (j % 3 == 0), .2f + .7f * (j % 3 == 1), .2f + .7f * (j % 3 == 2)};
            quad(center + Vec3{-w, -h, 0}, center + Vec3{w, -h, 0}, center + Vec3{w, h, 0}, center + Vec3{-w, h, 0},
                 color);
            quad(center + Vec3{0, -h, -w}, center + Vec3{0, -h, w}, center + Vec3{0, h, w}, center + Vec3{0, h, -w},
                 color * .7f);
        }
        for (int x = -10; x < 10; x++)
            for (int z = -10; z < 10; z++)
            {
                float c = ((x + z) & 1) ? .2f : .35f;
                quad({float(x), 0, float(z)}, {float(x + 1), 0, float(z)}, {float(x + 1), 0, float(z + 1)},
                     {float(x), 0, float(z + 1)}, {c, c, c});
                quad({float(x), 4, float(z)}, {float(x + 1), 4, float(z)}, {float(x + 1), 4, float(z + 1)},
                     {float(x), 4, float(z + 1)}, {c * .6f, c * .8f, c});
            }
        vertexCount = uint32_t(geometry.size());
        VkBufferCreateInfo bc{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bc.size = geometry.size() * sizeof(Vertex);
        bc.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        vkCheck(v.vkCreateBuffer(v.device, &bc, nullptr, &vertices), "vertices");
        VkMemoryRequirements req{};
        v.vkGetBufferMemoryRequirements(v.device, vertices, &req);
        vertexMemory = v.allocate(req, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        vkCheck(v.vkBindBufferMemory(v.device, vertices, vertexMemory, 0), "bind vertices");
        void *p;
        vkCheck(v.vkMapMemory(v.device, vertexMemory, 0, bc.size, 0, &p), "map vertices");
        std::memcpy(p, geometry.data(), size_t(bc.size));
        v.vkUnmapMemory(v.device, vertexMemory);
        VkCommandPoolCreateInfo cp{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        cp.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        cp.queueFamilyIndex = v.family;
        vkCheck(v.vkCreateCommandPool(v.device, &cp, nullptr, &pool), "command pool");
        VkCommandBufferAllocateInfo cb{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        cb.commandPool = pool;
        cb.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cb.commandBufferCount = 1;
        vkCheck(v.vkAllocateCommandBuffers(v.device, &cb, &cmd), "command buffer");
        VkFenceCreateInfo fc{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        vkCheck(v.vkCreateFence(v.device, &fc, nullptr, &fence), "render fence");
    }
    void draw(uint32_t eye, uint32_t index, const XrView &viewPose, const std::string &capture)
    {
        auto &t = targets.at(eye).at(index);
        VkBuffer readback{};
        VkDeviceMemory memory{};
        VkDeviceSize bytes = VkDeviceSize(t.width) * t.height * 4;
        try
        {
            if (!capture.empty())
            {
                VkBufferCreateInfo b{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
                b.size = bytes;
                b.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
                vkCheck(v.vkCreateBuffer(v.device, &b, nullptr, &readback), "readback buffer");
                VkMemoryRequirements req{};
                v.vkGetBufferMemoryRequirements(v.device, readback, &req);
                memory = v.allocate(req, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
                vkCheck(v.vkBindBufferMemory(v.device, readback, memory, 0), "readback memory");
            }
            vkCheck(v.vkResetCommandBuffer(cmd, 0), "reset command");
            VkCommandBufferBeginInfo b{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
            b.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            vkCheck(v.vkBeginCommandBuffer(cmd, &b), "begin command");
            VkClearValue clear{};
            clear.color = {{.025f, .04f, .08f, 1}};
            VkRenderPassBeginInfo rb{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
            rb.renderPass = pass;
            rb.framebuffer = t.framebuffer;
            rb.renderArea.extent = {t.width, t.height};
            rb.clearValueCount = 1;
            rb.pClearValues = &clear;
            v.vkCmdBeginRenderPass(cmd, &rb, VK_SUBPASS_CONTENTS_INLINE);
            v.vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
            VkViewport viewport{0, 0, float(t.width), float(t.height), 0, 1};
            VkRect2D scissor{{0, 0}, {t.width, t.height}};
            v.vkCmdSetViewport(cmd, 0, 1, &viewport);
            v.vkCmdSetScissor(cmd, 0, 1, &scissor);
            VkDeviceSize offset = 0;
            v.vkCmdBindVertexBuffers(cmd, 0, 1, &vertices, &offset);
            auto mvp = multiply(projection(viewPose.fov, .05f, 100), view(viewPose.pose));
            v.vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_VERTEX_BIT, 0, 64, mvp.m.data());
            v.vkCmdDraw(cmd, vertexCount, 1, 0, 0);
            v.vkCmdEndRenderPass(cmd);
            if (readback)
            {
                VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
                barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                barrier.oldLayout = finalLayout;
                barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.image = t.image;
                barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
                v.vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                       VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
                VkBufferImageCopy copy{};
                copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
                copy.imageExtent = {t.width, t.height, 1};
                v.vkCmdCopyImageToBuffer(cmd, t.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, readback, 1, &copy);
                barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                barrier.dstAccessMask = finalLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                    ? VK_ACCESS_SHADER_READ_BIT : VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
                barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                barrier.newLayout = finalLayout;
                v.vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                       VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1,
                                       &barrier);
            }
            vkCheck(v.vkEndCommandBuffer(cmd), "end command");
            VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
            submit.commandBufferCount = 1;
            submit.pCommandBuffers = &cmd;
            vkCheck(v.vkQueueSubmit(v.queue, 1, &submit, fence), "submit eye");
            vkCheck(v.vkWaitForFences(v.device, 1, &fence, VK_TRUE, 5000000000), "eye render fence");
            vkCheck(v.vkResetFences(v.device, 1, &fence), "reset fence");
            if (readback)
            {
                void *p = nullptr;
                vkCheck(v.vkMapMemory(v.device, memory, 0, bytes, 0, &p), "map capture");
                std::ofstream out(capture, std::ios::binary);
                if (!out)
                {
                    v.vkUnmapMemory(v.device, memory);
                    throw std::runtime_error("capture path unavailable");
                }
                out << "P6\n" << t.width << ' ' << t.height << "\n255\n";
                auto *pixels = static_cast<unsigned char *>(p);
                bool bgra = format == VK_FORMAT_B8G8R8A8_SRGB || format == VK_FORMAT_B8G8R8A8_UNORM;
                for (size_t j = 0; j < size_t(t.width) * t.height; j++)
                {
                    char rgb[] = {char(pixels[4 * j + (bgra ? 2 : 0)]), char(pixels[4 * j + 1]),
                                  char(pixels[4 * j + (bgra ? 0 : 2)])};
                    out.write(rgb, 3);
                }
                v.vkUnmapMemory(v.device, memory);
                if (!out)
                    throw std::runtime_error("capture write failed");
            }
        }
        catch (...)
        {
            v.vkDeviceWaitIdle(v.device);
            if (readback)
                v.vkDestroyBuffer(v.device, readback, nullptr);
            if (memory)
                v.vkFreeMemory(v.device, memory, nullptr);
            throw;
        }
        if (readback)
            v.vkDestroyBuffer(v.device, readback, nullptr);
        if (memory)
            v.vkFreeMemory(v.device, memory, nullptr);
    }
    void destroy() noexcept
    {
        if (!v.device)
            return;
        v.vkDeviceWaitIdle(v.device);
        if (fence)
            v.vkDestroyFence(v.device, fence, nullptr);
        if (pool)
            v.vkDestroyCommandPool(v.device, pool, nullptr);
        if (vertices)
            v.vkDestroyBuffer(v.device, vertices, nullptr);
        if (vertexMemory)
            v.vkFreeMemory(v.device, vertexMemory, nullptr);
        if (pipeline)
            v.vkDestroyPipeline(v.device, pipeline, nullptr);
        if (layout)
            v.vkDestroyPipelineLayout(v.device, layout, nullptr);
        for (auto &eye : targets)
            for (auto &t : eye)
            {
                if (t.framebuffer)
                    v.vkDestroyFramebuffer(v.device, t.framebuffer, nullptr);
                if (t.view)
                    v.vkDestroyImageView(v.device, t.view, nullptr);
            }
        if (pass)
            v.vkDestroyRenderPass(v.device, pass, nullptr);
    }
};
} // namespace evr
