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

ExtendedDynamicState2::ExtendedDynamicState2()
{
	title = "Extended Dynamic State2";

	add_instance_extension(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
	add_device_extension(VK_EXT_EXTENDED_DYNAMIC_STATE_2_EXTENSION_NAME);
	add_device_extension(VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME);
}

ExtendedDynamicState2::~ExtendedDynamicState2()
{
	if (device)
	{
		vkDestroySampler(get_device().get_handle(), textures.envmap.sampler, VK_NULL_HANDLE);
		textures = {};
		skybox.reset();
		object.reset();
		ubo.reset();

		vkDestroyPipeline(get_device().get_handle(), model_pipeline, VK_NULL_HANDLE);
		vkDestroyPipeline(get_device().get_handle(), skybox_pipeline, VK_NULL_HANDLE);
		vkDestroyPipelineLayout(get_device().get_handle(), pipeline_layout, VK_NULL_HANDLE);
		vkDestroyDescriptorSetLayout(get_device().get_handle(), descriptor_set_layout, VK_NULL_HANDLE);
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

#if VK_NO_PROTOTYPES
	VkInstance instance = get_device().get_gpu().get_instance().get_handle();
	assert(!!instance);
	vkCmdSetDepthBiasEnableEXT         = (PFN_vkCmdSetDepthBiasEnableEXT) vkGetInstanceProcAddr(instance, "vkCmdSetDepthBiasEnableEXT");
	vkCmdSetLogicOpEXT                 = (PFN_vkCmdSetLogicOpEXT) vkGetInstanceProcAddr(instance, "vkCmdSetLogicOpEXT");
	vkCmdSetPatchControlPointsEXT      = (PFN_vkCmdSetPatchControlPointsEXT) vkGetInstanceProcAddr(instance, "vkCmdSetPatchControlPointsEXT");
	vkCmdSetPrimitiveRestartEnableEXT  = (PFN_vkCmdSetPrimitiveRestartEnableEXT) vkGetInstanceProcAddr(instance, "vkCmdSetPrimitiveRestartEnableEXT");
	vkCmdSetRasterizerDiscardEnableEXT = (PFN_vkCmdSetRasterizerDiscardEnableEXT) vkGetInstanceProcAddr(instance, "vkCmdSetRasterizerDiscardEnableEXT");
	vkCmdSetPrimitiveTopologyEXT 	   = (PFN_vkCmdSetPrimitiveTopologyEXT) vkGetInstanceProcAddr(instance, "vkCmdSetPrimitiveTopologyEXT");
	if (!vkCmdSetDepthBiasEnableEXT)
	{
		throw std::runtime_error("Unable to dynamically load vkCmdSetDepthBiasEnableEXT");
	}
	if (!vkCmdSetLogicOpEXT)
	{
		throw std::runtime_error("Unable to dynamically load vkCmdSetLogicOpEXT");
	}
	if (!vkCmdSetPatchControlPointsEXT)
	{
		throw std::runtime_error("Unable to dynamically load vkCmdSetPatchControlPointsEXT");
	}
	if (!vkCmdSetPrimitiveRestartEnableEXT)
	{
		throw std::runtime_error("Unable to dynamically load vkCmdSetPrimitiveRestartEnableEXT");
	}
	if (!vkCmdSetRasterizerDiscardEnableEXT)
	{
		throw std::runtime_error("Unable to dynamically load vkCmdSetRasterizerDiscardEnableEXT");
	}
	if (!vkCmdSetPrimitiveTopologyEXT)
	{
		throw std::runtime_error("Unable to dynamically load vkCmdSetPrimitiveTopologyEXT");
	}
	
	/* TODO: make it better later */

#endif

	camera.type = vkb::CameraType::LookAt;
	camera.set_position({0.f, 0.f, -4.0f});
	camera.set_rotation({0.f, 0.f, 0.f});
	camera.set_perspective(60.f, (float) width / (float) height, 256.f, 0.1f);

	load_assets();
	prepare_uniform_buffers();
	model_data_creation();
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
	/* Models */
	skybox = load_model("scenes/cube.gltf");
	object = load_model("scenes/cube.gltf");

	/* Load HDR cube map */
	textures.envmap = load_texture_cubemap("textures/uffizi_rgba16f_cube.ktx", vkb::sg::Image::Color);
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
	ubo = std::make_unique<vkb::core::Buffer>(get_device(), sizeof(ubo_vs), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

	update_uniform_buffers();
}

/**
 * 	@fn void ExtendedDynamicState2::update_uniform_buffers()
 * 	@brief Updating data from application to GPU uniform buffer
 */
void ExtendedDynamicState2::update_uniform_buffers()
{
	ubo_vs.projection       = camera.matrices.perspective;
	ubo_vs.modelview        = camera.matrices.view * glm::mat4(1.f);
	ubo_vs.skybox_modelview = camera.matrices.view;
	ubo->convert_and_update(ubo_vs);

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

	VkPipelineColorBlendAttachmentState blend_attachment_state =
	    vkb::initializers::pipeline_color_blend_attachment_state(
	        0xf,
	        VK_FALSE);

	const auto color_attachment_state = vkb::initializers::pipeline_color_blend_attachment_state(0xf, VK_FALSE);

	VkPipelineColorBlendStateCreateInfo color_blend_state =
	    vkb::initializers::pipeline_color_blend_state_create_info(
	        1,
	        &blend_attachment_state);
	color_blend_state.attachmentCount = 1;
	color_blend_state.pAttachments    = &color_attachment_state;

	/* Note: Using Reversed depth-buffer for increased precision, so Greater depth values are kept */
	VkPipelineDepthStencilStateCreateInfo depth_stencil_state =
	    vkb::initializers::pipeline_depth_stencil_state_create_info(
	        VK_FALSE,
	        VK_FALSE,
	        VK_COMPARE_OP_GREATER);

	VkPipelineViewportStateCreateInfo viewport_state =
	    vkb::initializers::pipeline_viewport_state_create_info(1, 1, 0);

	VkPipelineMultisampleStateCreateInfo multisample_state =
	    vkb::initializers::pipeline_multisample_state_create_info(
	        VK_SAMPLE_COUNT_1_BIT,
	        0);

	std::vector<VkDynamicState> dynamic_state_enables = {
	    VK_DYNAMIC_STATE_VIEWPORT,
	    VK_DYNAMIC_STATE_SCISSOR,
		VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY_EXT,
		VK_DYNAMIC_STATE_PRIMITIVE_RESTART_ENABLE_EXT,
		VK_DYNAMIC_STATE_RASTERIZER_DISCARD_ENABLE_EXT};
	VkPipelineDynamicStateCreateInfo dynamic_state =
	    vkb::initializers::pipeline_dynamic_state_create_info(
	        dynamic_state_enables.data(),
	        static_cast<uint32_t>(dynamic_state_enables.size()),
	        0);

	// Vertex bindings an attributes for model rendering
	// Binding description
	std::vector<VkVertexInputBindingDescription> vertex_input_bindings = {
	    vkb::initializers::vertex_input_binding_description(0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX),
	};

	// Attribute descriptions
	std::vector<VkVertexInputAttributeDescription> vertex_input_attributes = {
	    vkb::initializers::vertex_input_attribute_description(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, pos)),            // Position
	    vkb::initializers::vertex_input_attribute_description(0, 1, VK_FORMAT_R32G32B32_SFLOAT,  offsetof(Vertex, normal)),        // Normal
	};

	VkPipelineVertexInputStateCreateInfo vertex_input_state = vkb::initializers::pipeline_vertex_input_state_create_info();
	vertex_input_state.vertexBindingDescriptionCount        = static_cast<uint32_t>(vertex_input_bindings.size());
	vertex_input_state.pVertexBindingDescriptions           = vertex_input_bindings.data();
	vertex_input_state.vertexAttributeDescriptionCount      = static_cast<uint32_t>(vertex_input_attributes.size());
	vertex_input_state.pVertexAttributeDescriptions         = vertex_input_attributes.data();

	std::array<VkPipelineShaderStageCreateInfo, 2> shader_stages{};
	shader_stages[0] = load_shader("vertex_dynamic_state/gbuffer.vert", VK_SHADER_STAGE_VERTEX_BIT);
	shader_stages[1] = load_shader("vertex_dynamic_state/gbuffer.frag", VK_SHADER_STAGE_FRAGMENT_BIT);

	/* Create graphics pipeline for dynamic rendering */
	VkFormat color_rendering_format = render_context->get_format();

	/* Provide information for dynamic rendering */
	VkPipelineRenderingCreateInfoKHR pipeline_create{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR};
	pipeline_create.pNext                   = VK_NULL_HANDLE;
	pipeline_create.colorAttachmentCount    = 1;
	pipeline_create.pColorAttachmentFormats = &color_rendering_format;
	pipeline_create.depthAttachmentFormat   = depth_format;
	pipeline_create.stencilAttachmentFormat = depth_format;

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
	graphics_create.stageCount          = static_cast<uint32_t>(shader_stages.size());
	graphics_create.pStages             = shader_stages.data();
	graphics_create.layout              = pipeline_layout;

	/* Skybox pipeline (background cube) */
	VkSpecializationInfo                    specialization_info;
	std::array<VkSpecializationMapEntry, 1> specialization_map_entries{};
	specialization_map_entries[0]        = vkb::initializers::specialization_map_entry(0, 0, sizeof(uint32_t));
	uint32_t shadertype                  = 0;
	specialization_info                  = vkb::initializers::specialization_info(1, specialization_map_entries.data(), sizeof(shadertype), &shadertype);
	shader_stages[0].pSpecializationInfo = &specialization_info;
	shader_stages[1].pSpecializationInfo = &specialization_info;

	graphics_create.pNext      = VK_NULL_HANDLE;
	graphics_create.renderPass = render_pass;

	vkCreateGraphicsPipelines(get_device().get_handle(), VK_NULL_HANDLE, 1, &graphics_create, VK_NULL_HANDLE, &skybox_pipeline);
	/* Object rendering pipeline */
	shadertype = 1;

	/* Enable depth test and write */
	depth_stencil_state.depthWriteEnable = VK_TRUE;
	depth_stencil_state.depthTestEnable  = VK_TRUE;
	/* Flip cull mode */
	rasterization_state.cullMode = VK_CULL_MODE_FRONT_BIT;
	vkCreateGraphicsPipelines(get_device().get_handle(), VK_NULL_HANDLE, 1, &graphics_create, VK_NULL_HANDLE, &model_pipeline);
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
		VK_CHECK(vkBeginCommandBuffer(draw_cmd_buffer, &command_begin));

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

		vkCmdBeginRenderPass(draw_cmd_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);


		VkViewport viewport = vkb::initializers::viewport((float) width, (float) height, 0.0f, 1.0f);
		vkCmdSetViewport(draw_cmd_buffer, 0, 1, &viewport);

		VkRect2D scissor = vkb::initializers::rect2D(static_cast<int>(width), static_cast<int>(height), 0, 0);
		vkCmdSetScissor(draw_cmd_buffer, 0, 1, &scissor);

		/* One descriptor set is used, and the draw type is toggled by a specialization constant */
		vkCmdBindDescriptorSets(draw_cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 0, 1, &descriptor_set, 0, nullptr);

		/* Update RasterizerDiscardEnableEXT extension */
		vkCmdSetRasterizerDiscardEnableEXT(draw_cmd_buffer, gui_settings.rasterizer_discard_enable);

		/* skybox */
		vkCmdSetPrimitiveTopologyEXT(draw_cmd_buffer, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
		vkCmdSetPrimitiveRestartEnableEXT(draw_cmd_buffer, VK_FALSE);

		vkCmdBindPipeline(draw_cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, skybox_pipeline);
		draw_model(skybox, draw_cmd_buffer);

		/* object */
		vkCmdSetPrimitiveTopologyEXT(draw_cmd_buffer, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP);
		vkCmdSetPrimitiveRestartEnableEXT(draw_cmd_buffer, VK_TRUE);

		vkCmdBindPipeline(draw_cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, model_pipeline);
		//draw_model(object, draw_cmd_buffer);
		draw_created_model(draw_cmd_buffer);

		/* UI */
		draw_ui(draw_cmd_buffer);
		
		vkCmdEndRenderPass(draw_cmd_buffer);

		VK_CHECK(vkEndCommandBuffer(draw_cmd_buffer));
	}
}

/**
 * 	@fn void ExtendedDynamicState2::create_descriptor_pool()
 * 	@brief Creating descriptor pool with size adjusted to use uniform buffer and image sampler
*/
void ExtendedDynamicState2::create_descriptor_pool()
{
	std::vector<VkDescriptorPoolSize> pool_sizes = {
	    vkb::initializers::descriptor_pool_size(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2),
	    vkb::initializers::descriptor_pool_size(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2)};
	uint32_t                   num_descriptor_sets = 4;
	VkDescriptorPoolCreateInfo descriptor_pool_create_info =
	    vkb::initializers::descriptor_pool_create_info(static_cast<uint32_t>(pool_sizes.size()), pool_sizes.data(), num_descriptor_sets);
	VK_CHECK(vkCreateDescriptorPool(get_device().get_handle(), &descriptor_pool_create_info, nullptr, &descriptor_pool));
}

/**
 * 	@fn void ExtendedDynamicState2::setup_descriptor_set_layout()
 * 	@brief Creating layout for descriptor sets
*/
void ExtendedDynamicState2::setup_descriptor_set_layout()
{
	std::vector<VkDescriptorSetLayoutBinding> set_layout_bindings = {
	    vkb::initializers::descriptor_set_layout_binding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0),
	    vkb::initializers::descriptor_set_layout_binding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1),
	};

	VkDescriptorSetLayoutCreateInfo descriptor_layout_create_info =
	    vkb::initializers::descriptor_set_layout_create_info(set_layout_bindings.data(), static_cast<uint32_t>(set_layout_bindings.size()));

	VK_CHECK(vkCreateDescriptorSetLayout(get_device().get_handle(), &descriptor_layout_create_info, nullptr, &descriptor_set_layout));

	VkPipelineLayoutCreateInfo pipeline_layout_create_info =
	    vkb::initializers::pipeline_layout_create_info(
	        &descriptor_set_layout,
	        1);

	VK_CHECK(vkCreatePipelineLayout(get_device().get_handle(), &pipeline_layout_create_info, nullptr, &pipeline_layout));
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
	        &descriptor_set_layout,
	        1);

	VK_CHECK(vkAllocateDescriptorSets(get_device().get_handle(), &alloc_info, &descriptor_set));

	VkDescriptorBufferInfo            matrix_buffer_descriptor     = create_descriptor(*ubo);
	VkDescriptorImageInfo             environment_image_descriptor = create_descriptor(textures.envmap);
	std::vector<VkWriteDescriptorSet> write_descriptor_sets        = {
        vkb::initializers::write_descriptor_set(descriptor_set, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &matrix_buffer_descriptor),
        vkb::initializers::write_descriptor_set(descriptor_set, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &environment_image_descriptor),
    };
	vkUpdateDescriptorSets(get_device().get_handle(), static_cast<uint32_t>(write_descriptor_sets.size()), write_descriptor_sets.data(), 0, nullptr);
}

