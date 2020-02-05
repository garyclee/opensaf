/*      -*- OpenSAF  -*-
 *
 * Copyright Ericsson AB 2020 - All Rights Reserved.
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
 * Reference: Serial Number Arithmetic from RFC1982
 *
 */

#include "base/sna.h"
#include "gtest/gtest.h"

#ifndef DEBUG_SNA
#define printf(x, args...)  // No printf
#endif

template <class T>
int test_sna(T x) {
  int rc = 1;
  printf("\n============= START with x=%lu =============\n", (uint64_t)x.v());
  T y = x;
  printf("x=%lu, y=%lu: check x == y++ is TRUE\n",
    (uint64_t)x.v(), (uint64_t)y.v());
  if (x == y++) {
    printf("now y=%lu, reset y = x\n", (uint64_t)y.v());
    y = x;
    printf("x=%lu, y=%lu: check x != ++y is TRUE\n",
      (uint64_t)x.v(), (uint64_t)y.v());
    if (x != ++y) {
      printf("now y=%lu, reset y = x\n", (uint64_t)y.v());
      y = x;
      printf("x=%lu, y=%lu: check x < ++y is TRUE\n",
        (uint64_t)x.v(), (uint64_t)y.v());
      if (x < ++y) {
        printf("x=%lu: check x + 1 > x and x + 1 >= x is TRUE\n",
          (uint64_t)x.v());
        if ((x + 1 > x) && (x + 1 >= x)) {
          printf("x=%lu: check x < x + 1 and x <= x + 1 is TRUE\n",
            (uint64_t)x.v());
          y = x + 1;
          printf("y = x+1 => y=%lu\n", (uint64_t)y.v());
          y = y + 1;
          printf("y = y+1 => y=%lu\n", (uint64_t)y.v());
          if ((x < x + 1) && (x <= x + 1)) {
            try {
              printf("x=%lu: add invalid (-1)\n", (uint64_t)x.v());
              x = x + (-1);
            } catch (const std::out_of_range& oor) {
              printf("Expected error: %s\n", oor.what());
              try {
                uint64_t max_value = 0;
                if (typeid(T) == typeid(Seq16))
                  max_value = SNA16_MAX;
                else if (typeid(T) == typeid(Seq32))
                  max_value = SNA32_MAX;
                printf("x=%lu: add invalid (%lu)\n",
                  (uint64_t)x.v(), max_value);
                x = x + max_value;
              } catch (const std::out_of_range& oor) {
                printf("Expected error: %s\n", oor.what());
                rc = 0;
              }
            }
          }
        }
      }
    }
  }
  printf("================ END with x=%lu ==============\n", (uint64_t)x.v());
  return rc;
}


class SnaTest : public ::testing::Test {
 protected:
  SnaTest() {}
  virtual ~SnaTest() {
    // Cleanup work that doesn't throw exceptions here.
  }
  virtual void SetUp() {
    // Code here will be called immediately after the constructor (right
    // before each test)
  }
  virtual void TearDown() {}
};

TEST_F(SnaTest, unit16_sna) {
  Seq16 x;
  EXPECT_EQ(0, test_sna(x));
  Seq16 x1 = Seq16(1);
  Seq16 x2 = Seq16(SNA16_MAX - 1);
  EXPECT_EQ(2, x1 - x2);
  EXPECT_EQ(-2, x2 - x1);
  EXPECT_EQ(0, test_sna(x1));
  EXPECT_EQ(0, test_sna(x2));
}

TEST_F(SnaTest, unit32_sna) {
  Seq32 x;
  EXPECT_EQ(0, test_sna(x));
  Seq32 x1 = Seq32(1);
  Seq32 x2 = Seq32(SNA32_MAX - 1);
  EXPECT_EQ(2, x1 - x2);
  EXPECT_EQ(-2, x2 - x1);
  EXPECT_EQ(0, test_sna(x1));
  EXPECT_EQ(0, test_sna(x2));
}


int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
