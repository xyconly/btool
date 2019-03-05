/*************************************************
File name:  uuid.hpp
Author:     AChar
Version:
Date:
Purpose: ����boost��ȡuuid��ֵ
*************************************************/

#pragma once

#include <string>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/uuid_generators.hpp>

namespace BTool
{
    class UUID
    {
    public:
        static std::string GetUUID()
        {
            return boost::uuids::to_string(boost::uuids::random_generator()());
        }
    };
}
