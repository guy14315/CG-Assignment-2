#version 330 core

out vec4 colour;

in vec2 TexCoord;
in vec3 Normal;
in vec3 FragPos;

uniform vec3 lightColour;
uniform vec3 lightPos;
uniform vec3 viewPos;

uniform sampler2D texture_data;

void main()
{
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);

    float dist = length(lightPos - FragPos);
    float attenuation = 1.0 / (1.0 + 0.09 * dist + 0.032 * dist * dist);

    float ambientStrength = 0.25;
    float diffuseStrength = 1.2;
    float specularStrength = 0.3;
    float shininess = 16.0;

    vec3 ambient = ambientStrength * lightColour;
    vec3 diffuse = diffuseStrength * max(dot(norm, lightDir), 0.0) * lightColour;
    vec3 specular = specularStrength * pow(max(dot(reflectDir, viewDir), 0.0), shininess) * lightColour;

    colour = texture(texture_data, TexCoord) * vec4(ambient + (diffuse + specular) * attenuation, 1.0);
}
