#version 460 core

out vec4 o_color;
in vec4 v_color;
in vec2 v_texcoord;
in vec2 v_scale;
in float v_tex_index;
in vec4 v_border_color;
in float v_border_width;
uniform sampler2D u_textures[32];

void main() {
    vec4 color;
    if(v_tex_index == -1) {
        color = v_color;
    } else {
        color = texture(u_textures[int(v_tex_index)], v_texcoord) * v_color;
    }
    if(v_border_width == 0.0f) {
            o_color = color;
    } else {
        vec2 texel = 1.0 / v_scale;
        // Calculate the border
        float borderX = v_border_width / v_scale.x;
        float borderY = v_border_width / v_scale.y;

        // Check if the current pixel is within the border region
        bool inBorder = v_texcoord.x < borderX || v_texcoord.x > 1.0 - borderX || v_texcoord.y < borderY || v_texcoord.y > 1.0 - borderY;

        // Set the output color depending on whether the current pixel is in the border region
        o_color = inBorder ? v_border_color : color;
    }
}
