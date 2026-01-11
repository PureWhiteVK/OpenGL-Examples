// clang-format off
#include "pipeline.h"

#include <iostream>
#include <limits>

#include "lib/log.h"
#include "lib/mathlib.h"
#include "lib/vec3.h"
#include "rasterizer/framebuffer.h"
#include "rasterizer/sample_pattern.h"
#include "geometry/util.h"

template<PrimitiveType primitive_type, class Program, uint32_t flags>
void Pipeline<primitive_type, Program, flags>::run(std::vector<Vertex> const& vertices,
                                                   typename Program::Parameters const& parameters,
                                                   Framebuffer* framebuffer_) {
  // Framebuffer must be non-null:
  assert(framebuffer_);
	auto& framebuffer = *framebuffer_;

  // A1T7: sample loop
	// TODO: update this function to rasterize to *all* sample locations in the framebuffer.
  //  	 This will probably involve inserting a loop of the form:
	// 		 	std::vector< Vec3 > const &samples = framebuffer.sample_pattern.centers_and_weights;
  //      	for (uint32_t s = 0; s < samples.size(); ++s) { ... }
  //   	 around some subset of the code.
	// 		 You will also need to transform the input and output of the rasterize_* functions to
	// 	     account for the fact they deal with pixels centered at (0.5,0.5).

  std::vector<ShadedVertex> shaded_vertices;
  shaded_vertices.reserve(vertices.size());

  //--------------------------
  // shade vertices:
	for (auto const& v : vertices) {
    ShadedVertex sv;
		Program::shade_vertex(parameters, v.attributes, &sv.clip_position, &sv.attributes);
    shaded_vertices.emplace_back(sv);
  }

  //--------------------------
  // assemble + clip + homogeneous divide vertices:
  std::vector<ClippedVertex> clipped_vertices;

  // reserve some space to avoid reallocations later:
  if constexpr (primitive_type == PrimitiveType::Lines) {
    // clipping lines can never produce more than one vertex per input vertex:
    clipped_vertices.reserve(shaded_vertices.size());
  } else if constexpr (primitive_type == PrimitiveType::Triangles) {
    // clipping triangles can produce up to 8 vertices per input vertex:
    clipped_vertices.reserve(shaded_vertices.size() * 8);
  }
  // clang-format off

	//coefficients to map from clip coordinates to framebuffer (i.e., "viewport") coordinates:
	//x: [-1,1] -> [0,width]
	//y: [-1,1] -> [0,height]
	//z: [-1,1] -> [0,1] (OpenGL-style depth range)
	Vec3 const clip_to_fb_scale = Vec3{
		framebuffer.width / 2.0f,
		framebuffer.height / 2.0f,
		0.5f
	};
	Vec3 const clip_to_fb_offset = Vec3{
		0.5f * framebuffer.width,
		0.5f * framebuffer.height,
		0.5f
	};

	// helper used to put output of clipping functions into clipped_vertices:
	auto emit_vertex = [&](ShadedVertex const& sv) {
		ClippedVertex cv;
		float inv_w = 1.0f / sv.clip_position.w;
		cv.fb_position = clip_to_fb_scale * inv_w * sv.clip_position.xyz() + clip_to_fb_offset;
		cv.inv_w = inv_w;
		cv.attributes = sv.attributes;
		clipped_vertices.emplace_back(cv);
	};

	// actually do clipping:
	if constexpr (primitive_type == PrimitiveType::Lines) {
		for (uint32_t i = 0; i + 1 < shaded_vertices.size(); i += 2) {
			clip_line(shaded_vertices[i], shaded_vertices[i + 1], emit_vertex);
		}
	} else if constexpr (primitive_type == PrimitiveType::Triangles) {
		for (uint32_t i = 0; i + 2 < shaded_vertices.size(); i += 3) {
			clip_triangle(shaded_vertices[i], shaded_vertices[i + 1], shaded_vertices[i + 2], emit_vertex);
		}
	} else {
		static_assert(primitive_type == PrimitiveType::Lines, "Unsupported primitive type.");
	}

	//--------------------------
	// rasterize primitives:

	std::vector<Fragment> fragments;

	// helper used to put output of rasterization functions into fragments:
	auto emit_fragment = [&](Fragment const& f) { fragments.emplace_back(f); };

	// actually do rasterization:
	if constexpr (primitive_type == PrimitiveType::Lines) {
		for (uint32_t i = 0; i + 1 < clipped_vertices.size(); i += 2) {
			rasterize_line(clipped_vertices[i], clipped_vertices[i + 1], emit_fragment);
		}
	} else if constexpr (primitive_type == PrimitiveType::Triangles) {
		for (uint32_t i = 0; i + 2 < clipped_vertices.size(); i += 3) {
			rasterize_triangle(clipped_vertices[i], clipped_vertices[i + 1], clipped_vertices[i + 2], emit_fragment);
		}
	} else {
		static_assert(primitive_type == PrimitiveType::Lines, "Unsupported primitive type.");
	}

	//--------------------------
	// depth test + shade + blend fragments:
	uint32_t out_of_range = 0; // check if rasterization produced fragments outside framebuffer 
							   // (indicates something is wrong with clipping)
	for (auto const& f : fragments) {

		// fragment location (in pixels):
		int32_t x = (int32_t)std::floor(f.fb_position.x);
		int32_t y = (int32_t)std::floor(f.fb_position.y);

		// if clipping is working properly, this condition shouldn't be needed;
		// however, it prevents crashes while you are working on your clipping functions,
		// so we suggest leaving it in place:
		if (x < 0 || (uint32_t)x >= framebuffer.width || 
		    y < 0 || (uint32_t)y >= framebuffer.height) {
			++out_of_range;
			continue;
		}

		// local names that refer to destination sample in framebuffer:
		float& fb_depth = framebuffer.depth_at(x, y, 0);
		Spectrum& fb_color = framebuffer.color_at(x, y, 0);


		// depth test:
		if constexpr ((flags & PipelineMask_Depth) == Pipeline_Depth_Always) {
			// "Always" means the depth test always passes.
		} else if constexpr ((flags & PipelineMask_Depth) == Pipeline_Depth_Never) {
			// "Never" means the depth test never passes.
			continue; //discard this fragment
		} else if constexpr ((flags & PipelineMask_Depth) == Pipeline_Depth_Less) {
			// "Less" means the depth test passes when the new fragment has depth less than the stored depth.
			// A1T4: Depth_Less
			// TODO: implement depth test! We want to only emit fragments that have a depth less than the stored depth, hence "Depth_Less".
			if(fb_depth < f.fb_position.z) {
				continue;
			}
		} else {
			static_assert((flags & PipelineMask_Depth) <= Pipeline_Depth_Always, "Unknown depth test flag.");
		}

		// if depth test passes, and depth writes aren't disabled, write depth to depth buffer:
		if constexpr (!(flags & Pipeline_DepthWriteDisableBit)) {
			fb_depth = f.fb_position.z;
		}

		// shade fragment:
		ShadedFragment sf;
		sf.fb_position = f.fb_position;
		Program::shade_fragment(parameters, f.attributes, f.derivatives, &sf.color, &sf.opacity);

		// write color to framebuffer if color writes aren't disabled:
		if constexpr (!(flags & Pipeline_ColorWriteDisableBit)) {
			// blend fragment:
			if constexpr ((flags & PipelineMask_Blend) == Pipeline_Blend_Replace) {
				fb_color = sf.color;
			} else if constexpr ((flags & PipelineMask_Blend) == Pipeline_Blend_Add) {
				// A1T4: Blend_Add
				// TODO: framebuffer color should have fragment color multiplied by fragment opacity added to it.
				fb_color += sf.color * sf.opacity;
			} else if constexpr ((flags & PipelineMask_Blend) == Pipeline_Blend_Over) {
				// A1T4: Blend_Over
				// TODO: set framebuffer color to the result of "over" blending (also called "alpha blending") the fragment color over the framebuffer color, using the fragment's opacity
				// 		 You may assume that the framebuffer color has its alpha premultiplied already, and you just want to compute the resulting composite color
				fb_color = (1.0f - sf.opacity) * fb_color + sf.opacity * sf.color;
			} else {
				static_assert((flags & PipelineMask_Blend) <= Pipeline_Blend_Over, "Unknown blending flag.");
			}
		}
	}
	if (out_of_range > 0) {
		if constexpr (primitive_type == PrimitiveType::Lines) {
			warn("Produced %d fragments outside framebuffer; this indicates something is likely "
			     "wrong with the clip_line function.",
			     out_of_range);
		} else if constexpr (primitive_type == PrimitiveType::Triangles) {
			warn("Produced %d fragments outside framebuffer; this indicates something is likely "
			     "wrong with the clip_triangle function.",
			     out_of_range);
		}
	}
}

