#include "line_loops.h"

#include <fmt/base.h>
#include <fmt/format.h>
#include <glbinding/gl/functions.h>

using namespace gl;

void LineLoops::create_buffer() {
  glGenVertexArrays(1, &vao);
  glGenBuffers(1, &vbo);

  glBindVertexArray(vao);

  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * vertices.size(),
               vertices.data(), GL_STATIC_DRAW);

  // position
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                        (void *)offsetof(Vertex, pos));
  // normal
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                        (void *)offsetof(Vertex, normal));
  // texcoord
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                        (void *)offsetof(Vertex, texcoord));

  glBindVertexArray(0);
}

void LineLoops::destroy_buffer() {
  fmt::println("delete LineLoops vao: {} and vbo: {}", vao, vbo);
  glDeleteVertexArrays(1, &vao);
  glDeleteBuffers(1, &vbo);
  vao = 0;
  vbo = 0;
}

void LineLoops::draw() {
  glBindVertexArray(vao);
  glDrawArrays(GL_LINE_LOOP, 0, vertices.size());
}

LineLoops::~LineLoops() {}