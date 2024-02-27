#ifndef IMAGE_HANDLER_H_
#define IMAGE_HANDLER_H_

#include <string>
#include <fstream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <cstdio>

#include "base64/base64.h"
#include "python/include/Python.h"

namespace http_server {

    static std::string file_base = "images/";
    static std::string AI_module_path = "../AI_module/";
    static const wchar_t* PYTHONHOME_V = L"C:/Users/wd2711/AppData/Local/Programs/Python/Python39";
    static const wchar_t* PYTHONPATH_V = L"C:/Users/wd2711/AppData/Local/Programs/Python/Python39/Lib;C:/Users/wd2711/AppData/Local/Programs/Python/Python39/DLLs";

    std::string filename_generate(const std::string s) {
        auto now = std::chrono::system_clock::now();
        std::time_t currentTime = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&currentTime), "%Y%m%d_%H%M%S");
        std::string fileName = s + "file_" + ss.str() + ".jpg";
        return fileName;
    }

    boolean set_env() {
        // Set PYTHONHOME
        const wchar_t* PYTHONHOME_N = L"PYTHONHOME";
        if (!SetEnvironmentVariableW(PYTHONHOME_N ,PYTHONHOME_V )) {
            std::cerr << "Failed to set PYTHONHOME environment variable." << std::endl;
            return false;
        }      

        // Set PYTHONPATH
        const wchar_t* PYTHONPATH_N = L"PYTHONPATH";
        if (!SetEnvironmentVariableW(PYTHONPATH_N ,PYTHONPATH_V )) {
            std::cerr << "Failed to set PYTHONHOME environment variable." << std::endl;
            return false;
        }   

        return true; 
    }

    std::string read_result_file() {
        std::ifstream file("result.txt");
        std::string s;

        if (file.is_open()) {
            std::getline(file, s);
            file.close();
            return s;
        } else {
            return "Result.txt open fail.";
        }
    }
    std::string run_python_model(const std::string fileName) {
        Py_Initialize();
        PyRun_SimpleString("import os");

        std::string demo_file_path = AI_module_path + "demo.py";
        std::string model_file_path = AI_module_path + "BEST_checkpoint_.pth.tar";
        std::string word_map_file_path = AI_module_path + "data/WORDMAP.json";

        std::string command;
        command += "python.exe ";
        command += demo_file_path;
        command += " --img ";
        command += fileName;
        command += " --model ";
        command += model_file_path;
        command += " --word_map ";
        command += word_map_file_path;
        command += " --beam_size 5";

        std::string full_command = "os.system('" + std::string(command) + "')";
        if (PyRun_SimpleString(full_command.c_str()) < 0)
        {
            return "Python command run error.";
        }

        return read_result_file();
    }

    std::string model_process(const std::string fileName) {
        if (!set_env()) {
            return "Environment variable set error.";
        }
        return run_python_model(fileName);
    }

    std::string request_handler(const std::string content, const size_t len) {
        size_t commaPos = content.find(',');
        if (commaPos != std::string::npos) {
            std::string image = content.substr(commaPos + 1);
            if (image.size() % 4 != 0)
                return "Invalid image transfer#1.";
            
            std::string decodedImage = base64_decode(image);
            std::string fileName = filename_generate(file_base);
            std::ofstream file(fileName, std::ios::binary);
            
            file.write(decodedImage.c_str(), decodedImage.length());
            if(!file.good()) {
                return "Save error#1.";
            }
            
            file.close();
            if(!file.good()) {
                return "Save error#2.";
            }

            return model_process(fileName);         
        } else {
            return "Invalid image transfer#2.";
        }
    }
}

#endif