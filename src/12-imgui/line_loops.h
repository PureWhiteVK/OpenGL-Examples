#ifndef LINE_LOOPS_H
#define LINE_LOOPS_H

#include "drawable.h"
#include "mesh.h"

class LineLoops : public Drawable {
public:
  void create_buffer() override;
  void destroy_buffer() override;
  void draw() override;

  LineLoops() = default;
  ~LineLoops() override;

public:
  std::vector<Vertex> vertices{};
  gl::GLuint vao{}, vbo{};
};

#endif // LINE_LOOPS_H