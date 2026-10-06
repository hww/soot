#pragma once

#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace sider {

    /// @brief Minimal command-line argument parser.
    /// @details Supported forms:
    ///            --key value      option with one value
    ///            --key            flag (no value)
    ///            positional       everything not starting with "--"
    ///          Repeated options are allowed: use get_all("base") to retrieve them.
    class Args {
    public:
        Args(int argc, const char **argv) {
            for (int i = 1; i < argc; ++i) {
                const std::string a = argv[i];
                if (a.size() >= 2 && a[0] == '-' && a[1] == '-') {
                    const std::string key = a.substr(2);
                    const bool        next_is_option = (i + 1 < argc) &&
                                                (std::string(argv[i + 1]).size() >= 2) &&
                                                (std::string(argv[i + 1])[0] == '-') &&
                                                (std::string(argv[i + 1])[1] == '-');
                    if (!next_is_option && i + 1 < argc) {
                        m_options[key].push_back(argv[++i]);
                    } else {
                        m_flags.insert(key);
                        m_options[key]; // ensure key exists with empty list
                    }
                } else {
                    m_positional.push_back(a);
                }
            }
        }

        [[nodiscard]] bool has(const std::string &key) const {
            return m_flags.count(key) > 0 ||
                   (m_options.count(key) > 0 && !m_options.at(key).empty());
        }

        [[nodiscard]] std::string get(const std::string &key,
                                      const std::string &default_value = {}) const {
            const auto it = m_options.find(key);
            if (it == m_options.end() || it->second.empty()) { return default_value; }
            return it->second.back();
        }

        [[nodiscard]] const std::vector<std::string> &get_all(const std::string &key) const {
            static const std::vector<std::string> empty;
            const auto                            it = m_options.find(key);
            return it != m_options.end() ? it->second : empty;
        }

        [[nodiscard]] std::string require(const std::string &key) const {
            const auto it = m_options.find(key);
            if (it == m_options.end() || it->second.empty()) {
                throw std::runtime_error("missing required option --" + key);
            }
            return it->second.back();
        }

        [[nodiscard]] const std::vector<std::string> &positional() const { return m_positional; }

    private:
        std::unordered_map<std::string, std::vector<std::string>> m_options;
        std::unordered_set<std::string>                           m_flags;
        std::vector<std::string>                                  m_positional;
    };

} // namespace sider