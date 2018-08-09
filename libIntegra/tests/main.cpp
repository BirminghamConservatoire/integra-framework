////
////  main.cpp
////  UnitTests
////
////  Created by Jamie Bullock on 09/08/2018.
////  Copyright © 2018 Birmingham Conservatoire. All rights reserved.
////


#include "gtest.h"

using namespace testing;

TEST(CaseName, TestName)
{
    ASSERT_EQ(1, 0) << "Hello world";
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