// -------------------------------------------------------------------------
// clipping functions

// helper to interpolate between vertices:
template<PrimitiveType p, class P, uint32_t F>
auto Pipeline<p, P, F>::lerp(ShadedVertex const& a, ShadedVertex const& b, float t) -> ShadedVertex {
	ShadedVertex ret;
	ret.clip_position = (b.clip_position - a.clip_position) * t + a.clip_position;
	for (uint32_t i = 0; i < ret.attributes.size(); ++i) {
		ret.attributes[i] = (b.attributes[i] - a.attributes[i]) * t + a.attributes[i];
	}
	return ret;
}

/*
 * clip_line - clip line to portion with -w <= x,y,z <= w, emit vertices of clipped line (if non-empty)
 *  	va, vb: endpoints of line
 *  	emit_vertex: call to produce truncated line
 *
 * If clipping shortens the line, attributes of the shortened line should respect the pipeline's interpolation mode.
 * 
 * If no portion of the line remains after clipping, emit_vertex will not be called.
 *
 * The clipped line should have the same direction as the full line.
 */
template<PrimitiveType p, class P, uint32_t flags>
void Pipeline<p, P, flags>::clip_line(ShadedVertex const& va, ShadedVertex const& vb,
                                      std::function<void(ShadedVertex const&)> const& emit_vertex) {
	// Determine portion of line over which:
	// 		pt = (b-a) * t + a
	//  	-pt.w <= pt.x <= pt.w
	//  	-pt.w <= pt.y <= pt.w
	//  	-pt.w <= pt.z <= pt.w
	// ... as a range [min_t, max_t]:

	float min_t = 0.0f;
	float max_t = 1.0f;

	// want to set range of t for a bunch of equations like:
	//    a.x + t * ba.x <= a.w + t * ba.w
	// so here's a helper:
	auto clip_range = [&min_t, &max_t](float l, float dl, float r, float dr) {
		// restrict range such that:
		// l + t * dl <= r + t * dr
		// re-arranging:
		//  l - r <= t * (dr - dl)
		if (dr == dl) {
			// want: l - r <= 0
			if (l - r > 0.0f) {
				// works for none of range, so make range empty:
				min_t = 1.0f;
				max_t = 0.0f;
			}
		} else if (dr > dl) {
			// since dr - dl is positive:
			// want: (l - r) / (dr - dl) <= t
			min_t = std::max(min_t, (l - r) / (dr - dl));
		} else { // dr < dl
			// since dr - dl is negative:
			// want: (l - r) / (dr - dl) >= t
			max_t = std::min(max_t, (l - r) / (dr - dl));
		}
	};

	// local names for clip positions and their difference:
	Vec4 const& a = va.clip_position;
	Vec4 const& b = vb.clip_position;
	Vec4 const ba = b - a;

	// -a.w - t * ba.w <= a.x + t * ba.x <= a.w + t * ba.w
	clip_range(-a.w, -ba.w, a.x, ba.x);
	clip_range(a.x, ba.x, a.w, ba.w);
	// -a.w - t * ba.w <= a.y + t * ba.y <= a.w + t * ba.w
	clip_range(-a.w, -ba.w, a.y, ba.y);
	clip_range(a.y, ba.y, a.w, ba.w);
	// -a.w - t * ba.w <= a.z + t * ba.z <= a.w + t * ba.w
	clip_range(-a.w, -ba.w, a.z, ba.z);
	clip_range(a.z, ba.z, a.w, ba.w);

	if (min_t < max_t) {
		if (min_t == 0.0f) {
			emit_vertex(va);
		} else {
			ShadedVertex out = lerp(va, vb, min_t);
			// don't interpolate attributes if in flat shading mode:
			if constexpr ((flags & PipelineMask_Interp) == Pipeline_Interp_Flat) {
				out.attributes = va.attributes;
			}
			emit_vertex(out);
		}
		if (max_t == 1.0f) {
			emit_vertex(vb);
		} else {
			ShadedVertex out = lerp(va, vb, max_t);
			// don't interpolate attributes if in flat shading mode:
			if constexpr ((flags & PipelineMask_Interp) == Pipeline_Interp_Flat) {
				out.attributes = va.attributes;
			}
			emit_vertex(out);
		}
	}
}

