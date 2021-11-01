#include <gtest/gtest.h>
#include "../src/sqlite/sqlite3.cpp"

// Demonstrate some basic assertions.
TEST(HelloTest, BasicAssertions) {

  sqlite_client client;
  client.exec("hello\n");
  // Expect two strings not to be equal.
  EXPECT_STRNE("hello", "world");
  // Expect equality.
  EXPECT_EQ(7 * 6, 42);
}