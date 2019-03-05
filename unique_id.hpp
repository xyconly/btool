/*************************************************
File name:  unique_id.hpp
Author:     AChar
Version:
Date:
Purpose: �Ը���ΨһID����
*************************************************/

#pragma once

#include <string>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/uuid_generators.hpp>

namespace BTool
{
    class UniqueID
    {
    public:
        // ����UUID
        static std::string GetUUID() {
            return boost::uuids::to_string(boost::uuids::random_generator()());
        }
    };
}
