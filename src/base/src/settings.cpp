#include "mksync/base/settings.hpp"

#include <fstream>
#include <rapidjson/prettywriter.h>
#include <rapidjson/ostreamwrapper.h>
#include <rapidjson/istreamwrapper.h>
#include <spdlog/spdlog.h>

namespace mks::base
{
    Settings::Settings()
    {
        _document.SetObject();
    }

    Settings::Settings(std::string_view file)
    {
        if (std::filesystem::exists(file)) {
            _file = file;
            load(file);
            return;
        }
        if (!file.empty()) {
            // 创建多层目录
            std::filesystem::path path(file);
            std::filesystem::create_directories(path.parent_path());
            _file = file;
        }
        _document.SetObject();
    }

    Settings::~Settings()
    {
        if (!_file.empty()) {
            save();
        }
    }

    bool Settings::save() const
    {
        if (!std::filesystem::exists(_file)) {
            return false;
        }
        std::ofstream ofs(_file);
        if (!ofs.is_open()) {
            return false;
        }
        rapidjson::OStreamWrapper ostream(ofs);
        rapidjson::PrettyWriter   writer(ostream);
        _document.Accept(writer);
        writer.Flush();
        ofs.close();
        return true;
    }

    bool Settings::save(std::string_view file)
    {
        _file = file;
        return save();
    }

    bool Settings::load()
    {
        if (!std::filesystem::exists(_file)) {
            return false;
        }
        std::ifstream ifs(_file);
        if (!ifs.is_open()) {
            return false;
        }
        rapidjson::IStreamWrapper istream(ifs);
        _document.ParseStream(istream);
        return true;
    }

    bool Settings::load(std::string_view file)
    {
        _file = file;
        return load();
    }
} // namespace mks::base