/*
 * clip_triangle - clip triangle to portion with -w <= x,y,z <= w, emit resulting shape as triangles (if non-empty)
 *  	va, vb, vc: vertices of triangle
 *  	emit_vertex: call to produce clipped triangles (three calls per triangle)
 *
 * If clipping truncates the triangle, attributes of the new vertices should respect the pipeline's interpolation mode.
 * 
 * If no portion of the triangle remains after clipping, emit_vertex will not be called.
 *
 * The clipped triangle(s) should have the same winding order as the full triangle.
 */
template<PrimitiveType p, class P, uint32_t flags>
void Pipeline<p, P, flags>::clip_triangle(
	ShadedVertex const& va, ShadedVertex const& vb, ShadedVertex const& vc,
	std::function<void(ShadedVertex const&)> const& emit_vertex) {
	// A1EC: clip_triangle
	// TODO: correct code!
	// emit_vertex(va);
	// emit_vertex(vb);
	// emit_vertex(vc);
	// return;

	std::vector<ShadedVertex> curr_vertices{va, vb, vc};
  static std::array<Vec4, 6> planes{
      // -w <= x -> 0 <= x + w;
      Vec4{1.0f, 0.0f, 0.0f, 1.0f},
      // x <= w -> 0 <= w - x;
      Vec4{-1.0f, 0.0f, 0.0f, 1.0f},
      // -w <= y -> 0 <= y + w;
      Vec4{0.0f, 1.0f, 0.0f, 1.0f},
      // y <= w -> 0 <= w - y;
      Vec4{0.0f, -1.0f, 0.0f, 1.0f},
      // -w <= z -> 0 <= z + w;
      Vec4{0.0f, 0.0f, 1.0f, 1.0f},
      // z <= w -> 0 <= w - z;
      Vec4{0.0f, 0.0f, -1.0f, 1.0f},
  };

  auto plane_intersection = [](ShadedVertex v0, ShadedVertex v1, Vec4 plane,
                               ShadedVertex &out) -> int {
    float d0 = dot(v0.clip_position, plane);
    float d1 = dot(v1.clip_position, plane);
    int intersection_case = ((d0 >= -1e-6f) << 1) | (d1 >= -1e-6f);
    // 0b00 -> both outside
    // 0b01 -> v0 outside, v1 inside
    // 0b10 -> v0 inside, v1 outside
    // 0b11 -> both inside
    switch (intersection_case) {
    case 0b00: {
      break;
    }
    case 0b01:
      [[fallthrough]];
    case 0b10: {
      float t = d0 / (d0 - d1);
      out = lerp(v0, v1, t);
      break;
    }
    case 0b11: {
      out = v1;
      break;
    }
    }
    return intersection_case;
  };

  for (const Vec4 &plane : planes) {
    std::vector<ShadedVertex> next_vertices{};
    for (int i = 0; i < curr_vertices.size(); i++) {
      // vn,vn+1
      int j = (i + 1) % curr_vertices.size();
      ShadedVertex curr_vertex{};
      int intersection_case = plane_intersection(
          curr_vertices[i], curr_vertices[j], plane, curr_vertex);
      switch (intersection_case) {
      // 0b00 -> both outside
      // 0b01 -> v0 outside, v1 inside
      // 0b10 -> v0 inside, v1 outside
      // 0b11 -> both inside
      case 0b01: {
        // here we should add both new vertex and end point, as this point is
        // also a valid point here
        next_vertices.emplace_back(std::move(curr_vertex));
        next_vertices.emplace_back(curr_vertices[j]);
        break;
      }
      case 0b10:
        [[fallthrough]];
      case 0b11: {
        next_vertices.emplace_back(std::move(curr_vertex));
        break;
      }
      }
    }
    curr_vertices = std::move(next_vertices);
    if (curr_vertices.size() < 3) {
      return;
    }
  }

  // then we can triangulate polygon here
  for (int i = 2; i < curr_vertices.size(); i++) {
    emit_vertex(curr_vertices[0]);
    emit_vertex(curr_vertices[i - 1]);
    emit_vertex(curr_vertices[i]);
  }
}

// -------------------------------------------------------------------------
// rasterization functions

