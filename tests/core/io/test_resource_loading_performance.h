// Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md).
// Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.

#include "tests/test_macros.h"

#include "core/io/resource_loader.h"
#include "core/os/os.h"
#include "scene/main/node.h"
#include "scene/resources/packed_scene.h"

namespace TestResourceLoading {

// Benchmark simple resource loading performance
TEST_CASE("[Performance] Resource loading - small textures") {
	// Note: This benchmark requires actual project resources
	// In real usage, replace with valid resource paths from test project
	
	const int iterations = 100;
	uint64_t start_time = OS::get_singleton()->get_ticks_usec();
	
	for (int i = 0; i < iterations; i++) {
		// Simulate loading small resources repeatedly
		// In production, use actual test resources
		String test_path = "res://test_texture_small.png";
		Ref<Resource> res = ResourceLoader::load(test_path, "", ResourceFormatLoader::CACHE_MODE_IGNORE);
	}
	
	uint64_t end_time = OS::get_singleton()->get_ticks_usec();
	uint64_t elapsed_ms = (end_time - start_time) / 1000;
	
	print_line(vformat("Small texture loading: %d iterations in %d ms (avg: %.2f ms)",
			iterations, elapsed_ms, (float)elapsed_ms / iterations));
			
	// Performance threshold: Should complete in reasonable time
	// Adjust threshold based on target hardware
	CHECK_MESSAGE(elapsed_ms < 5000, "Resource loading took too long");
}

TEST_CASE("[Performance] Resource loading - threaded") {
	const int concurrent_loads = 10;
	uint64_t start_time = OS::get_singleton()->get_ticks_usec();
	
	Vector<String> paths;
	for (int i = 0; i < concurrent_loads; i++) {
		String path = vformat("res://test_resource_%d.tres", i);
		paths.push_back(path);
		ResourceLoader::load_threaded_request(path, "", false, ResourceFormatLoader::CACHE_MODE_IGNORE);
	}
	
	// Wait for all loads to complete
	bool all_loaded = false;
	int timeout_iterations = 100;
	while (!all_loaded && timeout_iterations > 0) {
		all_loaded = true;
		for (const String &path : paths) {
			ResourceLoader::ThreadLoadStatus status = ResourceLoader::load_threaded_get_status(path);
			if (status == ResourceLoader::THREAD_LOAD_IN_PROGRESS) {
				all_loaded = false;
				break;
			}
		}
		if (!all_loaded) {
			OS::get_singleton()->delay_usec(10000); // 10ms
			timeout_iterations--;
		}
	}
	
	uint64_t end_time = OS::get_singleton()->get_ticks_usec();
	uint64_t elapsed_ms = (end_time - start_time) / 1000;
	
	print_line(vformat("Threaded loading: %d resources in %d ms",
			concurrent_loads, elapsed_ms));
	
	CHECK_MESSAGE(timeout_iterations > 0, "Threaded loading timed out");
}

TEST_CASE("[Performance] Resource loading - cache hit vs miss") {
	const String test_path = "res://test_cached_resource.tres";
	
	// First load (cache miss)
	uint64_t miss_start = OS::get_singleton()->get_ticks_usec();
	Ref<Resource> res1 = ResourceLoader::load(test_path, "", ResourceFormatLoader::CACHE_MODE_REUSE);
	uint64_t miss_time = OS::get_singleton()->get_ticks_usec() - miss_start;
	
	// Second load (cache hit)
	uint64_t hit_start = OS::get_singleton()->get_ticks_usec();
	Ref<Resource> res2 = ResourceLoader::load(test_path, "", ResourceFormatLoader::CACHE_MODE_REUSE);
	uint64_t hit_time = OS::get_singleton()->get_ticks_usec() - hit_start;
	
	print_line(vformat("Cache miss: %d μs, Cache hit: %d μs (%.1fx faster)",
			miss_time, hit_time, (float)miss_time / MAX(hit_time, 1)));
	
	// Cache hit should be significantly faster
	CHECK_MESSAGE(hit_time < miss_time / 10, "Cache hit not significantly faster than miss");
}

} // namespace TestResourceLoading
