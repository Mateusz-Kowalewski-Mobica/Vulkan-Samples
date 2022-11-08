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

layout (set = 0, binding = 0) uniform UBO 
{
	mat4 projection;
	mat4 modelview;
	vec4 lightPos;
	vec4 frustumPlanes[6];
	float displacementFactor;
	float tessellationFactor;
	vec2 viewportDim;
	float tessellatedEdgeSize;
	float modelscale;
} ubo; 

//layout (set = 0, binding = 1) uniform sampler2D displacementMap; 
layout(triangles, equal_spacing, cw) in;

layout (location = 0) in vec3 inPos[];
layout (location = 1) in vec3 inNormal[];
layout (location = 2) in vec2 inUV[];

 
layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec2 outUV;
layout (location = 2) out vec3 outViewVec;
layout (location = 3) out vec3 outLightVec;
layout (location = 4) out vec3 outEyePos;
layout (location = 5) out vec3 outWorldPos;

vec4 interpolate(vec4 v0, vec4 v1, vec4 v2, vec4 v3)
{
	vec4 a = mix(v0, v1, gl_TessCoord.x);
	vec4 b = mix(v3, v2, gl_TessCoord.x);
	return mix(a, b, gl_TessCoord.y);
}
vec3 interpolate(vec3 v0, vec3 v1, vec3 v2, vec3 v3)
{
	vec3 a = mix(v0, v1, gl_TessCoord.x);
	vec3 b = mix(v3, v2, gl_TessCoord.x);
	return mix(a, b, gl_TessCoord.y);
}
vec2 interpolate(vec2 v0, vec2 v1, vec2 v2, vec2 v3)
{
	vec2 a = mix(v0, v1, gl_TessCoord.x);
	vec2 b = mix(v3, v2, gl_TessCoord.x);
	return mix(a, b, gl_TessCoord.y);
}

vec2 interpolate3D(vec2 v0, vec2 v1, vec2 v2)
{
    return vec2(gl_TessCoord.x) * v0 + vec2(gl_TessCoord.y) * v1 + vec2(gl_TessCoord.z) * v2;
}

vec3 interpolate3D(vec3 v0, vec3 v1, vec3 v2)
{
    return vec3(gl_TessCoord.x) * v0 + vec3(gl_TessCoord.y) * v1 + vec3(gl_TessCoord.z) * v2;
}

vec4 interpolate3D(vec4 v0, vec4 v1, vec4 v2)
{
    return vec4(gl_TessCoord.x) * v0 + vec4(gl_TessCoord.y) * v1 + vec4(gl_TessCoord.z) * v2;
}


void main()
{
	// Interpolate UV coordinates
	outUV = interpolate3D(inUV[0], inUV[1], inUV[2]);

	outNormal = interpolate3D(inNormal[0], inNormal[1], inNormal[2]);

	//vec4 pos = interpolate(gl_in[0].gl_Position, gl_in[1].gl_Position, gl_in[2].gl_Position, gl_in[3].gl_Position);
	//outUV = interpolate(inUV[0], inUV[1], inUV[2], inUV[3]);
	//outNormal = interpolate(inNormal[0], inNormal[1], inNormal[2], inNormal[3]);

	// // Interpolate positions

	//  float u = gl_TessCoord.x;
    //  float v = gl_TessCoord.y;
	//     // retrieve control point position coordinates
    // vec4 p00 = gl_in[0].gl_Position;
    // vec4 p01 = gl_in[1].gl_Position;
    // vec4 p10 = gl_in[2].gl_Position;
    // vec4 p11 = gl_in[3].gl_Position;

    // // compute patch surface normal
    // vec4 uVec = p01 - p00;
    // vec4 vVec = p10 - p00;
    // vec4 normal = normalize( vec4(cross(vVec.xyz, uVec.xyz), 0) );

    // // bilinearly interpolate position coordinate across patch
    // vec4 p0 = (p01 - p00) * u + p00;
    // vec4 p1 = (p11 - p10) * u + p10;
    // vec4 pos = (p1 - p0) * v + p0;

	vec4 pos = interpolate3D(gl_in[0].gl_Position, gl_in[1].gl_Position, gl_in[2].gl_Position);
	
	// Displace
	// pos.y -= textureLod(displacementMap, outUV, 0.0).r * ubo.displacementFactor;
	//pos.z -= gl_TessCoord.x * 5.0;
	// Perspective projection
	pos.xyz *=  ubo.modelscale;
	gl_Position = ubo.projection * ubo.modelview * pos ;
	gl_PointSize = 4;

	// Calculate vectors for lighting based on tessellated position
	outViewVec = -pos.xyz;
	outLightVec = normalize(ubo.lightPos.xyz + outViewVec);
	outWorldPos = pos.xyz;
	outEyePos = vec3(ubo.modelview * pos);
}