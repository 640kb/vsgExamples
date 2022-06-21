
#include "Log.h"

#include <iostream>

using namespace vsg;

ref_ptr<Logger>& vsg::logger()
{
    static ref_ptr<Logger> s_logger = StdLogger::create();
    return s_logger;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// StdLogger
//
StdLogger::StdLogger() {}

void StdLogger::debug_implementation(const std::string& message)
{
    std::cout<<message<<'\n';
}

void StdLogger::info_implementation(const std::string& message)
{
    std::cout<<message;
    std::cout.put('\n');
}

void StdLogger::warn_implementation(const std::string& message)
{
    std::cerr<<message<<std::endl;
}

void StdLogger::error_implementation(const std::string& message)
{
    std::cerr<<message<<std::endl;
}

