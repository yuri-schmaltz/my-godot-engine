/**************************************************************************/
/*  object_pool.h                                                         */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#pragma once

#include "core/os/memory.h"
#include "core/os/spin_lock.h"
#include "core/templates/local_vector.h"
#include "core/typedefs.h"

// ObjectPool provides efficient reuse of frequently allocated objects.
// Unlike PagedAllocator which manages raw memory, ObjectPool:
// - Always calls constructors on acquire() and destructors on release()
// - Maintains a free list of constructed-but-unused objects
// - Thread-safe when thread_safe=true
// - Ideal for types with expensive construction (Vector3, Transform3D, etc.)
//
// Usage:
//   ObjectPool<Transform3D> pool(128); // Initial capacity
//   Transform3D* t = pool.acquire();    // Get object (may reuse or construct new)
//   // ... use transform ...
//   pool.release(t);                    // Return to pool (destructor NOT called yet)
//
// Performance characteristics:
// - acquire(): O(1) - pops from free list or allocates new
// - release(): O(1) - pushes to free list
// - Memory overhead: sizeof(T*) per free object + vector capacity overhead

template <typename T, bool thread_safe = false>
class ObjectPool {
	// All allocated objects (both in-use and free)
	LocalVector<T *> allocated_objects;
	// Free list - pointers to objects ready for reuse
	LocalVector<T *> free_list;
	// Synchronization for thread-safe mode
	mutable SpinLock spin_lock;
	// Initial capacity hint
	uint32_t initial_capacity = 0;

	// Statistics for monitoring pool efficiency
	uint64_t total_acquires = 0;
	uint64_t total_releases = 0;
	uint64_t reuse_count = 0; // Number of times we reused an object

public:
	// Construct pool with optional initial capacity hint
	explicit ObjectPool(uint32_t p_initial_capacity = 32) :
			initial_capacity(p_initial_capacity) {
		if (initial_capacity > 0) {
			allocated_objects.reserve(initial_capacity);
			free_list.reserve(initial_capacity);
		}
	}

	~ObjectPool() {
		// Clean up all allocated objects
		if constexpr (thread_safe) {
			spin_lock.lock();
		}

		for (uint32_t i = 0; i < allocated_objects.size(); i++) {
			T *obj = allocated_objects[i];
			if (obj) {
				obj->~T();
				memfree(obj);
			}
		}

		if constexpr (thread_safe) {
			spin_lock.unlock();
		}
	}

	// Acquire an object from the pool
	// Returns: Pointer to constructed object (may be reused or newly allocated)
	template <typename... Args>
	T *acquire(Args &&...p_args) {
		if constexpr (thread_safe) {
			spin_lock.lock();
		}

		total_acquires++;
		T *obj = nullptr;

		// Try to reuse from free list first
		if (free_list.size() > 0) {
			obj = free_list[free_list.size() - 1];
			free_list.resize(free_list.size() - 1);
			reuse_count++;

			if constexpr (thread_safe) {
				spin_lock.unlock();
			}

			// Reconstruct object in-place with new parameters
			// This properly initializes the object for reuse
			obj->~T();
			memnew_placement(obj, T(p_args...));
			return obj;
		}

		// No free objects available - allocate new one
		obj = (T *)memalloc(sizeof(T));
		memnew_placement(obj, T(p_args...));
		allocated_objects.push_back(obj);

		if constexpr (thread_safe) {
			spin_lock.unlock();
		}

		return obj;
	}

	// Release object back to the pool for reuse
	// Object is NOT destroyed - it remains constructed for fast reuse
	// Do NOT access the object after calling release()
	void release(T *p_obj) {
		ERR_FAIL_NULL(p_obj);

		if constexpr (thread_safe) {
			spin_lock.lock();
		}

		total_releases++;

#ifdef DEBUG_ENABLED
		// Verify object belongs to this pool
		bool found = false;
		for (uint32_t i = 0; i < allocated_objects.size(); i++) {
			if (allocated_objects[i] == p_obj) {
				found = true;
				break;
			}
		}
		ERR_FAIL_COND_MSG(!found, "Object does not belong to this pool");

		// Verify object not already in free list (double-free detection)
		for (uint32_t i = 0; i < free_list.size(); i++) {
			ERR_FAIL_COND_MSG(free_list[i] == p_obj, "Object already released (double-free)");
		}
#endif

		free_list.push_back(p_obj);

		if constexpr (thread_safe) {
			spin_lock.unlock();
		}
	}

	// Get pool statistics
	uint32_t get_allocated_count() const {
		if constexpr (thread_safe) {
			spin_lock.lock();
		}
		uint32_t result = allocated_objects.size();
		if constexpr (thread_safe) {
			spin_lock.unlock();
		}
		return result;
	}

	uint32_t get_free_count() const {
		if constexpr (thread_safe) {
			spin_lock.lock();
		}
		uint32_t result = free_list.size();
		if constexpr (thread_safe) {
			spin_lock.unlock();
		}
		return result;
	}

	uint32_t get_in_use_count() const {
		return get_allocated_count() - get_free_count();
	}

	// Get reuse efficiency (0.0 = no reuse, 1.0 = perfect reuse)
	float get_reuse_rate() const {
		if constexpr (thread_safe) {
			spin_lock.lock();
		}
		float result = 0.0f;
		if (total_acquires > 0) {
			result = (float)reuse_count / (float)total_acquires;
		}
		if constexpr (thread_safe) {
			spin_lock.unlock();
		}
		return result;
	}

	// Reset statistics (useful for profiling specific sections)
	void reset_stats() {
		if constexpr (thread_safe) {
			spin_lock.lock();
		}
		total_acquires = 0;
		total_releases = 0;
		reuse_count = 0;
		if constexpr (thread_safe) {
			spin_lock.unlock();
		}
	}

	// Clear all objects and reset pool
	// WARNING: Only call when you're sure no objects are in use!
	void clear() {
		if constexpr (thread_safe) {
			spin_lock.lock();
		}

		// Destroy all allocated objects
		for (uint32_t i = 0; i < allocated_objects.size(); i++) {
			T *obj = allocated_objects[i];
			if (obj) {
				obj->~T();
				memfree(obj);
			}
		}

		allocated_objects.clear();
		free_list.clear();
		total_acquires = 0;
		total_releases = 0;
		reuse_count = 0;

		if constexpr (thread_safe) {
			spin_lock.unlock();
		}
	}

	// Estimate memory usage (bytes)
	uint64_t estimate_memory_use() const {
		if constexpr (thread_safe) {
			spin_lock.lock();
		}
		uint64_t result = allocated_objects.size() * sizeof(T) +
				allocated_objects.size() * sizeof(T *) + // allocated_objects vector
				free_list.size() * sizeof(T *); // free_list vector
		if constexpr (thread_safe) {
			spin_lock.unlock();
		}
		return result;
	}

	// Pre-allocate objects to avoid allocation spikes during runtime
	// Useful during initialization or between frames
	void prewarm(uint32_t p_count) {
		LocalVector<T *> temp_objects;
		temp_objects.reserve(p_count);

		// Acquire objects
		for (uint32_t i = 0; i < p_count; i++) {
			temp_objects.push_back(acquire());
		}

		// Release them all back to pool
		for (uint32_t i = 0; i < temp_objects.size(); i++) {
			release(temp_objects[i]);
		}
	}
};

// Convenience typedef for common thread-safe usage
template <typename T>
using ThreadSafeObjectPool = ObjectPool<T, true>;

#endif // OBJECT_POOL_H
