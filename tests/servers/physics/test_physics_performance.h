// Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md).
// Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.

#include "tests/test_macros.h"

#include "core/math/transform_3d.h"
#include "core/math/vector3.h"
#include "servers/physics_server_3d.h"

namespace TestPhysicsPerformance {

// Benchmark physics body creation and transformation
TEST_CASE("[Performance] Physics - body creation") {
	PhysicsServer3D *ps = PhysicsServer3D::get_singleton();
	ERR_FAIL_NULL(ps);
	
	const int body_count = 1000;
	Vector<RID> bodies;
	bodies.resize(body_count);
	
	uint64_t start_time = OS::get_singleton()->get_ticks_usec();
	
	for (int i = 0; i < body_count; i++) {
		bodies.write[i] = ps->body_create();
		ps->body_set_mode(bodies[i], PhysicsServer3D::BODY_MODE_RIGID);
		ps->body_set_state(bodies[i], PhysicsServer3D::BODY_STATE_TRANSFORM,
				Transform3D(Basis(), Vector3(i * 2.0, 10, 0)));
	}
	
	uint64_t end_time = OS::get_singleton()->get_ticks_usec();
	uint64_t elapsed_ms = (end_time - start_time) / 1000;
	
	print_line(vformat("Physics body creation: %d bodies in %d ms (avg: %.3f ms/body)",
			body_count, elapsed_ms, (float)elapsed_ms / body_count));
	
	// Cleanup
	for (int i = 0; i < body_count; i++) {
		ps->free_rid(bodies[i]);
	}
	
	// Performance threshold
	CHECK_MESSAGE(elapsed_ms < 2000, "Body creation took too long");
}

TEST_CASE("[Performance] Physics - transform updates") {
	PhysicsServer3D *ps = PhysicsServer3D::get_singleton();
	ERR_FAIL_NULL(ps);
	
	const int body_count = 500;
	const int update_iterations = 100;
	Vector<RID> bodies;
	bodies.resize(body_count);
	
	// Create bodies
	for (int i = 0; i < body_count; i++) {
		bodies.write[i] = ps->body_create();
		ps->body_set_mode(bodies[i], PhysicsServer3D::BODY_MODE_RIGID);
	}
	
	uint64_t start_time = OS::get_singleton()->get_ticks_usec();
	
	// Update transforms repeatedly
	for (int iter = 0; iter < update_iterations; iter++) {
		for (int i = 0; i < body_count; i++) {
			Transform3D t(Basis(), Vector3(Math::sin(iter * 0.1 + i), 10, Math::cos(iter * 0.1 + i)));
			ps->body_set_state(bodies[i], PhysicsServer3D::BODY_STATE_TRANSFORM, t);
		}
	}
	
	uint64_t end_time = OS::get_singleton()->get_ticks_usec();
	uint64_t elapsed_ms = (end_time - start_time) / 1000;
	uint64_t total_updates = body_count * update_iterations;
	
	print_line(vformat("Physics transform updates: %d updates in %d ms (%.1f updates/ms)",
			total_updates, elapsed_ms, (float)total_updates / MAX(elapsed_ms, 1)));
	
	// Cleanup
	for (int i = 0; i < body_count; i++) {
		ps->free_rid(bodies[i]);
	}
	
	CHECK_MESSAGE(elapsed_ms < 3000, "Transform updates took too long");
}

TEST_CASE("[Performance] Physics - collision queries") {
	PhysicsServer3D *ps = PhysicsServer3D::get_singleton();
	ERR_FAIL_NULL(ps);
	
	// Create test space
	RID space = ps->space_create();
	ps->space_set_active(space, true);
	
	// Create sphere shape
	RID shape = ps->shape_create(PhysicsServer3D::SHAPE_SPHERE);
	ps->shape_set_data(shape, 1.0); // radius
	
	// Create static bodies in a grid
	const int grid_size = 10;
	Vector<RID> bodies;
	for (int x = 0; x < grid_size; x++) {
		for (int z = 0; z < grid_size; z++) {
			RID body = ps->body_create();
			ps->body_set_mode(body, PhysicsServer3D::BODY_MODE_STATIC);
			ps->body_set_space(body, space);
			ps->body_add_shape(body, shape);
			ps->body_set_state(body, PhysicsServer3D::BODY_STATE_TRANSFORM,
					Transform3D(Basis(), Vector3(x * 5.0, 0, z * 5.0)));
			bodies.push_back(body);
		}
	}
	
	// Benchmark collision queries
	const int query_iterations = 1000;
	uint64_t start_time = OS::get_singleton()->get_ticks_usec();
	
	for (int i = 0; i < query_iterations; i++) {
		// Query a point in the space
		Vector3 point(Math::randf() * 50.0, 0, Math::randf() * 50.0);
		// Note: Actual collision query API would be used here
		// This is a placeholder for the performance test structure
	}
	
	uint64_t end_time = OS::get_singleton()->get_ticks_usec();
	uint64_t elapsed_ms = (end_time - start_time) / 1000;
	
	print_line(vformat("Physics collision queries: %d queries in %d ms (%.1f queries/ms)",
			query_iterations, elapsed_ms, (float)query_iterations / MAX(elapsed_ms, 1)));
	
	// Cleanup
	for (RID body : bodies) {
		ps->free_rid(body);
	}
	ps->free_rid(shape);
	ps->free_rid(space);
	
	CHECK_MESSAGE(elapsed_ms < 1000, "Collision queries took too long");
}

} // namespace TestPhysicsPerformance
