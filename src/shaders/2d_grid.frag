#version 330 core
out vec4 FragColor;
uniform vec2 framebuffer_size;

const vec3 background = vec3(0.2);

void main()
{
  const float grid_scale = 4.0;
  vec2 coord = gl_FragCoord.xy / framebuffer_size;
  coord = coord * 2.0 - 1.0;
  vec2 scaled_coord = coord * grid_scale;
  vec2 coord_pixel = fwidth(scaled_coord);
  vec2 dist = -abs(fract(scaled_coord) - 0.5) + 0.5;
  vec2 pixel_size_scale = 1.0 / coord_pixel;
  vec2 dist_pixel = dist * pixel_size_scale;
  vec2 axis_pixel = abs(scaled_coord) * pixel_size_scale;
  // float grid = min(dist.x,dist.y);
  float grid = min(dist_pixel.x,dist_pixel.y);
  // float line = smoothstep(0.01,0.0,grid);
  float grid_thickness = 2.0;
  float line = smoothstep(grid_thickness,0.0,grid);
  float axis1 = smoothstep(grid_thickness,0.0,axis_pixel.x);
  float axis2 = smoothstep(grid_thickness,0.0,axis_pixel.y);
  vec3 grid_color = vec3(0.1);
  if(axis2 > 0.0) {
    line = axis2;
    grid_color = vec3(0.0,1.0,0.0);
  }
  if(axis1 > 0.0) {
    line = axis1;
    grid_color = vec3(1.0,0.0,0.0);
  }
  // float line = grid < 2.0 ? 1.0 : 0.0;
  // FragColor = vec4(dist,0.0,1.0);
  // FragColor = vec4(line,0.0,0.0,1.0);
  FragColor = vec4((1.0-line) * background + line * grid_color,1.0);
}