/*
 * rasterize_line:
 * calls emit_fragment( frag ) for every pixel "covered" by the line (va.fb_position.xy, vb.fb_position.xy).
 *
 *    a pixel (x,y) is "covered" by the line if it exits the inscribed diamond:
 * 
 *        (x+0.5,y+1)
 *        /        \
 *    (x,y+0.5)  (x+1,y+0.5)
 *        \        /
 *         (x+0.5,y)
 *
 *    to avoid ambiguity, we consider diamonds to contain their left and bottom points
 *    but not their top and right points. 
 * 
 * 	  since 45 degree lines breaks this rule, our rule in general is to rasterize the line as if its
 *    endpoints va and vb were at va + (e, e^2) and vb + (e, e^2) where no smaller nonzero e produces 
 *    a different rasterization result. 
 *    We will not explicitly check for 45 degree lines along the diamond edges (this will be extra credit),
 *    but you should be able to handle 45 degree lines in every other case (such as starting from pixel centers)
 *
 * for each such diamond, pass Fragment frag to emit_fragment, with:
 *  - frag.fb_position.xy set to the center (x+0.5,y+0.5)
 *  - frag.fb_position.z interpolated linearly between va.fb_position.z and vb.fb_position.z
 *  - frag.attributes set to va.attributes (line will only be used in Interp_Flat mode)
 *  - frag.derivatives set to all (0,0)
 *
 * when interpolating the depth (z) for the fragments, you may use any depth the line takes within the pixel
 * (i.e., you don't need to interpolate to, say, the closest point to the pixel center)
 *
 * If you wish to work in fixed point, check framebuffer.h for useful information about the framebuffer's dimensions.
 */
