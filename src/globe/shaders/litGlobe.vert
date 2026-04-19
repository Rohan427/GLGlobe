#version 130

in vec3 aPos;       // Position (x, y, z)
in vec3 aNormal;    // Normal (x, y, z) - usually same as aPos for a unit sphere
in vec2 aTexCoord;  // UV (u, v)

out vec2 TexCoord;
out vec3 Normal;
out vec3 FragPos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    // Calculate world position for lighting
    FragPos = vec3 (model * vec4(aPos, 1.0));
    
    // Transform normals to match rotation (Normal Matrix)
    Normal = mat3 (transpose (inverse (model))) * aNormal;  
    
    TexCoord = aTexCoord;
    gl_Position = projection * view * vec4 (FragPos, 1.0);
}
