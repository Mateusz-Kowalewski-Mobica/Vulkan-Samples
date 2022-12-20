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
#include "scene_graph/components/pbr_material.h"

class ExtendedDynamicState2 : public ApiVulkanSample
{
  public:
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

	typedef struct
	{
		bool depth_bias         = false;
		bool rasterizer_discard = false;
	} model_dynamic_param;

	struct
	{
		bool                             primitive_restart_enable = false;
		bool                             tessellation             = false;
		float                            tess_factor              = 1.0;
		int32_t                          logic_op_index{};
		VkLogicOp                        logicOp = VK_LOGIC_OP_CLEAR;
		float                            patch_control_points_float{4.0f};
		uint32_t                         patch_control_points{3};
		std::vector<model_dynamic_param> objects;
		int                              selected_obj     = 0;
		bool                             selection_active = true;
		bool                             time_tick        = false;
	} gui_settings;

	struct UBOVS
	{
		alignas(16) glm::mat4 projection;
		alignas(16) glm::mat4 view;
		alignas(16) glm::vec4 ambientLightColor = glm::vec4(1.f, 1.f, 1.f, 0.1f);
		alignas(16) glm::vec4 lightPosition     = glm::vec4(3.0f, 8.0f, -6.0f, 1.0f);
		//glm::vec3 lightPosition{3.0f, 8.f, -6.f};
		alignas(16) glm::vec4 lightColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
		alignas(4) float lightIntensity  = 50.0f;
	} ubo_vs;

	struct UBOTESS
	{
		glm::mat4 projection;
		glm::mat4 modelview;
		glm::vec4 light_pos           = glm::vec4(-48.0f, -40.0f, 46.0f, 0.0f);
		float     displacement_factor = 32.0f;
		float     tessellation_factor = 0.75f;
	} ubo_tess;

	struct
	{
		VkDescriptorSetLayout baseline{VK_NULL_HANDLE};
		VkDescriptorSetLayout model{VK_NULL_HANDLE};
	} descriptor_set_layouts;

	struct
	{
		VkPipelineLayout baseline{VK_NULL_HANDLE};
		VkPipelineLayout model{VK_NULL_HANDLE};
	} pipeline_layouts;

	struct
	{
		VkDescriptorSet baseline{VK_NULL_HANDLE};
		VkDescriptorSet model{VK_NULL_HANDLE};
	} descriptor_sets;

	struct
	{
		VkPipeline baseline{VK_NULL_HANDLE};
		VkPipeline tesselation{VK_NULL_HANDLE};
	} pipeline;

	struct
	{
		std::unique_ptr<vkb::core::Buffer> model_tessellation;
		std::unique_ptr<vkb::core::Buffer> baseline;
	} uniform_buffers;

	VkPhysicalDeviceExtendedDynamicState2FeaturesEXT   extended_dynamic_state2_features{};
	VkPhysicalDeviceGraphicsPipelineLibraryFeaturesEXT graphics_pipeline_library{};

	struct
	{
		glm::mat4 model_matrix;
		glm::vec4 color;
	} push_const_block;

	std::unique_ptr<vkb::sg::Scene> scene;
	struct SceneNode
	{
		std::string       name;
		vkb::sg::Node *   node;
		vkb::sg::SubMesh *sub_mesh;
	};
	std::vector<std::vector<SceneNode>> scene_nodes;
	VkDescriptorPool                    descriptor_pool{VK_NULL_HANDLE};

	struct Cube
	{
		std::unique_ptr<vkb::core::Buffer> vertices;
		std::unique_ptr<vkb::core::Buffer> indices;
		uint32_t                           index_count;
	} cube;

	ExtendedDynamicState2();
	~ExtendedDynamicState2();

	virtual void render(float delta_time) override;
	virtual void build_command_buffers() override;
	virtual bool prepare(vkb::Platform &platform) override;
	virtual void request_gpu_features(vkb::PhysicalDevice &gpu) override;
	virtual void on_update_ui_overlay(vkb::Drawer &drawer) override;
	virtual void update(float delta_time) override;

	void prepare_uniform_buffers();
	void update_uniform_buffers();
	void prepare_visibility_buffer();
	void create_pipeline();
	void draw();

	void     load_assets();
	void     create_descriptor_pool();
	void     setup_descriptor_set_layout();
	void     create_descriptor_sets();
	uint32_t get_node_index(std::string name, std::vector<SceneNode> *scene_node);
	void     selection_indicator(const vkb::sg::PBRMaterial *original_mat, vkb::sg::PBRMaterial *new_mat);
	void     scene_pipeline_divide(std::vector<std::vector<SceneNode>> *scene_node);
	void     draw_from_scene(VkCommandBuffer command_buffer, std::vector<std::vector<SceneNode>> *scene_node, int scene_index);
	void     draw_created_model(VkCommandBuffer commandBuffer);
	void     model_data_creation();
};

std::unique_ptr<vkb::VulkanSample> create_extended_dynamic_state2();
