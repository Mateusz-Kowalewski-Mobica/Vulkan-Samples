/* Copyright (c) 2019-2020, Arm Limited and Contributors
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

#include "extended_dynamic_state2.h"

#include "gltf_loader.h"
#include "scene_graph/components/mesh.h"
#include "scene_graph/components/sub_mesh.h"

/* Defines */
/* Camera initial settings */
#define EDS_CAM_POS_X 2.0f
#define EDS_CAM_POS_Y 4.0f
#define EDS_CAM_POS_Z -10.0f

#define EDS_CAM_ROT_X -15.0f
#define EDS_CAM_ROT_Y 100.0f
#define EDS_CAM_ROT_Z 180.0f

#define EDS_CAM_PERS_FOV 60.0f
#define EDS_CAM_PERS_ZNEAR 256.0f
#define EDS_CAM_PERS_ZFAR 0.1f

#define EDS_DEPTH_BIAS_CONST_FAC 1.0f
#define EDS_DEPTH_BIAS_SLOPE_FAC 1.0f
#define EDS_DEPTH_BIAS_CLAMP 0.0f

#define EDS_PIPELINE_ALL_INDEX 0
#define EDS_PIPELINE_BASELINE_INDEX 1
#define EDS_PIPELINE_TESS_INDEX 2