void ExtendedDynamicState2::request_gpu_features(vkb::PhysicalDevice &gpu)
{
	/* Enable extension features required by this sample
	   These are passed to device creation via a pNext structure chain */
	auto &requested_extended_dynamic_state2_features                                   = gpu.request_extension_features<VkPhysicalDeviceExtendedDynamicState2FeaturesEXT>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_2_FEATURES_EXT);
	requested_extended_dynamic_state2_features.extendedDynamicState2                   = VK_TRUE;
	requested_extended_dynamic_state2_features.extendedDynamicState2LogicOp            = VK_TRUE;
	requested_extended_dynamic_state2_features.extendedDynamicState2PatchControlPoints = VK_TRUE;

	/* temporary solution to keep validation layer clean of bug that will be solved is next version of Vulkan SDK*/
	auto &requested_extended_dynamic_state_feature 									   = gpu.request_extension_features<VkPhysicalDeviceExtendedDynamicStateFeaturesEXT>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT);
	requested_extended_dynamic_state_feature.extendedDynamicState					   = VK_TRUE;

	if (gpu.get_features().samplerAnisotropy)
	{
		gpu.get_mutable_requested_features().samplerAnisotropy = true;
	}
}

void ExtendedDynamicState2::on_update_ui_overlay(vkb::Drawer &drawer)
{
	if (drawer.header("Settings"))
	{
		if (drawer.checkbox("Depth Bias Enable", &gui_settings.depth_bias_enable))
		{
			update_uniform_buffers();
		}
		if (drawer.checkbox("Rasterizer Discard Enable", &gui_settings.rasterizer_discard_enable))
		{
			update_uniform_buffers();
		}
		if (drawer.combo_box("Logic type", &gui_settings.logic_op_index, logic_op_object_names))
		{
			gui_settings.logicOp = (VkLogicOp) gui_settings.logic_op_index;
			update_uniform_buffers();
			build_command_buffers();
		}
		if (drawer.input_float("Patch Control Points", &gui_settings.patch_control_points_float, 1.0f, 1))
		{
			gui_settings.patch_control_points = (uint32_t) roundf(gui_settings.patch_control_points_float);
		}
	}
}

