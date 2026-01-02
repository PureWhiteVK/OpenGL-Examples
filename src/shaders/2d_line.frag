#version 330 core
out vec4 FragColor;
uniform vec2 framebuffer_size;

void main()
{
  vec2 coord = gl_FragCoord.xy / framebuffer_size;
  float target_coord = 0.5;
  float line_width = 0.01;
  float draw_line = float(coord.x >= target_coord - line_width * 0.5 && coord.x <= target_coord + line_width * 0.5);
  FragColor = vec4(draw_line,0.0,0.0,1.0);
}