template<PrimitiveType p, class P, uint32_t flags>
void Pipeline<p, P, flags>::rasterize_line(
	ClippedVertex const& va, ClippedVertex const& vb,
	std::function<void(Fragment const&)> const& emit_fragment) {
	if constexpr ((flags & PipelineMask_Interp) != Pipeline_Interp_Flat) {
		assert(0 && "rasterize_line should only be invoked in flat interpolation mode.");
	}
	// A1T2: rasterize_line

	// TODO: Check out the block comment above this function for more information on how to fill in
	// this function!
	// The OpenGL specification section 3.5 may also come in handy.

	auto gen_fragment = [&emit_fragment,&va,&vb](float x,float y) -> void {
		// // check intersection here
		// float t_min{}, t_max{};
		// if(!Util::line_diamond_intersection(start.fb_position.xy(), end.fb_position.xy(), Vec2{x,y}, t_min, t_max)) {
		// 	// info("shit! line does not intersect with pixel diamond!");
		// 	return;
		// };
		// // check if the endpoint is inside this pixel
		// if(std::abs(t_max - 1.0f) < 1e-8f && (std::abs(end.fb_position.x - x) + std::abs(end.fb_position.y - y) < 0.5f)) {
		// 	// info("shit! end point is inside the pixel");
		// 	return;
		// } 
		Fragment f{};
		Vec2 pos{ x,y };
		float t = dot((pos - va.fb_position.xy()),(vb.fb_position.xy() - va.fb_position.xy())) / (vb.fb_position.xy() - va.fb_position.xy()).norm_squared();
		float z = (1.0f - t) * va.fb_position.z + t * vb.fb_position.z;
		f.fb_position = Vec3{ x,y,z };
		f.derivatives.fill(Vec2{0.0f,0.0f});
		f.attributes = va.attributes;
		// info("generate fragment (%d,%d)",static_cast<int>(x-0.5f),static_cast<int>(y-0.5f));
		emit_fragment(f);
	};

	// Bresenham's Algorithm (float version)

	/* naive version 
	float x1 = va.fb_position.x;
	float y1 = va.fb_position.y;
	float x2 = vb.fb_position.x;
	float y2 = vb.fb_position.y;
	float delta_x = x2 - x1;
	float delta_y = y2 - y1;
	int x1_floor = static_cast<int>(std::floor(x1));
	int x2_floor = static_cast<int>(std::floor(x2));
	int y1_floor = static_cast<int>(std::floor(y1));
	int y2_floor = static_cast<int>(std::floor(y2));
	if(std::abs(delta_x) > std::abs(delta_y)) {
		if(x1 > x2) {
			std::swap(x1,x2);
			std::swap(y1,y2);
			std::swap(x1_floor,x2_floor);
			std::swap(y1_floor,y2_floor);
		}
		if(y1 > y2) {
			float k = delta_y / delta_x;
			float epsilon = - (y1 - y1_floor) - k * (1.0-(x1-x1_floor));
			int j = y1_floor; 
			for(int i=x1_floor;i<=x2_floor;i++) {
				gen_fragment(static_cast<float>(i) + 0.5f,static_cast<float>(j) + 0.5f);
				if(epsilon >= 0.0) {
					j -= 1;
					epsilon -= 1.0f;
				}
				epsilon -= k;
			}
		} else {
			float k = delta_y / delta_x;
			float epsilon = -(1.0 - (y1 - y1_floor) - k * (1.0-(x1-x1_floor)));
			int j = y1_floor; 
			for(int i=x1_floor;i<=x2_floor;i++) {
				gen_fragment(static_cast<float>(i) + 0.5f,static_cast<float>(j) + 0.5f);
				if(epsilon >= 0.0) {
					j += 1;
					epsilon -= 1.0f;
				}
				epsilon += k;
			}
		}
	} else {
		if(y1 > y2) {
			std::swap(x1,x2);
			std::swap(y1,y2);
			std::swap(x1_floor,x2_floor);
			std::swap(y1_floor,y2_floor);
		}
		if(x1 > x2) {
			float inv_k = delta_x / delta_y; 
			float epsilon =  - (x1-x1_floor) - (1.0-(y1-y1_floor)) * inv_k;
			int i = x1_floor;
			for(int j=y1_floor;j<=y2_floor;j++) {
				gen_fragment(static_cast<float>(i) + 0.5f,static_cast<float>(j) + 0.5f);
				if(epsilon >= 0.0) {
					i -= 1;
					epsilon -= 1.0f;
				}
				epsilon -= inv_k;
			}
		} else {
			float inv_k = delta_x / delta_y; 
			float epsilon = -(1.0-(x1-x1_floor)- (1.0-(y1-y1_floor)) * inv_k);
			int i = x1_floor;
			for(int j=y1_floor;j<=y2_floor;j++) {
				gen_fragment(static_cast<float>(i) + 0.5f,static_cast<float>(j) + 0.5f);
				if(epsilon >= 0.0) {
					i += 1;
					epsilon -= 1.0f;
				}
				epsilon += inv_k;
			}
		}
	}
	*/

	/* updated version
	float x1 = va.fb_position.x;
	float x2 = vb.fb_position.x;
	float y1 = va.fb_position.y;
	float y2 = vb.fb_position.y;
	float delta_x = x2 - x1;
	float delta_y = y2 - y1;
	bool major_x = std::abs(delta_x) > std::abs(delta_y);
	bool swapped = false;
	if((major_x && (x1 > x2)) || (!major_x && (y1 > y2))) {
		swapped = true;
		std::swap(x1,x2);
		std::swap(y1,y2);
		delta_x = -delta_x;
		delta_y = -delta_y;
	}

	int x1_floor = static_cast<int>(std::floor(x1));
	int y1_floor = static_cast<int>(std::floor(y1));
	int x2_floor = static_cast<int>(std::floor(x2));
	int y2_floor = static_cast<int>(std::floor(y2));
	float x_offset = x1 - x1_floor;
	float y_offset = y1 - y1_floor;
	int minor_direction = major_x ? (y1 < y2 ? 1 : -1 ) : (x1 < x2 ? 1 : -1);
	int major_start = major_x ? x1_floor : y1_floor;
	int major_end = major_x ? x2_floor : y2_floor;
	int minor = major_x ? y1_floor : x1_floor;
	float major_offset = major_x ? x_offset : y_offset;
	float minor_offset = major_x ? y_offset : x_offset;
	float err_accum = std::abs(major_x ? (delta_y / delta_x) : (delta_x / delta_y));
	float err = (minor_direction == 1 ? -1.0f : 0.0f) + minor_direction * minor_offset + err_accum * (1.0f - major_offset);
	for(int major=major_start;major<=major_end;major++) {
		int x = major_x ? major : minor;
		int y = major_x ? minor : major;
		gen_fragment(swapped ? va : vb, swapped ? vb : va,static_cast<float>(x) + 0.5f,static_cast<float>(y) + 0.5f);
		if(err > 0.0f) {
			minor += minor_direction;
			err -= 1.0f;
		}
		err += err_accum;
	}
	*/

	// supercover line drawing algorithm (parameter stepping)
	float x1 = va.fb_position.x;
	float x2 = vb.fb_position.x;
	float y1 = va.fb_position.y;
	float y2 = vb.fb_position.y;
	float delta_x = x2 - x1;
	float delta_y = y2 - y1;
	float abs_delta_x = std::abs(delta_x);
	float abs_delta_y = std::abs(delta_y);

	constexpr float EPSILON = 1e-8f;

	if(abs_delta_x < EPSILON && abs_delta_y < EPSILON) {
		return;
	}

	int step_x = (delta_x > EPSILON) ? 1 : (delta_x < -EPSILON ? -1 : 0);
	int step_y = (delta_y > EPSILON) ? 1 : (delta_y < -EPSILON ? -1 : 0);

	// inc of t when x move 1 pixel
	float dt_dx = abs_delta_x > EPSILON ? 1.0f / abs_delta_x : std::numeric_limits<float>::infinity();
	// inc of t when y move 1 pixel
	float dt_dy = abs_delta_y > EPSILON ? 1.0f / abs_delta_y : std::numeric_limits<float>::infinity();

	int x1_floor = static_cast<int>(std::floor(x1));
	int y1_floor = static_cast<int>(std::floor(y1));
	int x2_floor = static_cast<int>(std::floor(x2));
	int y2_floor = static_cast<int>(std::floor(y2));

	int curr_x = x1_floor;
	int curr_y = y1_floor;

	// next grid line
	// if we move from big to small, the next is different
	float next_t_x = step_x * (curr_x + (step_x > 0 ? step_x : 0) - x1) * dt_dx;
	float next_t_y = step_y * (curr_y + (step_y > 0 ? step_y : 0) - y1) * dt_dy;

	auto evaluate_point = [=](float t) -> Vec2 {
    return Vec2{x1 + delta_x * t, y1 + delta_y * t};
  };
	
	while(curr_x != x2_floor || curr_y != y2_floor) {
		// Vec2 next_p_x = evaluate_point(next_t_x);
		// Vec2 next_p_y = evaluate_point(next_t_y);
		gen_fragment(static_cast<float>(curr_x) + 0.5f,static_cast<float>(curr_y) + 0.5f);
		if(next_t_x > next_t_y) {
			// move along y
			curr_y += step_y;
			next_t_y += dt_dy;
		} else if (next_t_x < next_t_y)  {
			// move along x
			curr_x += step_x;
			next_t_x += dt_dx;
		} else {
			curr_y += step_y;
			next_t_y += dt_dy;
			curr_x += step_x;
			next_t_x += dt_dx;
		}
	}
	gen_fragment(static_cast<float>(curr_x) + 0.5f,static_cast<float>(curr_y) + 0.5f);

/*
	{ // As a placeholder, draw a point in the middle of the line:
		//(remove this code once you have a real implementation)
		Fragment mid;
		mid.fb_position = (va.fb_position + vb.fb_position) / 2.0f;
		mid.attributes = va.attributes;
		mid.derivatives.fill(Vec2(0.0f, 0.0f));
		emit_fragment(mid);
	}
*/

}

