////
////  main.cpp
////  UnitTests
////
////  Created by Jamie Bullock on 09/08/2018.
////  Copyright © 2018 Birmingham Conservatoire. All rights reserved.
////

#include "server_startup_info.h"
#include "integra_session.h"
#include "interface_definition.h"
#include "error.h"
#include "server.h"
#include "trace.h"
#include "server_lock.h"
#include "command.h"
#include "path.h"

#include "gtest.h"

using namespace testing;
using namespace integra_api;

namespace k
{
    const std::string moduleDirectory           = "Integra.framework/Resources/modules";
    const std::string thirdPartyModuleDirectory = "third_party";
}

class IntegraTest : public ::testing::Test
{
protected:
    
    IntegraTest()
    {
        sinfo.system_module_directory       = k::moduleDirectory;
        sinfo.third_party_module_directory  = k::thirdPartyModuleDirectory;
        CTrace::set_categories_to_trace(false, false, false);
    }
    
    ~IntegraTest() override {}
    
    void SetUp() override {}
    void TearDown() override {}
    
    CServerStartupInfo sinfo;
};

// Test server startup
TEST_F(IntegraTest, SessionStartsCorrectly)
{
    CIntegraSession session;
    CError err = session.start_session(sinfo);
    ASSERT_EQ(err, CError::code::SUCCESS);
}

// Test server shutdown


int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