void ExtendedDynamicState2::draw_created_model(VkCommandBuffer commandBuffer)
{
	VkDeviceSize offsets[1] = {0};
	vkCmdBindVertexBuffers(commandBuffer, 0, 1, cube.vertices->get(), offsets);
	vkCmdBindIndexBuffer(commandBuffer, cube.indices->get_handle(), 0, VK_INDEX_TYPE_UINT32);
	vkCmdDrawIndexed(commandBuffer, cube.index_count, 1, 0, 0, 0);
}

void ExtendedDynamicState2::model_data_creation()
{
	constexpr uint32_t                     vertex_count = 8;
	std::array<Vertex, vertex_count> vertices;

	vertices[0].pos = {0.0f, 0.0f, 0.0f};
	vertices[1].pos = {1.0f, 0.0f, 0.0f};
	vertices[2].pos = {1.0f, 1.0f, 0.0f};
	vertices[3].pos = {0.0f, 1.0f, 0.0f};
	vertices[4].pos = {0.0f, 0.0f, 1.0f};
	vertices[5].pos = {1.0f, 0.0f, 1.0f};
	vertices[6].pos = {1.0f, 1.0f, 1.0f};
	vertices[7].pos = {0.0f, 1.0f, 1.0f};

	/* Normalized normal vectors for each face of cube */
	glm::vec3 Xp = {1.0, 0.0, 0.0};
	glm::vec3 Xm = {-1.0, 0.0, 0.0};
	glm::vec3 Yp = {0.0, 1.0, 0.0};
	glm::vec3 Ym = {0.0, -1.0, 0.0};
	glm::vec3 Zp = {0.0, 0.0, 1.0};
	glm::vec3 Zm = {0.0, 0.0, -1.0};

	/* Normalized normal vectors for each vertex (created by sum of corresponding faces) */
	vertices[0].normal = glm::normalize(Xm + Ym + Zm);
	vertices[1].normal = glm::normalize(Xp + Ym + Zm);
	vertices[2].normal = glm::normalize(Xp + Yp + Zm);
	vertices[3].normal = glm::normalize(Xm + Yp + Zm);
	vertices[4].normal = glm::normalize(Xm + Ym + Zp);
	vertices[5].normal = glm::normalize(Xp + Ym + Zp);
	vertices[6].normal = glm::normalize(Xp + Yp + Zp);
	vertices[7].normal = glm::normalize(Xm + Yp + Zp);


	/* Scaling and position transform */
	for (uint8_t i = 0; i < vertex_count; i++)
	{
		vertices[i].pos *= glm::vec3(10.0f, 10.0f, 10.0f);
		vertices[i].pos -= glm::vec3(5.0f, 5.0f, 5.0f);
	}

	constexpr uint32_t index_count        = 29;
	uint32_t           vertex_buffer_size = vertex_count * sizeof(Vertex);
	uint32_t           index_buffer_size  = index_count * sizeof(uint32_t);
	cube.index_count                      = index_count;

	/* Array with vertices indexes for corresponding triangles */
	std::array<uint32_t, index_count> indices{0, 4, 3, 7,
	                                          0xFFFFFFFF,
											  1, 0, 2, 3,
											  0xFFFFFFFF,
											  2, 6, 1, 5,
											  0xFFFFFFFF,
											  1, 5, 0, 4,
											  0xFFFFFFFF,
											  4, 5, 7, 6,
											  0xFFFFFFFF,
											  2, 3, 6, 7,};

	struct
	{
		VkBuffer       buffer;
		VkDeviceMemory memory;
	} vertex_staging, index_staging;

	vertex_staging.buffer = get_device().create_buffer(
	    VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
	    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
	    vertex_buffer_size,
	    &vertex_staging.memory,
	    vertices.data());

	index_staging.buffer = get_device().create_buffer(
	    VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
	    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
	    index_buffer_size,
	    &index_staging.memory,
	    indices.data());

	cube.vertices = std::make_unique<vkb::core::Buffer>(get_device(),
	                                                    vertex_buffer_size,
	                                                    VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
	                                                    VMA_MEMORY_USAGE_GPU_ONLY);

	cube.indices = std::make_unique<vkb::core::Buffer>(get_device(),
	                                                   index_buffer_size,
	                                                   VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
	                                                   VMA_MEMORY_USAGE_GPU_ONLY);

	/* Copy from staging buffers */
	VkCommandBuffer copy_command = device->create_command_buffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

	VkBufferCopy copy_region = {};

	copy_region.size = vertex_buffer_size;
	vkCmdCopyBuffer(
	    copy_command,
	    vertex_staging.buffer,
	    cube.vertices->get_handle(),
	    1,
	    &copy_region);

	copy_region.size = index_buffer_size;
	vkCmdCopyBuffer(
	    copy_command,
	    index_staging.buffer,
	    cube.indices->get_handle(),
	    1,
	    &copy_region);

	device->flush_command_buffer(copy_command, queue, true);

	vkDestroyBuffer(get_device().get_handle(), vertex_staging.buffer, nullptr);
	vkFreeMemory(get_device().get_handle(), vertex_staging.memory, nullptr);
	vkDestroyBuffer(get_device().get_handle(), index_staging.buffer, nullptr);
	vkFreeMemory(get_device().get_handle(), index_staging.memory, nullptr);
}

std::unique_ptr<vkb::VulkanSample> create_extended_dynamic_state2()
{
	return std::make_unique<ExtendedDynamicState2>();
}
