#version 330 core
layout(location = 0) in vec2 a_position;
layout(location = 1) in vec4 a_color;
layout(location = 2) in vec2 a_uv;

uniform vec2 u_translation;
uniform vec2 u_viewport;

out vec4 v_color;
out vec2 v_uv;

void main() {
  vec2 p = a_position + u_translation;
  gl_Position = vec4(p.x / u_viewport.x * 2.0 - 1.0, 1.0 - p.y / u_viewport.y * 2.0, 0.0, 1.0);
  v_color = a_color;
  v_uv = a_uv;
}