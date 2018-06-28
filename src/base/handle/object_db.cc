/*      -*- OpenSAF  -*-
 *
 * Copyright Ericsson AB 2018 - All Rights Reserved.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. This file and program are licensed
 * under the GNU Lesser General Public License Version 2.1, February 1999.
 * The complete license can be accessed from the following location:
 * http://opensource.org/licenses/lgpl-license.php
 * See the Copying file included with the OpenSAF distribution for full
 * licensing terms.
 *
 */

#include <cassert>
#include <new>
#include <random>

#include "base/mutex.h"
#include "base/ncsgl_defs.h"
#include "base/osaf_utility.h"
#include "base/handle/object_db.h"

namespace base {

namespace handle {

pthread_once_t ObjectDb::once_control_ = PTHREAD_ONCE_INIT;
ObjectDb::RandomGenerator* ObjectDb::random_generator_ = nullptr;

ObjectDb::~ObjectDb() { osafassert(objects_.empty()); }

void ObjectDb::Clear(std::function<void(Object*)> delete_function) {
  for (const auto& object : objects_) delete_function(object);
  objects_.clear();
}

bool ObjectDb::InitStatic() {
  int result = pthread_once(&once_control_, PthreadOnceInitRoutine);
  if (result != 0) osaf_abort(result);
  return random_generator_ != nullptr;
}

Object::Id ObjectDb::GenerateRandomId() {
  base::Lock lock(random_generator_->Mutex());
  return random_generator_->GenerateId();
}

void ObjectDb::PthreadOnceInitRoutine() {
  assert(random_generator_ == nullptr);
  random_generator_ = new (std::nothrow) RandomGenerator();
}

bool ObjectDb::Add(Object* object) {
  assert(object->id() == Object::kNullId);
  object->set_id(NonNullId(next_free_id_));
  bool success = true;
  try {
    if (!objects_.insert(object).second) AddObjectSlowPath(object);
    next_free_id_ = object->id() + 1;
  } catch (std::bad_alloc&) {
    object->set_id(Object::kNullId);
    success = false;
  }
  return success;
}

Object* ObjectDb::Remove(Object::Id id) {
  Object tmp{id};
  auto result = objects_.find(&tmp);
  if (result != objects_.end()) {
    Object* object = *result;
    objects_.erase(result);
    return object;
  } else {
    return nullptr;
  }
}

void ObjectDb::AddObjectSlowPath(Object* object) {
  base::Lock lock(random_generator_->Mutex());
  for (;;) {
    object->set_id(NonNullId(random_generator_->GenerateId()));
    if (objects_.insert(object).second) break;
  }
}

}  // namespace handle

}  // namespace base