/*
 * rasterize_triangle(a,b,c,emit) calls 'emit(frag)' at every location
 *  	(x+0.5,y+0.5) (where x,y are integers) covered by triangle (a,b,c).
 *
 * The emitted fragment should have:
 * - frag.fb_position.xy = (x+0.5, y+0.5)
 * - frag.fb_position.z = linearly interpolated fb_position.z from a,b,c (NOTE: does not depend on Interp mode!)
 * - frag.attributes = depends on Interp_* flag in flags:
 *   - if Interp_Flat: copy from va.attributes
 *   - if Interp_Smooth: interpolate as if (a,b,c) is a 2D triangle flat on the screen
 *   - if Interp_Correct: use perspective-correct interpolation
 * - frag.derivatives = derivatives w.r.t. fb_position.x and fb_position.y of the first frag.derivatives.size() attributes.
 *
 * Notes on derivatives:
 * 	The derivatives are partial derivatives w.r.t. screen locations. That is:
 *    derivatives[i].x = d/d(fb_position.x) attributes[i]
 *    derivatives[i].y = d/d(fb_position.y) attributes[i]
 *  You may compute these derivatives analytically or numerically.
 *
 *  See section 8.12.1 "Derivative Functions" of the GLSL 4.20 specification for some inspiration. (*HOWEVER*, the spec is solving a harder problem, and also nothing in the spec is binding on your implementation)
 *
 *  One approach is to rasterize blocks of four fragments and use forward and backward differences to compute derivatives.
 *  To assist you in this approach, keep in mind that the framebuffer size is *guaranteed* to be even. (see framebuffer.h)
 *
 * Notes on coverage:
 *  If two triangles are on opposite sides of the same edge, and a
 *  fragment center lies on that edge, rasterize_triangle should
 *  make sure that exactly one of the triangles emits that fragment.
 *  (Otherwise, speckles or cracks can appear in the final render.)
 * 
 *  For degenerate (co-linear) triangles, you may consider them to not be on any side of an edge.
 * 	Thus, even if two degnerate triangles share an edge that contains a fragment center, you don't need to emit it.
 *  You will not lose points for doing something reasonable when handling this case
 *
 *  This is pretty tricky to get exactly right!
 *
 */
