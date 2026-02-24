#pragma once
#include <vulkan/vulkan.h>
#include <string>
#include <vector>

class BaseMeshPipeline {
public:
    VkPipeline       pipeline       = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;

    void create(VkDevice device, VkRenderPass renderPass,
                VkDescriptorSetLayout sceneLayout,
                VkDescriptorSetLayout halfEdgeLayout,
                VkDescriptorSetLayout perObjectLayout);
    void destroy(VkDevice device);

private:
    static std::vector<char> readSpv(const std::string& path);
    static VkShaderModule    createModule(VkDevice device, const std::vector<char>& code);
};
