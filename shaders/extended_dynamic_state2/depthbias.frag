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
layout(location = 2) in vec3 inLightColor;

layout(location = 0) out vec4 outColor0;

layout(constant_id = 0) const int type = 0;

void main()
{


        switch (type)
	{
        case 0:
        {
	        outColor0        = vec4(inLightColor, 1.0);
            break;
        }

        case 1:
        {
	        outColor0        = vec4(inLightColor, 1.0);
            break;
        }

        case 2:
        {
            outColor0.xyz = vec3(0.9451, 0.0275, 0.0275);
            break;
        }
    }



}