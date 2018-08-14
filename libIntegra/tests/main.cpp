////
////  main.cpp
////  UnitTests
////
////  Created by Jamie Bullock on 09/08/2018.
////  Copyright © 2018 Birmingham Conservatoire. All rights reserved.
////

#include "util.hpp"

#include "server_startup_info.h"
#include "integra_session.h"
#include "interface_definition.h"
#include "error.h"
#include "server.h"
#include "trace.h"
#include "server_lock.h"
#include "guid_helper.h"
#include "command.h"
#include "path.h"

#include "../src/node.h"

#include "gtest.h"



using namespace testing;
using namespace integra_api;

namespace k
{
    const std::string moduleDirectory           = "Integra.framework/Resources/modules";
    const std::string thirdPartyModuleDirectory = "third_party";
    const std::string versionFileName           = "VERSION";
    const std::string tapDelayGUID              = "c811c1b6-24b4-5a7a-065a-2c12cf061d4b";
    const std::string tapDelayName              = "TapDelay1";
    const std::string tapDelayEndpoint          = tapDelayName + "." + "delayTime";
    const float testFloatValue                  = 1.5f;
}

class SessionTest : public ::testing::Test
{
protected:
    
    SessionTest()
    {
        sinfo.system_module_directory       = k::moduleDirectory;
        sinfo.third_party_module_directory  = k::thirdPartyModuleDirectory;
        CTrace::set_categories_to_trace(false, false, false);
    }
    
    ~SessionTest() override {}
    
    void SetUp() override {}
    void TearDown() override {}
    
    CServerStartupInfo sinfo;
};

class ServerTest : public SessionTest
{
protected:
    
    ServerTest()
    {
        CError err = session.start_session(sinfo);
        assert(err == CError::SUCCESS);
    }
    
    ~ServerTest() override
    {
        CError err = session.end_session();
        assert(err == CError::SUCCESS);
    }

    void SetUp() override {}
    void TearDown() override {}

    CServerLock server()
    {
        return session.get_server();
    }
    
    CIntegraSession session;
};

class CommandTest : public ServerTest
{
protected:
    void SetUp() override
    {
        GUID guid;
        CGuidHelper::string_to_guid(k::tapDelayGUID, guid);
        
        CError err = server()->process_command(INewCommand::create(guid, k::tapDelayName, CPath()));
        assert(err == CError::SUCCESS);
    }
    
    void TearDown() override {}

};

#pragma mark - Test session startup

TEST_F(SessionTest, SessionStartsCorrectly)
{
    CIntegraSession session;
    CError err = session.start_session(sinfo);
    
    ASSERT_EQ(err, CError::code::SUCCESS);
}

#pragma mark - Test session shutdown

TEST_F(SessionTest, SessionEndsCorrectly)
{
    CIntegraSession session;
    session.start_session(sinfo);
    CError err = session.end_session();
    
    ASSERT_EQ(err, CError::code::SUCCESS);
}

#pragma mark - Test server version

TEST_F(ServerTest, VersionIsCorrect)
{
    auto libIntegraVersion = server()->get_libintegra_version();
    
    std::ifstream versionFile(k::versionFileName, std::ios::in);
    std::string expectedVersion;
    
    assert(!versionFile.fail());
    std::getline(versionFile, expectedVersion);
    
    ASSERT_EQ(libIntegraVersion, expectedVersion);
}

#pragma mark -  Test server default state

TEST_F(ServerTest, AllModuleIdsIsCorrect)
{
    auto rv = server()->get_all_module_ids();
    ASSERT_EQ(rv.size(), number_of_files_in_directory(k::moduleDirectory.c_str()));
}

TEST_F(ServerTest, FindInterfaceReturnsNull)
{
    auto rv = server()->find_interface(GUID());
    ASSERT_EQ(rv, nullptr);
}

TEST_F(ServerTest, NodesEmpty)
{
    auto rv = server()->get_nodes();
    ASSERT_TRUE(rv.empty());
}

