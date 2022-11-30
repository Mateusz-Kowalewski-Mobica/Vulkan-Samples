/* Copyright (c) 2019, Arm Limited and Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 the "License";
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "api_vulkan_sample.h"
#include "geometry/frustum.h"

class ExtendedDynamicState2 : public ApiVulkanSample
{
  public:
	struct
	{
		bool      depth_bias_enable         = false;
		bool      primitive_restart_enable  = false;
		bool      rasterizer_discard_enable = false;
		bool      tessellation              = false;
		float     tess_factor               = 1.0;
		int32_t   logic_op_index{};
		VkLogicOp logicOp = VK_LOGIC_OP_CLEAR;
		float     patch_control_points_float{4.0f};
		uint32_t  patch_control_points{4};
		float	  lightX{1};
		float	  lightY{1};
		float	  lightZ{1};
	} gui_settings;

	std::vector<std::string> logic_op_object_names{"CLEAR",
	                                               "AND",
	                                               "AND_REVERSE",
	                                               "COPY",
	                                               "AND_INVERTED",
	                                               "NO_OP",
	                                               "XOR",
	                                               "OR",
	                                               "NOR",
	                                               "EQUIVALENT",
	                                               "INVERT",
	                                               "OR_REVERSE",
	                                               "COPY_INVERTED",
	                                               "OR_INVERTED",
	                                               "NAND",
	                                               "SET"};

	struct
	{
		Texture envmap;
		Texture terrain;
		Texture displace;
	} textures;

	struct UBOVS
	{
		glm::mat4 projection;
		glm::mat4 modelview;
		glm::mat4 skybox_modelview;
		glm::vec4 ambientLightColor{1.f, 1.f, 1.f, 0.025f};
		glm::vec3 lightPosition{0.5f, 5.f, 0.f};
		glm::vec4 lightColor{1.f};
		float     modelscale = 0.25f;
	} ubo_vs;

	struct UBOTESS
	{
		glm::mat4 projection;
		glm::mat4 modelview;
		glm::vec4 light_pos = glm::vec4(-48.0f, -40.0f, 46.0f, 0.0f);
		glm::vec4 frustum_planes[6];
		float     displacement_factor = 32.0f;
		float     tessellation_factor = 0.75f;
		glm::vec2 viewport_dim;
		// Desired size of tessellated quad patch edge
		float tessellated_edge_size = 20.0f;
		float modelscale            = 0.15f;
	} ubo_tess;

	struct
	{
		VkDescriptorSetLayout skybox{VK_NULL_HANDLE};
		VkDescriptorSetLayout model{VK_NULL_HANDLE};
	} descriptor_set_layouts;

	struct
	{
		VkPipelineLayout skybox{VK_NULL_HANDLE};
		VkPipelineLayout model{VK_NULL_HANDLE};
	} pipeline_layouts;

	struct
	{
		VkDescriptorSet skybox{VK_NULL_HANDLE};
		VkDescriptorSet model{VK_NULL_HANDLE};
	} descriptor_sets;

	struct
	{
		std::unique_ptr<vkb::core::Buffer> model_tessellation;
		std::unique_ptr<vkb::core::Buffer> skybox;
	} uniform_buffers;
	

	//VkPipelineLayout                                   pipeline_layout{VK_NULL_HANDLE};
	VkPipeline                                         model_pipeline{VK_NULL_HANDLE};
	VkPipeline                                         skybox_pipeline{VK_NULL_HANDLE};
	VkPipeline                                         light_indicator_pipeline{VK_NULL_HANDLE};
	VkPhysicalDeviceExtendedDynamicState2FeaturesEXT   extended_dynamic_state2_features{};
	VkPhysicalDeviceGraphicsPipelineLibraryFeaturesEXT graphics_pipeline_library{};

	std::unique_ptr<vkb::sg::Scene> scene;
	struct SceneNode
	{
		std::string       name;
		vkb::sg::Node *   node;
		vkb::sg::SubMesh *sub_mesh;
	};
	std::vector<SceneNode> linear_scene_nodes;
	std::vector<int32_t>               visibility_list;

	//VkDescriptorSet       descriptor_set{VK_NULL_HANDLE};
	//VkDescriptorSetLayout descriptor_set_layout{VK_NULL_HANDLE};
	VkDescriptorPool descriptor_pool{VK_NULL_HANDLE};

	std::unique_ptr<vkb::sg::SubMesh> skybox;
	std::unique_ptr<vkb::sg::SubMesh> object;
	std::unique_ptr<vkb::sg::SubMesh> plane;
	std::unique_ptr<vkb::sg::SubMesh> lightIndicator;
	vkb::Frustum                      frustum;

	ExtendedDynamicState2();
	~ExtendedDynamicState2();

	virtual void render(float delta_time) override;
	virtual void build_command_buffers() override;
	virtual bool prepare(vkb::Platform &platform) override;
	virtual void request_gpu_features(vkb::PhysicalDevice &gpu) override;
	virtual void on_update_ui_overlay(vkb::Drawer &drawer) override;

	void prepare_uniform_buffers();
	void update_uniform_buffers();
	void create_pipeline();
	void draw();

	void load_assets();
	void create_descriptor_pool();
	void setup_descriptor_set_layout();
	void create_descriptor_sets();

#if VK_NO_PROTOTYPES
	PFN_vkCmdSetDepthBiasEnableEXT         vkCmdSetDepthBiasEnableEXT{VK_NULL_HANDLE};
	PFN_vkCmdSetLogicOpEXT                 vkCmdSetLogicOpEXT{VK_NULL_HANDLE};
	PFN_vkCmdSetPatchControlPointsEXT      vkCmdSetPatchControlPointsEXT{VK_NULL_HANDLE};
	PFN_vkCmdSetPrimitiveRestartEnableEXT  vkCmdSetPrimitiveRestartEnableEXT{VK_NULL_HANDLE};
	PFN_vkCmdSetRasterizerDiscardEnableEXT vkCmdSetRasterizerDiscardEnableEXT{VK_NULL_HANDLE};
#endif
};

std::unique_ptr<vkb::VulkanSample> create_extended_dynamic_state2();
