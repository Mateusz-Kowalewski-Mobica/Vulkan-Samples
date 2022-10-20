/* Copyright (c) 2022, Mobica Limited
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
#version 450

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;

layout(constant_id = 0) const int type = 0;

layout(binding = 0) uniform UBO
{
	mat4  projection;
	mat4  modelview;
	mat4  skybox_modelview;
	float modelscale;
	float tessellatedEdgeSize;
	float tessellationFactor;
	float displacementFactor;
	vec4 frustumPlanes[6];
}
ubo;

layout(location = 0) out vec3 outUVW;
layout(location = 1) out vec3 outPos;
layout(location = 2) out vec3 outNormal;
layout(location = 3) out vec3 outViewVec;
layout(location = 4) out vec3 outLightVec;
layout(location = 5) out mat4 outInvModelView;

out gl_PerVertex
{
	vec4 gl_Position;
};

void main()
{
}
