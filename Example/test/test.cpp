#include "wolf.h"
#include "tiger.h"
#include "mouse.h"

#include <iostream>
#include <gtest/gtest.h>

using std::cout;
using std::endl;

int main(int argc, char **argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

TEST(WalfTest, WalfName)
{
  Wolf wolf;
  EXPECT_EQ("Wolf", wolf.Name());
  EXPECT_NE("Cat", wolf.Name());
}

TEST(MouseTest, TrueName)
{
  Mouse mouse;
  EXPECT_EQ("Mouse", mouse.Name());
}

TEST(MouseTest, FalseName)
{
  Mouse mouse;
  EXPECT_EQ("Cat", mouse.Name());
}

class TigerTest : public ::testing::Test
{
protected:
  void SetUp() override
  {
    cout << "TigerTest Test Case SetUp" << endl;
  }
  void TearDown() override
  {
    cout << "TigerTest Test Case TearDown" << endl;
  }

  Tiger tiger;
};

TEST_F(TigerTest, TrueName)
{
  EXPECT_EQ("Tiger", tiger.Name());
}

TEST_F(TigerTest, FalseName)
{
  EXPECT_EQ("Cat", tiger.Name());
}