TEST_F(ServerTest, FindNodeReturnsNull)
{
    auto rv = server()->find_node(CPath());
    ASSERT_EQ(rv, nullptr);
}

TEST_F(ServerTest, GetSiblingsEmpty)
{
    auto rv = server()->get_siblings(integra_internal::CNode());
    ASSERT_TRUE(rv.empty());
}

TEST_F(ServerTest, FindNodeEndpointReturnsNull)
{
    auto rv = server()->find_node_endpoint(CPath());
    ASSERT_EQ(rv, nullptr);
}

TEST_F(ServerTest, GetValueReturnsNull)
{
    auto rv = server()->get_value(CPath());
    ASSERT_EQ(rv, nullptr);
}

TEST_F(ServerTest, DumpStateSuccess)
{
    server()->dump_libintegra_state();
}

TEST_F(ServerTest, PingAllSuccess)
{
    server()->ping_all_dsp_modules();
}

#pragma mark -  Test server commands

TEST_F(ServerTest, NewCommandSuccess)
{
    GUID guid;
    CGuidHelper::string_to_guid(k::tapDelayGUID, guid);
    
    CError err = server()->process_command(INewCommand::create(guid, k::tapDelayName, CPath()));
    
    ASSERT_EQ(err, CError::SUCCESS);
}

TEST_F(ServerTest, NewCommandCreatesInstance)
{
    GUID guid;
    CGuidHelper::string_to_guid(k::tapDelayGUID, guid);
    
    CError err = server()->process_command(INewCommand::create(guid, k::tapDelayName, CPath()));
    assert(err == CError::SUCCESS);

    auto node = server()->get_nodes().at(k::tapDelayName);
    ASSERT_EQ(node->get_name(), k::tapDelayName);
}

TEST_F(CommandTest, DeleteCommandSuccess)
{
    CError err = server()->process_command(IDeleteCommand::create(CPath(k::tapDelayName)));
    ASSERT_EQ(err, CError::SUCCESS);
}

TEST_F(CommandTest, DeleteCommandRemovesInstance)
{
    CError err = server()->process_command(IDeleteCommand::create(CPath(k::tapDelayName)));
    assert(err == CError::SUCCESS);
    
    auto nodes = server()->get_nodes();
    ASSERT_TRUE(nodes.empty());
}

TEST_F(CommandTest, SetCommandSuccess)
{
    CError err = server()->process_command(ISetCommand::create(k::tapDelayEndpoint, CFloatValue(k::testFloatValue)));
    ASSERT_EQ(err, CError::SUCCESS);
}

TEST_F(CommandTest, SetCommandPathError)
{
    CError err = server()->process_command(ISetCommand::create(std::string(), CFloatValue(k::testFloatValue)));
    ASSERT_EQ(err, CError::PATH_ERROR);
}

TEST_F(CommandTest, SetCommandTypeError)
{
    CError err = server()->process_command(ISetCommand::create(k::tapDelayEndpoint, CStringValue("")));
    ASSERT_EQ(err, CError::TYPE_ERROR);
}

TEST_F(CommandTest, SetCommandConstraintError)
{
    CError err = server()->process_command(ISetCommand::create(k::tapDelayEndpoint, CFloatValue(100000.f)));
    ASSERT_EQ(err, CError::CONSTRAINT_ERROR);
}

TEST_F(CommandTest, SetCommandSetsValue)
{
    CError err = server()->process_command(ISetCommand::create(k::tapDelayEndpoint, CFloatValue(k::testFloatValue)));
    assert(err == CError::SUCCESS);
    
    auto endpoint = server()->find_node_endpoint(k::tapDelayEndpoint);
    float value = *endpoint->get_value();
    
    ASSERT_EQ(value, k::testFloatValue);
}



#pragma mark - Test module manager


#pragma mark - Test dump DSP state


#pragma mark - main

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
