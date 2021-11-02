#include <gtest/gtest.h>
#include "../src/sqlite/sqlite_client.cpp"

// Demonstrate some basic assertions.
TEST(Sqlclient_Test, group_table_creation_test) {

  sqlite::group_client client;

  ASSERT_NO_THROW (
    client.create_group_table();
    client.delete_group_table();
  );
}

TEST(Sqlclient_Test, group_insert_test) {
  sqlite::group_client client;
  ASSERT_NO_THROW (
    client.create_group_table();
    client.insert_group("group1", "owner1");
  );

  EXPECT_THROW (
    client.insert_group("group1", "owner1"); ,
    sqlite::sqlite_exception
  );

  EXPECT_NO_THROW (
    client.remove_group("group1");
  );

  ASSERT_NO_THROW (
    client.delete_group_table();
  );
}

TEST(Sqlclient_Test, group_select_test) {
  sqlite::group_client client;
  ASSERT_NO_THROW (
    client.create_group_table();
    client.insert_group("group1", "owner1");
    client.insert_group("group2", "owner1");
    client.insert_group("group3", "owner2");
    client.insert_group("group4", "owner3");
  );

  EXPECT_NO_THROW(
    auto res = client.get_group_list();
    for(auto &row : res) {
      for(auto val : row) {
        std::cout<<val<<" ";
      }
      std::cout<<"\n";
    }
  );

  ASSERT_NO_THROW (
    client.delete_group_table();
  );
}