template<PrimitiveType p, class P, uint32_t flags>
void Pipeline<p, P, flags>::rasterize_triangle(
	ClippedVertex const& va, ClippedVertex const& vb, ClippedVertex const& vc,
	std::function<void(Fragment const&)> const& emit_fragment) {
	// NOTE: it is okay to restructure this function to allow these tasks to use the
	//  same code paths. Be aware, however, that all of them need to remain working!
	//  (e.g., if you break Flat while implementing Correct, you won't get points
	//   for Flat.)
/*
	if constexpr ((flags & PipelineMask_Interp) == Pipeline_Interp_Flat) {
*/
		// A1T3: flat triangles
		// TODO: rasterize triangle (see block comment above this function).
		// calculate bbox of va,vb and vc
		auto min_vec2 = [](Vec2 a, Vec2 b) -> Vec2 {
			return Vec2{ std::min(a.x,b.x), std::min(a.y,b.y) };
		};
		auto max_vec2 = [](Vec2 a, Vec2 b) -> Vec2 {
			return Vec2{ std::max(a.x,b.x), std::max(a.y,b.y) }; 
		};

		Vec2 p0 = va.fb_position.xy();
		Vec2 p1 = vb.fb_position.xy();
		Vec2 p2 = vc.fb_position.xy();

		Vec2 p_min = min_vec2(p0,min_vec2(p1,p2));
		Vec2 p_max = max_vec2(p0,max_vec2(p1,p2));

		int x_start = static_cast<int>(std::floor(p_min.x));
		int x_end = static_cast<int>(std::ceil(p_max.x));
		int y_start = static_cast<int>(std::floor(p_min.y));
		int y_end = static_cast<int>(std::ceil(p_max.y));

		auto inside_triangle = [](Vec2 p0, Vec2 p1, Vec2 p2, Vec2 pp) -> bool {
			Vec2 a = p1 - p0;
			Vec2 a1 = pp - p0;
			Vec2 b = p2 - p1;
			Vec2 b1 = pp - p1;
			Vec2 c = p0 - p2;
			Vec2 c1 = pp - p2;

			auto cross = [](Vec2 a, Vec2 b) -> float {
				return (a.x * b.y - a.y * b.x);
			};

			float e0 = cross(a,a1);
			float e1 = cross(b,b1);
			float e2 = cross(c,c1);
			// for both CCW and CW
			return (e0 >= 0.0f && e1 >= 0.0f && e2 >= 0.0f) || (e0 <= 0.0f && e1 <= 0.0f && e2 <= 0.0f);
		};

		auto gen_fragment = [&va,&vb,&vc,p0,p1,p2,&inside_triangle](Vec2 pp, Fragment& f) -> bool {
			// calculate barycentric coordinate of pp inside triangle p0,p1,p2 (as 2D triangle)
			float gamma = ((p0.y - p1.y) * pp.x + (p1.x - p0.x) * pp.y + p0.x * p1.y - p1.x * p0.y) / ((p0.y - p1.y) * p2.x + (p1.x - p0.x) * p2.y + p0.x * p1.y - p1.x * p0.y);
			float beta = ((p0.y - p2.y) * pp.x + (p2.x - p0.x) * pp.y + p0.x * p2.y - p2.x * p0.y) / ((p0.y - p2.y) * p1.x + (p2.x - p0.x) * p1.y + p0.x * p2.y - p2.x * p0.y);
			float alpha = 1.0f - beta - gamma;
			float z_w_div = alpha * va.fb_position.z * va.inv_w + beta * vb.fb_position.z * vb.inv_w + gamma * vc.fb_position.z * vc.inv_w;
			float inv_w = alpha * va.inv_w + beta * vb.inv_w + gamma * vc.inv_w;
			float z = z_w_div / inv_w;
			f.fb_position = Vec3{ pp.x,pp.y,z };
			f.derivatives.fill(Vec2{0.0f,0.0f});
			if constexpr ((flags & PipelineMask_Interp) == Pipeline_Interp_Flat) {
				f.attributes = va.attributes;
			} else if constexpr ((flags & PipelineMask_Interp) == Pipeline_Interp_Smooth) {					
				for(int i=0;i<FA;i++) {
					float a = alpha * va.attributes[i] + beta * vb.attributes[i] + gamma * vc.attributes[i];
					f.attributes[i] = a;
				}
			} else if constexpr ((flags & PipelineMask_Interp) == Pipeline_Interp_Correct) {
				for(int i=0;i<FA;i++) {
					float a = alpha * va.attributes[i] * va.inv_w + beta * vb.attributes[i] * vb.inv_w + gamma * vc.attributes[i] * vc.inv_w;
					f.attributes[i] = a / inv_w;
				}
			}
			if(inside_triangle(p0,p1,p2,pp)) {
				return true;
			}
			return false;
		};
		
		// here we should rasterize this triangle with 2x2 tile once
		for(int i=x_start;i<=x_end;i+=2) {
			for(int j=y_start;j<=y_end;j+=2) {
				std::array<Fragment, 4> frags{};
				std::array<bool,4> emit_flags{};
				std::array<Vec2, 4> points {
					// 0b00
					Vec2{static_cast<float>(i) + 0.5f,static_cast<float>(j) + 0.5f  },
					// 0b01
					Vec2{static_cast<float>(i) + 0.5f,static_cast<float>(j + 1) + 0.5f  },
					// 0b10
					Vec2{static_cast<float>(i + 1) + 0.5f,static_cast<float>(j) + 0.5f  },
					// 0b11
					Vec2{static_cast<float>(i + 1) + 0.5f,static_cast<float>(j + 1) + 0.5f  }
				};
				emit_flags[0] = gen_fragment(points[0],frags[0]);
				emit_flags[1] = gen_fragment(points[1],frags[1]);
				emit_flags[2] = gen_fragment(points[2],frags[2]);
				emit_flags[3] = gen_fragment(points[3],frags[3]);
				std::array<Vec2, FD> derivatives{};
				for(int d_i=0;d_i<FD;d_i++) {
					derivatives[d_i].x = frags[0b10].attributes[d_i] - frags[0b00].attributes[d_i];
					derivatives[d_i].y = frags[0b01].attributes[d_i] - frags[0b00].attributes[d_i];
				}
				if (emit_flags[0b00]) {
					frags[0b00].derivatives = derivatives;
					emit_fragment(frags[0b00]);	
				}
				if (emit_flags[0b01]) {
					frags[0b01].derivatives = derivatives;
					emit_fragment(frags[0b01]);	
				}
				if (emit_flags[0b10]) {
					frags[0b10].derivatives = derivatives;
					emit_fragment(frags[0b10]);	
				}
				if (emit_flags[0b11]) {
					frags[0b11].derivatives = derivatives;
					emit_fragment(frags[0b11]);	
				}
			}
		}

/*
		// As a placeholder, here's code that draws some lines:
		//(remove this and replace it with a real solution)
		// Pipeline<PrimitiveType::Lines, P, flags>::rasterize_line(va, vb, emit_fragment);
		// Pipeline<PrimitiveType::Lines, P, flags>::rasterize_line(vb, vc, emit_fragment);
		// Pipeline<PrimitiveType::Lines, P, flags>::rasterize_line(vc, va, emit_fragment);
	} else if constexpr ((flags & PipelineMask_Interp) == Pipeline_Interp_Smooth) {
		// A1T5: screen-space smooth triangles
		// TODO: rasterize triangle (see block comment above this function).

		// As a placeholder, here's code that calls the Flat interpolation version of the function:
		//(remove this and replace it with a real solution)
		Pipeline<PrimitiveType::Lines, P, (flags & ~PipelineMask_Interp) | Pipeline_Interp_Flat>::rasterize_triangle(va, vb, vc, emit_fragment);
	} else if constexpr ((flags & PipelineMask_Interp) == Pipeline_Interp_Correct) {
		// A1T5: perspective correct triangles
		// TODO: rasterize triangle (block comment above this function).

		// As a placeholder, here's code that calls the Screen-space interpolation function:
		//(remove this and replace it with a real solution)
		Pipeline<PrimitiveType::Lines, P, (flags & ~PipelineMask_Interp) | Pipeline_Interp_Smooth>::rasterize_triangle(va, vb, vc, emit_fragment);
	}
*/
}

//-------------------------------------------------------------------------
// compile instantiations for all programs and blending and testing types:

#include "programs.h"

template struct Pipeline<PrimitiveType::Triangles, Programs::Lambertian,
                         Pipeline_Blend_Replace | Pipeline_Depth_Always | Pipeline_Interp_Flat>;
template struct Pipeline<PrimitiveType::Triangles, Programs::Lambertian,
                         Pipeline_Blend_Replace | Pipeline_Depth_Always | Pipeline_Interp_Smooth>;
template struct Pipeline<PrimitiveType::Triangles, Programs::Lambertian,
                         Pipeline_Blend_Replace | Pipeline_Depth_Always | Pipeline_Interp_Correct>;
template struct Pipeline<PrimitiveType::Triangles, Programs::Lambertian,
                         Pipeline_Blend_Replace | Pipeline_Depth_Never | Pipeline_Interp_Flat>;
template struct Pipeline<PrimitiveType::Triangles, Programs::Lambertian,
                         Pipeline_Blend_Replace | Pipeline_Depth_Never | Pipeline_Interp_Smooth>;
template struct Pipeline<PrimitiveType::Triangles, Programs::Lambertian,
                         Pipeline_Blend_Replace | Pipeline_Depth_Never | Pipeline_Interp_Correct>;
