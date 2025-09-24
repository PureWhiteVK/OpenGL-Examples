#include "lines.h"

#include <fmt/base.h>
#include <fmt/format.h>
#include <glbinding/gl/types.h>

using namespace gl;

void Lines::draw() {
  glBindVertexArray(vao);
  glDrawElements(GL_LINES, static_cast<GLsizei>(indices.size()),
                 GL_UNSIGNED_INT, 0);
}