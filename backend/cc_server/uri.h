#ifndef URI_H_
#define URI_H_

#include <string>
#include <algorithm>

namespace http_server {
    class Uri {
        public:
            Uri() = default;
            explicit Uri(const std::string& path) : path_(path) {
                SetPathToLowercase();
            }
            ~Uri() = default;

            inline bool operator<(const Uri& other) const {
                return path_ < other.path_;
            }
            inline bool operator==(const Uri& other) const {
                return path_ == other.path_;
            }

            void SetPath(const std::string& path) {
                path_ = std::move(path);
                SetPathToLowercase();
            }

            std::string path() const { 
                return path_; 
            }

        private:
            std::string path_;

            void SetPathToLowercase() {
                std::transform(path_.begin(), path_.end(), path_.begin(), [](char c) {
                    return tolower(c);
                });
            }
    };
}

#endif