ExtendedDynamicState2::ExtendedDynamicState2()
{
	title = "Extended Dynamic State2";

	add_instance_extension(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
	add_device_extension(VK_EXT_EXTENDED_DYNAMIC_STATE_2_EXTENSION_NAME);
}

ExtendedDynamicState2::~ExtendedDynamicState2()
{
	if (device)
	{
		vkDestroySampler(get_device().get_handle(), textures.envmap.sampler, VK_NULL_HANDLE);
		textures = {};
		uniform_buffers.baseline.reset();
		uniform_buffers.model_tessellation.reset();

		vkDestroyPipeline(get_device().get_handle(), pipeline.tesselation, VK_NULL_HANDLE);
		vkDestroyPipeline(get_device().get_handle(), pipeline.baseline, VK_NULL_HANDLE);
		vkDestroyPipelineLayout(get_device().get_handle(), pipeline_layouts.model, VK_NULL_HANDLE);
		vkDestroyPipelineLayout(get_device().get_handle(), pipeline_layouts.baseline, VK_NULL_HANDLE);
		vkDestroyDescriptorSetLayout(get_device().get_handle(), descriptor_set_layouts.model, VK_NULL_HANDLE);
		vkDestroyDescriptorSetLayout(get_device().get_handle(), descriptor_set_layouts.baseline, VK_NULL_HANDLE);
		vkDestroyDescriptorPool(get_device().get_handle(), descriptor_pool, VK_NULL_HANDLE);
	}
}

/**
 * 	@fn bool ExtendedDynamicState2::prepare(vkb::Platform &platform)
 * 	@brief Configuring all sample specific settings, creating descriptor sets/pool, pipelines, generating or loading models etc. 
*/
bool ExtendedDynamicState2::prepare(vkb::Platform &platform)
{
	if (!ApiVulkanSample::prepare(platform))
	{
		return false;
	}

	camera.type = vkb::CameraType::LookAt;
	camera.set_position({EDS_CAM_POS_X, EDS_CAM_POS_Y, EDS_CAM_POS_Z});
	camera.set_rotation({EDS_CAM_ROT_X, EDS_CAM_ROT_Y, EDS_CAM_ROT_Z});
	camera.set_perspective(EDS_CAM_PERS_FOV, (float) width / (float) height, EDS_CAM_PERS_ZNEAR, EDS_CAM_PERS_ZFAR);

	load_assets();
	prepare_uniform_buffers();
	create_descriptor_pool();
	setup_descriptor_set_layout();
	create_descriptor_sets();
	create_pipeline();
	build_command_buffers();
	prepared = true;

	return true;
}
/**
 * 	@fn void ExtendedDynamicState2::load_assets()
 *	@brief Loading extra models, textures from assets 
 */
void ExtendedDynamicState2::load_assets()
{
	vkb::GLTFLoader loader{get_device()};
	scene = loader.read_scene_from_file("scenes/Test_scene/Test_scene.gltf");
	assert(scene);

	std::vector<SceneNode> scene_elements;
	// Store all scene nodes in a linear vector for easier access
	for (auto &mesh : scene->get_components<vkb::sg::Mesh>())
	{
		for (auto &node : mesh->get_nodes())
		{
			model_dynamic_param object_param{};
			gui_settings.objects.push_back(object_param);
			for (auto &sub_mesh : mesh->get_submeshes())
			{
				scene_elements.push_back({mesh->get_name(), node, sub_mesh});
			}
		}
	}
	scene_nodes.push_back(scene_elements);

	scene_pipeline_divide(&scene_nodes);
}

/**
 * 	@fn void ExtendedDynamicState2::draw()
 *  @brief Preparing frame and submitting it to the present queue
 */
void ExtendedDynamicState2::draw()
{
	ApiVulkanSample::prepare_frame();
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers    = &draw_cmd_buffers[current_buffer];
	VK_CHECK(vkQueueSubmit(queue, 1, &submit_info, VK_NULL_HANDLE));
	ApiVulkanSample::submit_frame();
}

/**
 * 	@fn void ExtendedDynamicState2::render(float delta_time)
 * 	@brief Drawing frames and/or updating uniform buffers when camera position/rotation was changed
*/
void ExtendedDynamicState2::render(float delta_time)
{
	if (!prepared)
		return;
	draw();
	if (camera.updated)
		update_uniform_buffers();
}

/**
 * 	@fn void ExtendedDynamicState2::prepare_uniform_buffers()
 * 	@brief Preparing uniform buffer (setting bits) and updating UB data
 */
void ExtendedDynamicState2::prepare_uniform_buffers()
{
	uniform_buffers.baseline           = std::make_unique<vkb::core::Buffer>(get_device(), sizeof(ubo_vs), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
	uniform_buffers.model_tessellation = std::make_unique<vkb::core::Buffer>(get_device(), sizeof(ubo_tess), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
	update_uniform_buffers();
}

/**
 * 	@fn void ExtendedDynamicState2::update_uniform_buffers()
 * 	@brief Updating data from application to GPU uniform buffer
 */
void ExtendedDynamicState2::update_uniform_buffers()
{
	ubo_vs.projection = camera.matrices.perspective;
	ubo_vs.view       = camera.matrices.view * glm::mat4(1.f);
	//ubo_vs.lightIntensity = 0.5;
	uniform_buffers.baseline->convert_and_update(ubo_vs);

	// Tessellation

	ubo_tess.projection  = camera.matrices.perspective;
	ubo_tess.modelview   = camera.matrices.view * glm::mat4(1.0f);
	ubo_tess.light_pos.y = -0.5f - ubo_tess.displacement_factor;        // todo: Not uesed yet

	ubo_tess.tessellation_factor = gui_settings.tess_factor;
	float saved_factor           = ubo_tess.tessellation_factor;
	if (!gui_settings.tessellation)
	{
		// Setting this to zero sets all tessellation factors to 1.0 in the shader
		ubo_tess.tessellation_factor = 0.0f;
	}

	uniform_buffers.model_tessellation->convert_and_update(ubo_tess);

	if (!gui_settings.tessellation)
	{
		ubo_tess.tessellation_factor = saved_factor;
	}
}

/**
 * 	@fn void ExtendedDynamicState2::create_pipeline()
 * 	@brief Creating graphical pipeline
 * 	@details Preparing pipeline structures:
 * 			 - VkPipelineInputAssemblyStateCreateInfo
 * 			 - VkPipelineRasterizationStateCreateInfo
 * 			 - VkPipelineColorBlendAttachmentState
 * 			 - VkPipelineColorBlendStateCreateInfo
 * 			 - VkPipelineDepthStencilStateCreateInfo
 * 			 - VkPipelineViewportStateCreateInfo
 * 			 - VkPipelineMultisampleStateCreateInfo
 * 			 - VkPipelineDynamicStateCreateInfo
 * 			 - VkPipelineShaderStageCreateInfo
 * 			 - VkPipelineRenderingCreateInfoKHR
 * 			 - VkGraphicsPipelineCreateInfo
 * 
 * 	@note Specific settings that were used to implement Vertex Input Dynamic State extension in this sample:
 * 			 - In VkPipelineDynamicStateCreateInfo use "VK_DYNAMIC_STATE_VERTEX_INPUT_EXT" enumeration in config vector.
 * 			 - In VkGraphicsPipelineCreateInfo "pVertexInputState" element is not require to declare (when using vertex input dynamic state)
 * 
 */
void ExtendedDynamicState2::create_pipeline()
{
	VkPipelineInputAssemblyStateCreateInfo input_assembly_state =
	    vkb::initializers::pipeline_input_assembly_state_create_info(
	        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
	        0,
	        VK_FALSE);

	VkPipelineRasterizationStateCreateInfo rasterization_state =
	    vkb::initializers::pipeline_rasterization_state_create_info(
	        VK_POLYGON_MODE_FILL,
	        VK_CULL_MODE_BACK_BIT,
	        VK_FRONT_FACE_COUNTER_CLOCKWISE,
	        0);

	rasterization_state.depthBiasConstantFactor = EDS_DEPTH_BIAS_CONST_FAC;
	rasterization_state.depthBiasSlopeFactor    = EDS_DEPTH_BIAS_SLOPE_FAC;
	rasterization_state.depthBiasClamp          = EDS_DEPTH_BIAS_CLAMP;

	VkPipelineColorBlendAttachmentState blend_attachment_state =
	    vkb::initializers::pipeline_color_blend_attachment_state(
	        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
	        VK_TRUE);

	blend_attachment_state.colorWriteMask      = 0xF;
	blend_attachment_state.colorBlendOp        = VK_BLEND_OP_ADD;
	blend_attachment_state.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	blend_attachment_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	blend_attachment_state.alphaBlendOp        = VK_BLEND_OP_ADD;
	blend_attachment_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	blend_attachment_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;

	VkPipelineColorBlendStateCreateInfo color_blend_state =
	    vkb::initializers::pipeline_color_blend_state_create_info(
	        1,
	        &blend_attachment_state);

	/* Note: Using Reversed depth-buffer for increased precision, so Greater depth values are kept */
	VkPipelineDepthStencilStateCreateInfo depth_stencil_state =
	    vkb::initializers::pipeline_depth_stencil_state_create_info(
	        VK_TRUE, /* changed */
	        VK_TRUE,
	        VK_COMPARE_OP_GREATER);

	VkPipelineViewportStateCreateInfo viewport_state =
	    vkb::initializers::pipeline_viewport_state_create_info(1, 1, 0);

	VkPipelineMultisampleStateCreateInfo multisample_state =
	    vkb::initializers::pipeline_multisample_state_create_info(
	        VK_SAMPLE_COUNT_1_BIT,
	        0);

	VkPipelineTessellationStateCreateInfo tessellation_state =
	    vkb::initializers::pipeline_tessellation_state_create_info(2);

	std::vector<VkDynamicState> dynamic_state_enables = {
	    VK_DYNAMIC_STATE_VIEWPORT,
	    VK_DYNAMIC_STATE_SCISSOR,
	    VK_DYNAMIC_STATE_DEPTH_BIAS_ENABLE_EXT,
	    VK_DYNAMIC_STATE_RASTERIZER_DISCARD_ENABLE_EXT,
	};
	VkPipelineDynamicStateCreateInfo dynamic_state =
	    vkb::initializers::pipeline_dynamic_state_create_info(
	        dynamic_state_enables.data(),
	        static_cast<uint32_t>(dynamic_state_enables.size()),
	        0);

	// Vertex bindings an attributes for model rendering
	// Binding description
	std::vector<VkVertexInputBindingDescription> vertex_input_bindings = {
	    vkb::initializers::vertex_input_binding_description(0, sizeof(glm::vec3), VK_VERTEX_INPUT_RATE_VERTEX),
	    vkb::initializers::vertex_input_binding_description(1, sizeof(glm::vec3), VK_VERTEX_INPUT_RATE_VERTEX),
	};

	// Attribute descriptions
	std::vector<VkVertexInputAttributeDescription> vertex_input_attributes = {
	    vkb::initializers::vertex_input_attribute_description(0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0 /*offsetof(Vertex, pos)*/),           // Position
	    vkb::initializers::vertex_input_attribute_description(1, 1, VK_FORMAT_R32G32B32_SFLOAT, 0 /*offsetof(Vertex, normal)*/),        // Normal
	};

	VkPipelineVertexInputStateCreateInfo vertex_input_state = vkb::initializers::pipeline_vertex_input_state_create_info();
	vertex_input_state.vertexBindingDescriptionCount        = static_cast<uint32_t>(vertex_input_bindings.size());
	vertex_input_state.pVertexBindingDescriptions           = vertex_input_bindings.data();
	vertex_input_state.vertexAttributeDescriptionCount      = static_cast<uint32_t>(vertex_input_attributes.size());
	vertex_input_state.pVertexAttributeDescriptions         = vertex_input_attributes.data();

	std::array<VkPipelineShaderStageCreateInfo, 4> shader_stages{};
	shader_stages[0] = load_shader("extended_dynamic_state2/depthbias.vert", VK_SHADER_STAGE_VERTEX_BIT);
	shader_stages[1] = load_shader("extended_dynamic_state2/depthbias.frag", VK_SHADER_STAGE_FRAGMENT_BIT);

	/* Create graphics pipeline for dynamic rendering */
	VkFormat color_rendering_format = render_context->get_format();

	/* Provide information for dynamic rendering */
	VkPipelineRenderingCreateInfoKHR pipeline_create{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR};
	pipeline_create.pNext                   = VK_NULL_HANDLE;
	pipeline_create.colorAttachmentCount    = 1;
	pipeline_create.pColorAttachmentFormats = &color_rendering_format;
	pipeline_create.depthAttachmentFormat   = depth_format;
	pipeline_create.stencilAttachmentFormat = depth_format;

	/* Skybox pipeline (background cube) */
	VkSpecializationInfo                    specialization_info;
	std::array<VkSpecializationMapEntry, 1> specialization_map_entries{};
	specialization_map_entries[0]        = vkb::initializers::specialization_map_entry(0, 0, sizeof(uint32_t));
	uint32_t shadertype                  = 0;
	specialization_info                  = vkb::initializers::specialization_info(1, specialization_map_entries.data(), sizeof(shadertype), &shadertype);
	shader_stages[0].pSpecializationInfo = &specialization_info;
	shader_stages[1].pSpecializationInfo = &specialization_info;

	/* Use the pNext to point to the rendering create struct */
	VkGraphicsPipelineCreateInfo graphics_create{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
	graphics_create.pNext               = VK_NULL_HANDLE;
	graphics_create.renderPass          = VK_NULL_HANDLE;
	graphics_create.pInputAssemblyState = &input_assembly_state;
	graphics_create.pRasterizationState = &rasterization_state;
	graphics_create.pColorBlendState    = &color_blend_state;
	graphics_create.pMultisampleState   = &multisample_state;
	graphics_create.pViewportState      = &viewport_state;
	graphics_create.pDepthStencilState  = &depth_stencil_state;
	graphics_create.pDynamicState       = &dynamic_state;
	graphics_create.pVertexInputState   = &vertex_input_state;
	graphics_create.pTessellationState  = VK_NULL_HANDLE;
	graphics_create.stageCount          = 2;
	graphics_create.pStages             = shader_stages.data();
	graphics_create.layout              = pipeline_layouts.baseline;

	graphics_create.pNext      = VK_NULL_HANDLE;
	graphics_create.renderPass = render_pass;

	VK_CHECK(vkCreateGraphicsPipelines(get_device().get_handle(), pipeline_cache, 1, &graphics_create, VK_NULL_HANDLE, &pipeline.baseline));

	/* Object rendering pipeline */
	// shadertype = 1;
	graphics_create.pTessellationState = &tessellation_state;
	graphics_create.layout             = pipeline_layouts.model;
	input_assembly_state.topology      = VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
	dynamic_state_enables.push_back(VK_DYNAMIC_STATE_PATCH_CONTROL_POINTS_EXT);
	dynamic_state.pDynamicStates    = dynamic_state_enables.data();
	dynamic_state.dynamicStateCount = static_cast<uint32_t>(dynamic_state_enables.size());

	if (get_device().get_gpu().get_features().fillModeNonSolid)
	{
		rasterization_state.polygonMode = VK_POLYGON_MODE_LINE;        //VK_POLYGON_MODE_LINE; /* Wireframe mode */
	}

	shader_stages[0]           = load_shader("extended_dynamic_state2/gbuffer.vert", VK_SHADER_STAGE_VERTEX_BIT);
	shader_stages[1]           = load_shader("extended_dynamic_state2/gbuffer.frag", VK_SHADER_STAGE_FRAGMENT_BIT);
	shader_stages[2]           = load_shader("extended_dynamic_state2/gbuffer.tesc", VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);
	shader_stages[3]           = load_shader("extended_dynamic_state2/gbuffer.tese", VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);
	graphics_create.stageCount = static_cast<uint32_t>(shader_stages.size());
	graphics_create.pStages    = shader_stages.data();
	/* Enable depth test and write */
	depth_stencil_state.depthWriteEnable = VK_TRUE;
	depth_stencil_state.depthTestEnable  = VK_TRUE;
	/* Flip cull mode */
	rasterization_state.cullMode = VK_CULL_MODE_FRONT_BIT;
	VK_CHECK(vkCreateGraphicsPipelines(get_device().get_handle(), pipeline_cache, 1, &graphics_create, VK_NULL_HANDLE, &pipeline.tesselation));
}

/**
 * 	@fn void ExtendedDynamicState2::build_command_buffers()
 * 	@brief Creating command buffers and drawing particular elements on window.
 * 	@details Drawing object list:
 * 			 - Skybox - cube that have background texture attached (easy way to generate background to scene).
 * 			 - Object - cube that was placed in the middle with some reflection shader effect.
 * 			 - Created model - cube that was created on runtime.
 * 			 - UI - some statistic tab
 * 
 * 	@note In case of Vertex Input Dynamic State feature sample need to create model in runtime because of requirement to have different data structure.
 * 		  By default function "load_model" from framework is parsing data from .gltf files and build it every time in declared structure (see Vertex structure in framework files).
 * 		  Before drawing different models (in case of vertex input data structure) "change_vertex_input_data" fuction is call for dynamically change Vertex Input data.
 */
void ExtendedDynamicState2::build_command_buffers()
{
	std::array<VkClearValue, 2> clear_values{};
	clear_values[0].color        = {{0.0f, 0.0f, 0.0f, 0.0f}};
	clear_values[1].depthStencil = {0.0f, 0};

	int i = -1;
	for (auto &draw_cmd_buffer : draw_cmd_buffers)
	{
		i++;
		auto command_begin = vkb::initializers::command_buffer_begin_info();
		VK_CHECK(vkBeginCommandBuffer(draw_cmd_buffers[i], &command_begin));

		VkImageSubresourceRange range{};
		range.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
		range.baseMipLevel   = 0;
		range.levelCount     = VK_REMAINING_MIP_LEVELS;
		range.baseArrayLayer = 0;
		range.layerCount     = VK_REMAINING_ARRAY_LAYERS;

		VkImageSubresourceRange depth_range{range};
		depth_range.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

		VkRenderPassBeginInfo render_pass_begin_info    = vkb::initializers::render_pass_begin_info();
		render_pass_begin_info.renderPass               = render_pass;
		render_pass_begin_info.framebuffer              = framebuffers[i];
		render_pass_begin_info.renderArea.extent.width  = width;
		render_pass_begin_info.renderArea.extent.height = height;
		render_pass_begin_info.clearValueCount          = static_cast<uint32_t>(clear_values.size());
		render_pass_begin_info.pClearValues             = clear_values.data();

		vkCmdBeginRenderPass(draw_cmd_buffers[i], &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

		VkViewport viewport = vkb::initializers::viewport((float) width, (float) height, 0.0f, 1.0f);
		vkCmdSetViewport(draw_cmd_buffers[i], 0, 1, &viewport);

		VkRect2D scissor = vkb::initializers::rect2D(static_cast<int>(width), static_cast<int>(height), 0, 0);
		vkCmdSetScissor(draw_cmd_buffers[i], 0, 1, &scissor);

		/* One descriptor set is used, and the draw type is toggled by a specialization constant */
		vkCmdBindDescriptorSets(draw_cmd_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layouts.baseline, 0, 1, &descriptor_sets.baseline, 0, nullptr);

		vkCmdBindPipeline(draw_cmd_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.baseline);

		draw_from_scene(draw_cmd_buffers[i], &scene_nodes, EDS_PIPELINE_BASELINE_INDEX);

		vkCmdBindDescriptorSets(draw_cmd_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layouts.model, 0, 1, &descriptor_sets.model, 0, nullptr);

		vkCmdBindPipeline(draw_cmd_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.tesselation);
		vkCmdSetPatchControlPointsEXT(draw_cmd_buffers[i], gui_settings.patch_control_points);
		draw_from_scene(draw_cmd_buffers[i], &scene_nodes, EDS_PIPELINE_TESS_INDEX);

		// uint32_t node_index = 0;
		// for (auto &node : scene_nodes[EDS_PIPELINE_BASELINE_INDEX])
		// {
		// 	const auto &vertex_buffer_pos    = node.sub_mesh->vertex_buffers.at("position");
		// 	const auto &vertex_buffer_normal = node.sub_mesh->vertex_buffers.at("normal");
		// 	auto &      index_buffer         = node.sub_mesh->index_buffer;

		// 	if (gui_settings.objects[node_index].depth_bias == true)
		// 	{
		// 		vkCmdSetDepthBiasEnableEXT(draw_cmd_buffers[i], VK_TRUE);
		// 	}
		// 	else
		// 	{
		// 		vkCmdSetDepthBiasEnableEXT(draw_cmd_buffers[i], VK_FALSE);
		// 	}

		// 	if (gui_settings.objects[node_index].rasterizer_discard == true)
		// 	{
		// 		vkCmdSetRasterizerDiscardEnableEXT(draw_cmd_buffers[i], VK_TRUE);
		// 	}
		// 	else
		// 	{
		// 		vkCmdSetRasterizerDiscardEnableEXT(draw_cmd_buffers[i], VK_FALSE);
		// 	}

		// 	// Pass data for the current node via push commands
		// 	auto test                     = node.sub_mesh->get_material();
		// 	auto node_material            = dynamic_cast<const vkb::sg::PBRMaterial *>(node.sub_mesh->get_material());
		// 	push_const_block.model_matrix = node.node->get_transform().get_world_matrix();
		// 	if (node.node->get_id() != gui_settings.selected_obj ||
		// 	    gui_settings.selection_active == false)
		// 	{
		// 		push_const_block.color = node_material->base_color_factor;
		// 	}
		// 	else
		// 	{
		// 		vkb::sg::PBRMaterial temp_material{"Selected_Material"};
		// 		selection_indicator(node_material, &temp_material);
		// 		push_const_block.color = temp_material.base_color_factor;
		// 	}
		// 	vkCmdPushConstants(draw_cmd_buffers[i], pipeline_layouts.baseline, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(push_const_block), &push_const_block);

		// 	VkDeviceSize offsets[1] = {0};
		// 	vkCmdBindVertexBuffers(draw_cmd_buffers[i], 0, 1, vertex_buffer_pos.get(), offsets);
		// 	vkCmdBindVertexBuffers(draw_cmd_buffers[i], 1, 1, vertex_buffer_normal.get(), offsets);
		// 	vkCmdBindIndexBuffer(draw_cmd_buffers[i], index_buffer->get_handle(), 0, node.sub_mesh->index_type);

		// 	vkCmdDrawIndexed(draw_cmd_buffers[i], node.sub_mesh->vertex_indices, 1, 0, 0, 0);

		// 	node_index++;
		// }

		/* UI */
		draw_ui(draw_cmd_buffers[i]);

		vkCmdEndRenderPass(draw_cmd_buffers[i]);

		VK_CHECK(vkEndCommandBuffer(draw_cmd_buffers[i]));
	}
}

/**
 * 	@fn void ExtendedDynamicState2::create_descriptor_pool()
 * 	@brief Creating descriptor pool with size adjusted to use uniform buffer and image sampler
*/
void ExtendedDynamicState2::create_descriptor_pool()
{
	std::vector<VkDescriptorPoolSize> pool_sizes = {
	    vkb::initializers::descriptor_pool_size(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3),
	    vkb::initializers::descriptor_pool_size(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3)};

	VkDescriptorPoolCreateInfo descriptor_pool_create_info =
	    vkb::initializers::descriptor_pool_create_info(
	        static_cast<uint32_t>(pool_sizes.size()),
	        pool_sizes.data(),
	        2);

	VK_CHECK(vkCreateDescriptorPool(get_device().get_handle(), &descriptor_pool_create_info, nullptr, &descriptor_pool));
}

/**
 * 	@fn void ExtendedDynamicState2::setup_descriptor_set_layout()
 * 	@brief Creating layout for descriptor sets
*/
void ExtendedDynamicState2::setup_descriptor_set_layout()
{
	std::vector<VkDescriptorSetLayoutBinding> set_layout_bindings = {
	    vkb::initializers::descriptor_set_layout_binding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
	                                                     VK_SHADER_STAGE_VERTEX_BIT,
	                                                     0),
	    vkb::initializers::descriptor_set_layout_binding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1),
	};

	VkDescriptorSetLayoutCreateInfo descriptor_layout_create_info =
	    vkb::initializers::descriptor_set_layout_create_info(set_layout_bindings.data(), static_cast<uint32_t>(set_layout_bindings.size()));

	VK_CHECK(vkCreateDescriptorSetLayout(get_device().get_handle(), &descriptor_layout_create_info, nullptr, &descriptor_set_layouts.baseline));

	VkPipelineLayoutCreateInfo pipeline_layout_create_info =
	    vkb::initializers::pipeline_layout_create_info(
	        &descriptor_set_layouts.baseline,
	        1);

	// Pass scene node information via push constants
	VkPushConstantRange push_constant_range            = vkb::initializers::push_constant_range(VK_SHADER_STAGE_VERTEX_BIT, sizeof(push_const_block), 0);
	pipeline_layout_create_info.pushConstantRangeCount = 1;
	pipeline_layout_create_info.pPushConstantRanges    = &push_constant_range;

	VK_CHECK(vkCreatePipelineLayout(get_device().get_handle(), &pipeline_layout_create_info, nullptr, &pipeline_layouts.baseline));

	/* Dodać wersję dla modelu */

	// Terrain
	set_layout_bindings =
	    {
	        // Binding 0 : Shared Tessellation shader ubo
	        vkb::initializers::descriptor_set_layout_binding(
	            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
	            VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT | VK_SHADER_STAGE_VERTEX_BIT,
	            0),
	        // Binding 3 : Terrain texture array layers
	        vkb::initializers::descriptor_set_layout_binding(
	            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
	            VK_SHADER_STAGE_FRAGMENT_BIT,
	            1),
	        // Binding 1 : Height map
	        vkb::initializers::descriptor_set_layout_binding(
	            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
	            VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
	            2),
	    };

	descriptor_layout_create_info.pBindings    = set_layout_bindings.data();
	descriptor_layout_create_info.bindingCount = static_cast<uint32_t>(set_layout_bindings.size());
	VK_CHECK(vkCreateDescriptorSetLayout(get_device().get_handle(), &descriptor_layout_create_info, nullptr, &descriptor_set_layouts.model));

	pipeline_layout_create_info.pSetLayouts    = &descriptor_set_layouts.model;
	pipeline_layout_create_info.setLayoutCount = 1;
	push_constant_range.stageFlags = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
	VK_CHECK(vkCreatePipelineLayout(get_device().get_handle(), &pipeline_layout_create_info, nullptr, &pipeline_layouts.model));
}

/**
 * 	@fn void ExtendedDynamicState2::create_descriptor_sets()
 * 	@brief Creating both descriptor set:
 * 		   1. Uniform buffer
 * 		   2. Image sampler
*/
void ExtendedDynamicState2::create_descriptor_sets()
{
	VkDescriptorSetAllocateInfo alloc_info =
	    vkb::initializers::descriptor_set_allocate_info(
	        descriptor_pool,
	        &descriptor_set_layouts.baseline,
	        1);

	VK_CHECK(vkAllocateDescriptorSets(get_device().get_handle(), &alloc_info, &descriptor_sets.baseline));

	VkDescriptorBufferInfo            matrix_buffer_descriptor = create_descriptor(*uniform_buffers.baseline);
	std::vector<VkWriteDescriptorSet> write_descriptor_sets    = {
        vkb::initializers::write_descriptor_set(descriptor_sets.baseline, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &matrix_buffer_descriptor)};
	vkUpdateDescriptorSets(get_device().get_handle(), static_cast<uint32_t>(write_descriptor_sets.size()), write_descriptor_sets.data(), 0, nullptr);
	/* Dodac wersje dla modelu*/

	alloc_info = vkb::initializers::descriptor_set_allocate_info(descriptor_pool, &descriptor_set_layouts.model, 1);

	VK_CHECK(vkAllocateDescriptorSets(get_device().get_handle(), &alloc_info, &descriptor_sets.model));

	VkDescriptorBufferInfo model_buffer_descriptor = create_descriptor(*uniform_buffers.model_tessellation);

	write_descriptor_sets =
	    {
	        // Binding 0 : Shared tessellation shader ubo
	        vkb::initializers::write_descriptor_set(
	            descriptor_sets.model,
	            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
	            0,
	            &model_buffer_descriptor)};
	vkUpdateDescriptorSets(get_device().get_handle(), static_cast<uint32_t>(write_descriptor_sets.size()), write_descriptor_sets.data(), 0, NULL);
}

void ExtendedDynamicState2::request_gpu_features(vkb::PhysicalDevice &gpu)
{
	/* Enable extension features required by this sample
	   These are passed to device creation via a pNext structure chain */
	auto &requested_extended_dynamic_state2_features                                   = gpu.request_extension_features<VkPhysicalDeviceExtendedDynamicState2FeaturesEXT>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_2_FEATURES_EXT);
	requested_extended_dynamic_state2_features.extendedDynamicState2                   = VK_TRUE;
	requested_extended_dynamic_state2_features.extendedDynamicState2LogicOp            = VK_TRUE;
	requested_extended_dynamic_state2_features.extendedDynamicState2PatchControlPoints = VK_TRUE;

	auto &requested_extended_dynamic_state_feature                = gpu.request_extension_features<VkPhysicalDeviceExtendedDynamicStateFeaturesEXT>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT);
	requested_extended_dynamic_state_feature.extendedDynamicState = VK_TRUE;

	// Tessellation shader support is required for this example
	auto &requested_features = gpu.get_mutable_requested_features();
	if (gpu.get_features().tessellationShader)
	{
		requested_features.tessellationShader = VK_TRUE;
	}
	else
	{
		throw vkb::VulkanException(VK_ERROR_FEATURE_NOT_PRESENT, "Selected GPU does not support tessellation shaders!");
	}

	if (gpu.get_features().depthBiasClamp)
	{
		requested_features.depthBiasClamp = VK_TRUE;
	}
	else
	{
		throw vkb::VulkanException(VK_ERROR_FEATURE_NOT_PRESENT, "Selected GPU does not support depthBiasClamp!");
	}

	if (gpu.get_features().fillModeNonSolid)
	{
		requested_features.fillModeNonSolid = VK_TRUE;
	}

	if (gpu.get_features().samplerAnisotropy)
	{
		gpu.get_mutable_requested_features().samplerAnisotropy = true;
	}
}

void ExtendedDynamicState2::on_update_ui_overlay(vkb::Drawer &drawer)
{
	if (drawer.header("Settings"))
	{
		if (drawer.checkbox("Primitive Restart Enable", &gui_settings.primitive_restart_enable))
		{
			update_uniform_buffers();
		}
		if (drawer.checkbox("Tessellation Enable", &gui_settings.tessellation))
		{
			update_uniform_buffers();
		}
		if (drawer.input_float("Tessellation Factor", &gui_settings.tess_factor, 1.0f, 1))
		{
			update_uniform_buffers();
		}

		if (drawer.input_float("Patch Control Points", &gui_settings.patch_control_points_float, 1.0f, 1))
		{
			if (gui_settings.patch_control_points_float < 1)
			{
				gui_settings.patch_control_points_float = 1;
			}
			gui_settings.patch_control_points = (uint32_t) roundf(gui_settings.patch_control_points_float);
		}
		if (drawer.combo_box("Logic type", &gui_settings.logic_op_index, logic_op_object_names))
		{
			gui_settings.logicOp = (VkLogicOp) gui_settings.logic_op_index;
			update_uniform_buffers();
			build_command_buffers();
		}
	}
	if (drawer.header("Models"))
	{
		const char *indicators[] = {"1. ", "2. ", "3. ", "4. ", "5. ", "6. ", "7. ", "8. ", "9. ", "10. ", "11. ", "12. "};
		const char *col_names    = {"Index"};
		if (drawer.checkbox("Selection active", &gui_settings.selection_active))
		{
		}
		ImGui::Columns(2, col_names);
		ImGui::SetColumnWidth(0, 100);
		ImGui::ListBox("", &gui_settings.selected_obj, indicators, 12);
		ImGui::NextColumn();
		if (drawer.checkbox("Depth Bias Enable", &gui_settings.objects[gui_settings.selected_obj].depth_bias))
		{
		}
		if (drawer.checkbox("Rasterizer Discard", &gui_settings.objects[gui_settings.selected_obj].rasterizer_discard))
		{
		}
	}
}

void ExtendedDynamicState2::update(float delta_time)
{
	static float time_pass = 0;
	time_pass += delta_time;
	static glm::vec3 translation = scene_nodes[EDS_PIPELINE_BASELINE_INDEX].at(get_node_index("z_fight_1", &scene_nodes[EDS_PIPELINE_BASELINE_INDEX])).node->get_transform().get_translation();
	static float     difference  = 0;
	static bool      rising      = true;

	if (time_pass > 0.05)
	{
		if (difference < -0.030)
		{
			rising = true;
		}
		else if (difference > 0.030)
		{
			rising = false;
		}

		if (rising == true)
		{
			translation.x += 0.0005;
			difference += 0.0005;
		}
		else
		{
			translation.x -= 0.0005;
			difference -= 0.0005;
		}
		time_pass = 0;
		for (uint32_t i = 0; i < scene_nodes[EDS_PIPELINE_BASELINE_INDEX].size(); i++)
		{
			if (scene_nodes[EDS_PIPELINE_BASELINE_INDEX].at(i).node->get_name() == "z_fight_1")
			{
				scene_nodes[EDS_PIPELINE_BASELINE_INDEX].at(i).node->get_transform().set_translation(translation);
				break;
			}
		}
		gui_settings.time_tick = true;
		build_command_buffers();
	}

	ApiVulkanSample::update(delta_time);
}

uint32_t ExtendedDynamicState2::get_node_index(std::string name, std::vector<SceneNode> *scene_node)
{
	uint32_t i = 0;
	for (i = 0; i < scene_node->size(); i++)
	{
		if (scene_node->at(i).node->get_name() == name)
		{
			break;
		}
	}
	return i;
}

void ExtendedDynamicState2::selection_indicator(const vkb::sg::PBRMaterial *original_mat, vkb::sg::PBRMaterial *new_mat)
{
	static bool                        rise              = false;
	static int                         previous_obj_id   = gui_settings.selected_obj;
	static const vkb::sg::PBRMaterial *previous_material = original_mat;
	static float                       accumulated_diff  = 0.0;

	new_mat->base_color_factor = original_mat->base_color_factor;
	new_mat->alpha_mode        = vkb::sg::AlphaMode::Blend;

	if (rise == true)
	{
		if (gui_settings.time_tick == true)
		{
			accumulated_diff += 0.075;
			gui_settings.time_tick = false;
		}
		new_mat->base_color_factor.w += accumulated_diff;
	}
	else
	{
		if (gui_settings.time_tick == true)
		{
			accumulated_diff -= 0.075;
			gui_settings.time_tick = false;
		}
		new_mat->base_color_factor.w += accumulated_diff;
	}

	if (previous_obj_id != gui_settings.selected_obj)
	{
		accumulated_diff = 0.0;
		previous_obj_id  = gui_settings.selected_obj;
	}

	if (new_mat->base_color_factor.w < 0.3)
	{
		rise = true;
	}
	else if (new_mat->base_color_factor.w > 0.98)
	{
		rise = false;
	}
}

void ExtendedDynamicState2::scene_pipeline_divide(std::vector<std::vector<SceneNode>> *scene_node)
{
	int scene_nodes_cnt = scene_node->at(EDS_PIPELINE_ALL_INDEX).size();

	std::vector<SceneNode> scene_elements_baseline;
	std::vector<SceneNode> scene_elements_tess;

	for (int i = 0; i < scene_nodes_cnt; i++)
	{
		if (scene_node->at(EDS_PIPELINE_ALL_INDEX).at(i).name == "Suzanne")
		{
			scene_elements_tess.push_back(scene_node->at(EDS_PIPELINE_ALL_INDEX).at(i));
		}
		else
		{
			scene_elements_baseline.push_back(scene_node->at(EDS_PIPELINE_ALL_INDEX).at(i));
		}
	}

	scene_node->push_back(scene_elements_baseline);
	scene_node->push_back(scene_elements_tess);

}
/* Dokończyć tą funkcję !!!!! #TODO  */
void ExtendedDynamicState2::draw_from_scene(VkCommandBuffer command_buffer, std::vector<std::vector<SceneNode>> *scene_node, int scene_index)
{
	auto &node = scene_node->at(scene_index);
	uint32_t scene_elements_cnt = scene_node->at(scene_index).size(); // TODO test if correct 

	for(int i = 0 ; i < scene_elements_cnt ; i++)
	{
		const auto &vertex_buffer_pos    = node[i].sub_mesh->vertex_buffers.at("position");
		const auto &vertex_buffer_normal = node[i].sub_mesh->vertex_buffers.at("normal");
		auto &      index_buffer         = node[i].sub_mesh->index_buffer;

		if (gui_settings.objects[i].depth_bias == true)
		{
			vkCmdSetDepthBiasEnableEXT(command_buffer, VK_TRUE);
		}
		else
		{
			vkCmdSetDepthBiasEnableEXT(command_buffer, VK_FALSE);
		}

		if (gui_settings.objects[i].rasterizer_discard == true)
		{
			vkCmdSetRasterizerDiscardEnableEXT(command_buffer, VK_TRUE);
		}
		else
		{
			vkCmdSetRasterizerDiscardEnableEXT(command_buffer, VK_FALSE);
		}

		// Pass data for the current node via push commands
		auto node_material            = dynamic_cast<const vkb::sg::PBRMaterial *>(node[i].sub_mesh->get_material());
		push_const_block.model_matrix = node[i].node->get_transform().get_world_matrix();
		if (i != gui_settings.selected_obj ||
			gui_settings.selection_active == false)
		{
			push_const_block.color = node_material->base_color_factor;
		}
		else
		{
			vkb::sg::PBRMaterial temp_material{"Selected_Material"};
			selection_indicator(node_material, &temp_material);
			push_const_block.color = temp_material.base_color_factor;
		}
		vkCmdPushConstants(command_buffer, pipeline_layouts.baseline, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(push_const_block), &push_const_block);

		VkDeviceSize offsets[1] = {0};
		vkCmdBindVertexBuffers(command_buffer, 0, 1, vertex_buffer_pos.get(), offsets);
		vkCmdBindVertexBuffers(command_buffer, 1, 1, vertex_buffer_normal.get(), offsets);
		vkCmdBindIndexBuffer(command_buffer, index_buffer->get_handle(), 0, node[i].sub_mesh->index_type);

		vkCmdDrawIndexed(command_buffer, node[i].sub_mesh->vertex_indices, 1, 0, 0, 0);

	}
}

std::unique_ptr<vkb::VulkanSample> create_extended_dynamic_state2()
{
	return std::make_unique<ExtendedDynamicState2>();
}
