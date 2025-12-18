/**************************************************************************/
/*  nav_map_base.h                                                       */
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

#include "core/os/rw_lock.h"
#include "core/templates/self_list.h"

// Generic dirty request structures for NavMap 2D/3D
// This consolidates duplicated code between nav_map_2d.h and nav_map_3d.h.

template <class T>
struct DirtyList {
	RWLock rwlock;
	SelfList<T>::List list;
};

// Synchronous dirty requests (regions, links, agents, obstacles)
template <class RegionT, class LinkT, class AgentT, class ObstacleT>
struct NavMapDirtyRequests {
	DirtyList<RegionT> regions;
	DirtyList<LinkT> links;
	DirtyList<AgentT> agents;
	DirtyList<ObstacleT> obstacles;
};

// Asynchronous dirty requests (currently only regions)
template <class RegionT>
struct NavAsyncDirtyRequests {
	DirtyList<RegionT> regions;
};
