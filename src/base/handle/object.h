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

#ifndef BASE_HANDLE_OBJECT_H_
#define BASE_HANDLE_OBJECT_H_

#include <cstdint>

namespace base {

namespace handle {

// An object which has a 64-bit identifier.
class Object {
  friend class ObjectDb;

 public:
  // The integer type used for object ids.
  using Id = uint64_t;
  // A special Null object id, which is not a valid object id.
  static constexpr Id kNullId = 0;
  // Constructs a new Object with a Null object id.
  Object() : id_{kNullId} {}
  // Returns true if the object id of this Object is less than the object id of
  // the specified @a object.
  bool operator<(const Object& object) const { return id_ < object.id_; }
  // Returns the object id of this Object.
  Id id() const { return id_; }

 private:
  explicit Object(Id id) : id_{id} {}
  void set_id(Id new_id) { id_ = new_id; }
  Id id_;
};

}  // namespace handle

}  // namespace base

#endif  // BASE_HANDLE_OBJECT_H_
