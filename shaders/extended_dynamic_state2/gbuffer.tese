#version 450
/* Copyright (c) 2019, Sascha Willems
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

layout(binding = 0) uniform UBO
{
	mat4  projection;
	mat4  modelview;
	mat4  skybox_modelview;
	float modelscale;
	float tessellatedEdgeSize;
	float tessellationFactor;
	float displacementFactor;
	vec2 viewportDim;
	vec4  frustumPlanes[6];
}ubo;

//layout (set = 0, binding = 1) uniform sampler2D displacementMap; 

layout(quads, equal_spacing, cw) in;

layout (location = 0) in vec2 inUV[];
layout (location = 1) in vec3 inNormal[];
layout (location = 2) in vec3 inPos[];
 
layout (location = 0) out vec2 outUV; 
layout (location = 1) out vec3 outNormal;
layout (location = 2) out vec3 outPos;
layout (location = 3) out vec3 outViewVec;
layout (location = 4) out vec3 outLightVec;
layout (location = 5) out mat4 outInvModelView;


void main()
{
	// Interpolate UV coordinates
	vec2 uv1 = mix(inUV[0], inUV[1], gl_TessCoord.x);
	vec2 uv2 = mix(inUV[3], inUV[2], gl_TessCoord.x);
	outUV = mix(uv1, uv2, gl_TessCoord.y);

	vec3 n1 = mix(inNormal[0], inNormal[1], gl_TessCoord.x);
	vec3 n2 = mix(inNormal[3], inNormal[2], gl_TessCoord.x);
	outNormal = mix(n1, n2, gl_TessCoord.y);

	// Interpolate positions
	vec4 pos1 = mix(gl_in[0].gl_Position, gl_in[1].gl_Position, gl_TessCoord.x);
	vec4 pos2 = mix(gl_in[3].gl_Position, gl_in[2].gl_Position, gl_TessCoord.x);
	vec4 pos = mix(pos1, pos2, gl_TessCoord.y);
	// Displace
	//pos.y -= textureLod(displacementMap, outUV, 0.0).r * ubo.displacementFactor;
	// Perspective projection
	gl_Position = ubo.projection * ubo.modelview * pos;

	// Calculate vectors for lighting based on tessellated position
    vec3 lightPos = vec3(0.0f, -5.0f, 5.0f);
    outPos = pos.xyz;
	outViewVec = -pos.xyz;
	outLightVec = normalize(lightPos.xyz + outViewVec);
    outInvModelView = inverse(ubo.skybox_modelview);
	// outWorldPos = pos.xyz;
	// outEyePos = vec3(ubo.modelview * pos);
}