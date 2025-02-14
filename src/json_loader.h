#pragma once

#include <filesystem>
#include <exception>
#include <string>

#include "model.h"

namespace json_loader {

const std::string x = "x";
const std::string y = "y";
const std::string x0 = "x0";
const std::string y0 = "y0";
const std::string x1 = "x1";
const std::string y1 = "y1";
const std::string width = "w";
const std::string height = "h";
const std::string offsetX = "offsetX";
const std::string offsetY = "offsetY";
const std::string id_key = "id";
const std::string name_key = "name";

class InvalidConfigureFile : public std::exception {
public:
    explicit InvalidConfigureFile(const std::string& message)
        : msg_("JsonParseException: " + message) {}
    const char* what() const noexcept override {
        return msg_.c_str();
    }
private:
    std::string msg_;
};

struct DefaultSettings {
    double global_default_speed = 0.001;
    int global_bag_capacity = 3; 
    double retirement_time = 60000.;
};

model::Game LoadGame(const std::filesystem::path& json_path);


}  // namespace json_loader
