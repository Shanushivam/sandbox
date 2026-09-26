#include "sandboxx/sandbox_config.hpp"
#include <cctype>
#include <climits>
#include <limits>
#include <type_traits>
#include <fstream>
#include <sstream>

namespace sandboxx {
namespace {
struct Value {
    enum class Kind { Bool, Integer, String } kind = Kind::Bool;
    bool boolean = false;
    long long integer = 0;
    std::string string;
};

class Parser {
public:
    explicit Parser(const std::string& text) : text_(text) {}

    bool parse(SandboxConfig& config, std::string& error) {
        skip_ws();
        if (!expect('{')) return fail(error);
        skip_ws();
        if (peek() == '}') {
            ++pos_;
        } else {
            while (true) {
                std::string key;
                Value value;
                skip_ws();
                if (!parse_string(key)) return fail(error);
                skip_ws();
                if (!expect(':')) return fail(error);
                skip_ws();
                if (!parse_value(value)) return fail(error);
                if (!apply(key, value, config)) return fail(error);
                skip_ws();
                if (peek() == ',') { ++pos_; continue; }
                if (!expect('}')) return fail(error);
                break;
            }
        }
        skip_ws();
        if (pos_ != text_.size()) {
            error_ = "unexpected trailing content";
            return fail(error);
        }
        return true;
    }

private:
    char peek() const { return pos_ < text_.size() ? text_[pos_] : '\0'; }

    void skip_ws() {
        while (pos_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[pos_]))) ++pos_;
    }

    bool expect(char c) {
        if (peek() != c) {
            error_ = std::string("expected '") + c + "'";
            return false;
        }
        ++pos_;
        return true;
    }

    bool fail(std::string& error) const {
        error = error_ + " at offset " + std::to_string(pos_);
        return false;
    }

    bool parse_string(std::string& out) {
        if (!expect('"')) return false;
        while (pos_ < text_.size() && text_[pos_] != '"') {
            char c = text_[pos_++];
            if (c == '\\') {
                char esc = peek();
                ++pos_;
                switch (esc) {
                    case '"': out += '"'; break;
                    case '\\': out += '\\'; break;
                    case '/': out += '/'; break;
                    case 'n': out += '\n'; break;
                    case 't': out += '\t'; break;
                    default: error_ = "unsupported escape sequence"; return false;
                }
            } else {
                out += c;
            }
        }
        return expect('"');
    }

    bool parse_value(Value& value) {
        char c = peek();
        if (c == '"') {
            value.kind = Value::Kind::String;
            return parse_string(value.string);
        }
        if (text_.compare(pos_, 4, "true") == 0) {
            value.kind = Value::Kind::Bool;
            value.boolean = true;
            pos_ += 4;
            return true;
        }
        if (text_.compare(pos_, 5, "false") == 0) {
            value.kind = Value::Kind::Bool;
            value.boolean = false;
            pos_ += 5;
            return true;
        }
        if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) {
            size_t start = pos_;
            if (c == '-') ++pos_;
            while (std::isdigit(static_cast<unsigned char>(peek()))) ++pos_;
            if (peek() == '.' || peek() == 'e' || peek() == 'E') {
                error_ = "only integer numbers are supported";
                return false;
            }
            std::string digits = text_.substr(start, pos_ - start);
            if (digits == "-" || digits.size() > 18) {
                error_ = "invalid number";
                return false;
            }
            value.kind = Value::Kind::Integer;
            value.integer = std::stoll(digits);
            return true;
        }
        error_ = "expected a string, integer or boolean";
        return false;
    }

    bool type_error(const std::string& key, const char* type) {
        error_ = "\"" + key + "\" must be " + type;
        return false;
    }

    bool apply(const std::string& key, const Value& v, SandboxConfig& config) {
        using Kind = Value::Kind;
        auto as_bool = [&](bool& field) {
            if (v.kind != Kind::Bool) return type_error(key, "a boolean");
            field = v.boolean;
            return true;
        };
        auto as_int = [&](auto& field) {
            using T = std::remove_reference_t<decltype(field)>;
            if (v.kind != Kind::Integer) return type_error(key, "an integer");
            if (v.integer < std::numeric_limits<T>::min() ||
                v.integer > std::numeric_limits<T>::max()) {
                return type_error(key, "in range");
            }
            field = static_cast<T>(v.integer);
            return true;
        };
        auto as_string = [&](std::string& field) {
            if (v.kind != Kind::String) return type_error(key, "a string");
            field = v.string;
            return true;
        };

        if (key == "cpu_percent") return as_int(config.cpu_percent);
        if (key == "memory_mb") return as_int(config.memory_mb);
        if (key == "timeout_seconds") return as_int(config.timeout_seconds);
        if (key == "pid_namespace") return as_bool(config.pid_namespace);
        if (key == "mount_namespace") return as_bool(config.mount_namespace);
        if (key == "network_namespace") return as_bool(config.network_namespace);
        if (key == "restricted_filesystem") return as_bool(config.restricted_filesystem);
        if (key == "security_policy") return as_bool(config.security_policy);
        if (key == "hostname") return as_string(config.hostname);
        if (key == "rootfs") return as_string(config.rootfs);
        error_ = "unknown key \"" + key + "\"";
        return false;
    }

    const std::string& text_;
    size_t pos_ = 0;
    std::string error_;
};
}

bool parse_config(const std::string& json, SandboxConfig& config, std::string& error) {
    SandboxConfig parsed = config;
    if (!Parser(json).parse(parsed, error)) return false;
    config = parsed;
    return true;
}

bool load_config(const std::string& path, SandboxConfig& config, std::string& error) {
    std::ifstream file(path);
    if (!file) {
        error = "cannot open " + path;
        return false;
    }
    std::ostringstream text;
    text << file.rdbuf();
    if (!parse_config(text.str(), config, error)) {
        error = path + ": " + error;
        return false;
    }
    return true;
}

bool validate_config(const SandboxConfig& config, std::string& error) {
    if (config.cpu_percent < 0) error = "cpu_percent must be >= 0";
    else if (config.memory_mb < 0) error = "memory_mb must be >= 0";
    else if (config.timeout_seconds < 0) error = "timeout_seconds must be >= 0";
    else if (config.hostname.empty() || config.hostname.size() > 64)
        error = "hostname must be 1-64 characters";
    else if (config.restricted_filesystem && !config.mount_namespace)
        error = "restricted_filesystem requires mount_namespace";
    else if (config.restricted_filesystem && config.rootfs.empty())
        error = "rootfs must be set when restricted_filesystem is enabled";
    else return true;
    return false;
}
}
