#ifndef PRIMITIVE_H
#define PRIMITIVE_H

#include "line_loops.h"
#include "mesh.h"
#include "lines.h"
#include <memory>

std::unique_ptr<Mesh> get_icosphere_primitive(int subdivide_level);

std::unique_ptr<Mesh> get_disk_primitive(int subdivide_level);

std::unique_ptr<Mesh> get_simplex_disk_primitive(int subdivide_level);

std::unique_ptr<LineLoops> get_circle_primitive(int subdivide_level);

std::unique_ptr<Mesh> get_polygon_cone_primitive(int subdivide_level);

std::unique_ptr<Mesh> get_cylinder_primitive(int subdivide_level);

std::unique_ptr<Mesh> get_plane_primitive();

std::unique_ptr<Mesh> get_cube_primitive();

std::unique_ptr<Lines> get_cube_wireframe_primitive();

#endif // PRIMITIVE_H