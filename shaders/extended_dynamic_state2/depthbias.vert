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


layout(binding = 0) uniform UBO
{
	mat4  projection;
	mat4  modelview;
	mat4  skybox_modelview;
}ubo;

layout(location = 0) out vec3 outPos;
layout(location = 1) out vec3 outNormal;

layout(constant_id = 0) const int type = 0;
out gl_PerVertex
{
	vec4 gl_Position;
};

void main()
{

    switch (type)
	{
        case 0:
        {
	        gl_Position = ubo.projection * ubo.modelview * vec4(inPos * 0.2, 1.0);
            break;
        }

        case 1:
        {
	        gl_Position = ubo.projection * ubo.modelview * (vec4(inPos * 0.2, 1.0) + vec4(0,4,0,0));
            break;
        }
    }
	

	outNormal = mat3(ubo.modelview) * inNormal;


}
