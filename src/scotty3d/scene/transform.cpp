
#include "transform.h"
#include <memory>

Mat4 Transform::local_to_parent() const {
	return Mat4::translate(translation) * rotation.to_mat() * Mat4::scale(scale);
}

Mat4 Transform::parent_to_local() const {
	return Mat4::scale(1.0f / scale) * rotation.inverse().to_mat() * Mat4::translate(-translation);
}

Mat4 Transform::local_to_world() const {
	// A1T1: local_to_world
	//don't use Mat4::inverse() in your code.
	Mat4 res = local_to_parent();
	std::shared_ptr<Transform> p = parent.lock();
	while(p) {
		res = p->local_to_parent() * res;
		p = p->parent.lock();
	}
	return res; //<-- wrong, but here so code will compile
}

Mat4 Transform::world_to_local() const {
	// A1T1: world_to_local
	//don't use Mat4::inverse() in your code.
	Mat4 res = parent_to_local();
	std::shared_ptr<Transform> p = parent.lock();
	while(p) {
		res = res * p->parent_to_local();
		p = p->parent.lock();
	}
	return res; //<-- wrong, but here so code will compile
}

bool operator!=(const Transform& a, const Transform& b) {
	return a.parent.lock() != b.parent.lock() || a.translation != b.translation ||
	       a.rotation != b.rotation || a.scale != b.scale;
}
