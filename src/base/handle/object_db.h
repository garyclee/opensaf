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

#ifndef BASE_HANDLE_OBJECT_DB_H_
#define BASE_HANDLE_OBJECT_DB_H_

#include <pthread.h>
#include <cstdint>
#include <functional>
#include <random>
#include <set>

#include "base/mutex.h"
#include "base/handle/object.h"
#include "base/time.h"

namespace base {

namespace handle {

// A database where Objects can be looked up using their object ids. Each
// instance of the ObjectDb class has its own sequence of object ids, and
// therefore Objects stored in separate instances of ObjectDb may share the same
// object id.
class ObjectDb {
 public:
  // Initialize static members of this class. Must be called before any object
  // of this class is created. It is harmless to call this method multiple
  // times.
  static bool InitStatic();
  // Construct a new, empty ObjectDb.
  ObjectDb() : next_free_id_{GenerateRandomId()}, objects_{} {}
  // Destroy the ObjectDb instance. The database must be empty before calling
  // this method.
  ~ObjectDb();
  // Delete all Objects using the @a delete_function, and then remove them from
  // the database.
  void Clear(std::function<void(Object*)> delete_function);
  // Assigns a new unique object id to the specified @a object, and then adds
  // the Object the ObjectDb database. Returns true if successful, and false if
  // the Object could not be added to the database due to lack of memory.
  //
  // The Object must not already have an object id (other than kNullId) assigned
  // to it when calling this method.
  bool Add(Object* object);
  // Returns a pointer to the Object in the database with the specified @a id,
  // or nullptr if the database doesn't contain any such Object.
  Object* Lookup(Object::Id id) {
    Object tmp{id};
    auto result = objects_.find(&tmp);
    return result != objects_.end() ? *result : nullptr;
  }
  // Removes the Object with the specified @a id, and returns a pointer to the
  // removed Object. Returns a nullptr if the database doesn't contain any such
  // Object. This method does not clear the object id of the removed Object, and
  // therefore the Object must not be added to any ObjectDb again.
  Object* Remove(Object::Id id);

 private:
  class RandomGenerator {
   public:
    RandomGenerator()
        : mutex_{},
          generator_{base::TimespecToNanos(base::ReadRealtimeClock())} {}
    base::Mutex& Mutex() { return mutex_; }
    Object::Id GenerateId() { return static_cast<Object::Id>(generator_()); }

   private:
    base::Mutex mutex_;
    std::mt19937_64 generator_;
  };

  struct Compare {
    bool operator()(const Object* const& a, const Object* const& b) const {
      return *a < *b;
    }
  };

  static Object::Id NonNullId(Object::Id id) {
    return id != Object::kNullId ? id : (Object::kNullId + 1);
  }

  Object::Id GenerateRandomId();
  static void PthreadOnceInitRoutine();
  void AddObjectSlowPath(Object* object);

  Object::Id next_free_id_;
  std::set<Object*, Compare> objects_;
  static pthread_once_t once_control_;
  static RandomGenerator* random_generator_;
};

}  // namespace handle

}  // namespace base

#endif  // BASE_HANDLE_OBJECT_DB_H_
