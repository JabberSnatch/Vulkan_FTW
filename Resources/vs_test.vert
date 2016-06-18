#version 400
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (location=0) in vec3 position;

layout(std140, binding = 0) uniform matrix_buffer 
{
        mat4 world;
        mat4 view;
		mat4 projection;        
} matrices;

out gl_PerVertex
{
	vec4 gl_Position;
};


void main(void)
{
	gl_Position = matrices.projection * matrices.view * matrices.world * vec4(position, 1);
	
	// GL->VK conventions
	gl_Position.y = -gl_Position.y;
	gl_Position.z = (gl_Position.z + gl_Position.w) / 2.0;
}