template struct Pipeline<PrimitiveType::Triangles, Programs::Lambertian,
                         Pipeline_Blend_Replace | Pipeline_Depth_Less | Pipeline_Interp_Flat>;
template struct Pipeline<PrimitiveType::Triangles, Programs::Lambertian,
                         Pipeline_Blend_Replace | Pipeline_Depth_Less | Pipeline_Interp_Smooth>;
template struct Pipeline<PrimitiveType::Triangles, Programs::Lambertian,
                         Pipeline_Blend_Replace | Pipeline_Depth_Less | Pipeline_Interp_Correct>;
template struct Pipeline<PrimitiveType::Triangles, Programs::Lambertian,
                         Pipeline_Blend_Add | Pipeline_Depth_Always | Pipeline_Interp_Flat>;
template struct Pipeline<PrimitiveType::Triangles, Programs::Lambertian,
                         Pipeline_Blend_Add | Pipeline_Depth_Always | Pipeline_Interp_Smooth>;
template struct Pipeline<PrimitiveType::Triangles, Programs::Lambertian,
                         Pipeline_Blend_Add | Pipeline_Depth_Always | Pipeline_Interp_Correct>;
template struct Pipeline<PrimitiveType::Triangles, Programs::Lambertian,
                         Pipeline_Blend_Add | Pipeline_Depth_Never | Pipeline_Interp_Flat>;
template struct Pipeline<PrimitiveType::Triangles, Programs::Lambertian,
                         Pipeline_Blend_Add | Pipeline_Depth_Never | Pipeline_Interp_Smooth>;
template struct Pipeline<PrimitiveType::Triangles, Programs::Lambertian,
                         Pipeline_Blend_Add | Pipeline_Depth_Never | Pipeline_Interp_Correct>;
template struct Pipeline<PrimitiveType::Triangles, Programs::Lambertian,
                         Pipeline_Blend_Add | Pipeline_Depth_Less | Pipeline_Interp_Flat>;
template struct Pipeline<PrimitiveType::Triangles, Programs::Lambertian,
                         Pipeline_Blend_Add | Pipeline_Depth_Less | Pipeline_Interp_Smooth>;
template struct Pipeline<PrimitiveType::Triangles, Programs::Lambertian,
                         Pipeline_Blend_Add | Pipeline_Depth_Less | Pipeline_Interp_Correct>;
template struct Pipeline<PrimitiveType::Triangles, Programs::Lambertian,
                         Pipeline_Blend_Over | Pipeline_Depth_Always | Pipeline_Interp_Flat>;
template struct Pipeline<PrimitiveType::Triangles, Programs::Lambertian,
                         Pipeline_Blend_Over | Pipeline_Depth_Always | Pipeline_Interp_Smooth>;
template struct Pipeline<PrimitiveType::Triangles, Programs::Lambertian,
                         Pipeline_Blend_Over | Pipeline_Depth_Always | Pipeline_Interp_Correct>;
template struct Pipeline<PrimitiveType::Triangles, Programs::Lambertian,
                         Pipeline_Blend_Over | Pipeline_Depth_Never | Pipeline_Interp_Flat>;
template struct Pipeline<PrimitiveType::Triangles, Programs::Lambertian,
                         Pipeline_Blend_Over | Pipeline_Depth_Never | Pipeline_Interp_Smooth>;
template struct Pipeline<PrimitiveType::Triangles, Programs::Lambertian,
                         Pipeline_Blend_Over | Pipeline_Depth_Never | Pipeline_Interp_Correct>;
template struct Pipeline<PrimitiveType::Triangles, Programs::Lambertian,
                         Pipeline_Blend_Over | Pipeline_Depth_Less | Pipeline_Interp_Flat>;
template struct Pipeline<PrimitiveType::Triangles, Programs::Lambertian,
                         Pipeline_Blend_Over | Pipeline_Depth_Less | Pipeline_Interp_Smooth>;
template struct Pipeline<PrimitiveType::Triangles, Programs::Lambertian,
                         Pipeline_Blend_Over | Pipeline_Depth_Less | Pipeline_Interp_Correct>;
template struct Pipeline<PrimitiveType::Lines, Programs::Lambertian,
                         Pipeline_Blend_Replace | Pipeline_Depth_Always | Pipeline_Interp_Flat>;
template struct Pipeline<PrimitiveType::Lines, Programs::Lambertian,
                         Pipeline_Blend_Replace | Pipeline_Depth_Never | Pipeline_Interp_Flat>;
template struct Pipeline<PrimitiveType::Lines, Programs::Lambertian,
                         Pipeline_Blend_Replace | Pipeline_Depth_Less | Pipeline_Interp_Flat>;
template struct Pipeline<PrimitiveType::Lines, Programs::Lambertian,
                         Pipeline_Blend_Add | Pipeline_Depth_Always | Pipeline_Interp_Flat>;
template struct Pipeline<PrimitiveType::Lines, Programs::Lambertian,
                         Pipeline_Blend_Add | Pipeline_Depth_Never | Pipeline_Interp_Flat>;
template struct Pipeline<PrimitiveType::Lines, Programs::Lambertian,
                         Pipeline_Blend_Add | Pipeline_Depth_Less | Pipeline_Interp_Flat>;
template struct Pipeline<PrimitiveType::Lines, Programs::Lambertian,
                         Pipeline_Blend_Over | Pipeline_Depth_Always | Pipeline_Interp_Flat>;
template struct Pipeline<PrimitiveType::Lines, Programs::Lambertian,
                         Pipeline_Blend_Over | Pipeline_Depth_Never | Pipeline_Interp_Flat>;
template struct Pipeline<PrimitiveType::Lines, Programs::Lambertian,
                         Pipeline_Blend_Over | Pipeline_Depth_Less | Pipeline_Interp_Flat>;