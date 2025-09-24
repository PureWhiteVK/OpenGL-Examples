#ifndef LINES_H
#define LINES_H

#include "mesh.h"

class Lines: public Mesh {
public:
  void draw() override;  
};

#endif//LINES_H