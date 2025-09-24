#ifndef DRAWABLE_H
#define DRAWABLE_H

class Drawable {
public:
  Drawable() = default;
  virtual ~Drawable() = default;
  virtual void create_buffer() = 0;
  virtual void destroy_buffer() = 0;
  virtual void draw() = 0;
};

#endif//DRAWABLE_H