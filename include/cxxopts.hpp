/*

Copyright (c) 2014, 2015, 2016, 2017 Jarryd Beck
Copyright (c) 2021 Pavel Artemkin

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.

*/

#ifndef CXXOPTS_HPP_INCLUDED
#define CXXOPTS_HPP_INCLUDED

#include <exception>
#include <list>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#ifdef __cpp_lib_optional
# include <optional>
# define CXXOPTS_HAS_OPTIONAL
#endif

#ifdef CXXOPTS_USE_UNICODE
# include <unicode/unistr.h>
#endif

#if __cplusplus >= 201603L
# define CXXOPTS_NODISCARD [[nodiscard]]
#else
# define CXXOPTS_NODISCARD
#endif

#if __cplusplus >= 201103L
# define CXXOPTS_NORETURN [[noreturn]]
#else
# define CXXOPTS_NORETURN
#endif

#ifndef CXXOPTS_VECTOR_DELIMITER
# define CXXOPTS_VECTOR_DELIMITER ','
#endif

#define CXXOPTS__VERSION_MAJOR 3
#define CXXOPTS__VERSION_MINOR 0
#define CXXOPTS__VERSION_PATCH 0

namespace cxxopts {

static constexpr struct {
  uint8_t major, minor, patch;
} version = {
  CXXOPTS__VERSION_MAJOR,
  CXXOPTS__VERSION_MINOR,
  CXXOPTS__VERSION_PATCH
};

#ifdef CXXOPTS_USE_UNICODE
  using String = icu::UnicodeString;
#else
  using String = std::string;
#endif

namespace detail {

template <typename T>
struct type_is_container {
  static constexpr bool value = false;
};

template <typename T>
struct type_is_container<std::vector<T>> {
  static constexpr bool value = true;
};

} // namespace detail


class option_error : public std::runtime_error {
public:
  explicit option_error(const std::string& what_arg);
};

class parse_error : public option_error {
public:
  explicit parse_error(const std::string& what_arg);
};

class spec_error : public option_error {
public:
  explicit spec_error(const std::string& what_arg);
};


class option_exists_error : public spec_error {
public:
  explicit option_exists_error(const std::string& option);
};

class invalid_option_format_error : public spec_error {
public:
  explicit invalid_option_format_error(const std::string& format);
};


class option_syntax_error : public parse_error {
public:
  explicit option_syntax_error(const std::string& text);
};

class option_not_exists_error : public parse_error {
public:
  explicit option_not_exists_error(const std::string& option);
};

class missing_argument_error : public parse_error {
public:
  explicit missing_argument_error(const std::string& option);
};

class option_requires_argument_error : public parse_error {
public:
  explicit option_requires_argument_error(const std::string& option);
};

class option_not_has_argument_error : public parse_error {
public:
  option_not_has_argument_error (
    const std::string& option,
    const std::string& arg);
};

class option_not_present_error : public parse_error {
public:
  explicit option_not_present_error(const std::string& option);
};

class argument_incorrect_type : public parse_error {
public:
  explicit argument_incorrect_type (const std::string& arg);
};

class option_has_no_value_error : public option_error {
public:
  explicit option_has_no_value_error(const std::string& option);
};

template <typename T>
CXXOPTS_NORETURN
void throw_or_mimic(const std::string& text) {
  static_assert(std::is_base_of<std::exception, T>::value,
                "throw_or_mimic only works on std::exception and "
                "deriving classes");

#ifndef CXXOPTS_NO_EXCEPTIONS
  // If CXXOPTS_NO_EXCEPTIONS is not defined, just throw
  throw T{text};
#else
  // Otherwise manually instantiate the exception, print what() to stderr,
  // and exit
  T exception{text};
  std::cerr << exception.what() << std::endl;
  std::exit(EXIT_FAILURE);
#endif
}

#if defined(__GNUC__)
// GNU GCC with -Weffc++ will issue a warning regarding the upcoming class, we want to silence it:
// warning: base class 'class std::enable_shared_from_this<cxxopts::Value>' has accessible non-virtual destructor
#pragma GCC diagnostic ignored "-Wnon-virtual-dtor"
#pragma GCC diagnostic push
// This will be ignored under other compilers like LLVM clang.
#endif
class Value : public std::enable_shared_from_this<Value> {
public:
  virtual ~Value() = default;

  virtual
  std::shared_ptr<Value>
  clone() const = 0;

  virtual void
  parse(const std::string& text) const = 0;

  virtual void
  parse() const = 0;

  virtual bool
  has_default() const = 0;

  virtual bool
  has_env() const = 0;

  virtual bool
  has_implicit() const = 0;

  virtual std::string
  get_default_value() const = 0;

  virtual std::string
  get_env_var() const = 0;

  virtual std::string
  get_implicit_value() const = 0;

  virtual std::shared_ptr<Value>
  default_value(const std::string& value) = 0;

  virtual std::shared_ptr<Value>
  env(const std::string& var) = 0;

  virtual std::shared_ptr<Value>
  implicit_value(const std::string& value) = 0;

  virtual std::shared_ptr<Value>
  no_implicit_value() = 0;

  virtual bool
  is_boolean() const = 0;

  virtual bool
  is_container() const = 0;
};
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

  namespace values {
    void
    parse_value(const std::string& text, uint8_t& value);

    void
    parse_value(const std::string& text, int8_t& value);

    void
    parse_value(const std::string& text, uint16_t& value);

    void
    parse_value(const std::string& text, int16_t& value);

    void
    parse_value(const std::string& text, uint32_t& value);

    void
    parse_value(const std::string& text, int32_t& value);

    void
    parse_value(const std::string& text, uint64_t& value);

    void
    parse_value(const std::string& text, int64_t& value);

    void
    parse_value(const std::string& text, bool& value);

    void
    parse_value(const std::string& text, char& c);

    void
    parse_value(const std::string& text, std::string& value);

    template <typename T>
    void stringstream_parser(const std::string& text, T& value) {
      std::stringstream in{text};
      in >> value;
      if (!in) {
        throw_or_mimic<argument_incorrect_type>(text);
      }
    }

    // The fallback parser. It uses the stringstream parser to parse all types
    // that have not been overloaded explicitly.  It has to be placed in the
    // source code before all other more specialized templates.
    template <typename T>
    void
    parse_value(const std::string& text, T& value) {
      stringstream_parser(text, value);
    }

    template <typename T>
    void
    parse_value(const std::string& text, std::vector<T>& value) {
      if (text.empty()) {
        value.push_back(T());
        return;
      }
      std::stringstream in{text};
      std::string token;
      while(!in.eof() && std::getline(in, token, CXXOPTS_VECTOR_DELIMITER)) {
        T v;
        parse_value(token, v);
        value.emplace_back(std::move(v));
      }
    }

#ifdef CXXOPTS_HAS_OPTIONAL
    template <typename T>
    void
    parse_value(const std::string& text, std::optional<T>& value) {
      T result;
      parse_value(text, result);
      value = std::move(result);
    }
#endif

    template <typename T>
    class abstract_value : public Value {
      using Self = abstract_value<T>;

    public:
      abstract_value()
        : result_(std::make_shared<T>())
        , store_(result_.get())
      {
      }

      explicit abstract_value(T* t)
        : store_(t)
      {
      }

      abstract_value& operator=(const abstract_value&) = default;

      abstract_value(const abstract_value& rhs) {
        if (rhs.result_) {
          result_ = std::make_shared<T>();
          store_ = result_.get();
        } else {
          store_ = rhs.store_;
        }

        default_ = rhs.default_;
        implicit_ = rhs.implicit_;
        default_value_ = rhs.default_value_;
        implicit_value_ = rhs.implicit_value_;
      }

      void
      parse(const std::string& text) const override {
        parse_value(text, *store_);
      }

      void
      parse() const override {
        parse_value(default_value_, *store_);
      }

      bool
      has_default() const override {
        return default_;
      }

      bool
      has_env() const override {
        return env_;
      }

      bool
      has_implicit() const override {
        return implicit_;
      }

      std::shared_ptr<Value>
      default_value(const std::string& value) override {
        default_ = true;
        default_value_ = value;
        return shared_from_this();
      }

      std::shared_ptr<Value>
      env(const std::string& var) override {
        env_ = true;
        env_var_ = var;
        return shared_from_this();
      }

      std::shared_ptr<Value>
      implicit_value(const std::string& value) override {
        implicit_ = true;
        implicit_value_ = value;
        return shared_from_this();
      }

      std::shared_ptr<Value>
      no_implicit_value() override {
        implicit_ = false;
        return shared_from_this();
      }

      std::string
      get_default_value() const override {
        return default_value_;
      }

      std::string
      get_env_var() const override {
        return env_var_;
      }

      std::string
      get_implicit_value() const override {
        return implicit_value_;
      }

      const T&
      get() const {
        if (store_ == nullptr) {
          return *result_;
        }
        return *store_;
      }

      bool
      is_boolean() const override {
        return std::is_same<T, bool>::value;
      }

      bool
      is_container() const override {
        return detail::type_is_container<T>::value;
      }

    protected:
      std::shared_ptr<T> result_;
      T* store_{};

      std::string default_value_;
      std::string env_var_;
      std::string implicit_value_;

      bool default_{false};
      bool implicit_{false};
      bool env_{false};
    };

    template <typename T>
    class standard_value : public abstract_value<T> {
    public:
      using abstract_value<T>::abstract_value;

      CXXOPTS_NODISCARD
      std::shared_ptr<Value>
      clone() const override {
        return std::make_shared<standard_value<T>>(*this);
      }
    };

    template <>
    class standard_value<bool> : public abstract_value<bool> {
    public:
      standard_value() {
        set_default_and_implicit();
      }

      explicit standard_value(bool* b)
        : abstract_value(b)
      {
        set_default_and_implicit();
      }

      std::shared_ptr<Value>
      clone() const override {
        return std::make_shared<standard_value<bool>>(*this);
      }

    private:
      void
      set_default_and_implicit() {
        default_ = true;
        default_value_ = "false";
        implicit_ = true;
        implicit_value_ = "true";
      }
    };
  } // namespace values

  template <typename T>
  std::shared_ptr<Value>
  value() {
    return std::make_shared<values::standard_value<T>>();
  }

  template <typename T>
  std::shared_ptr<Value>
  value(T& t) {
    return std::make_shared<values::standard_value<T>>(&t);
  }

  class OptionDetails {
  public:
    OptionDetails(
      std::string short_name,
      std::string long_name,
      String desc,
      std::shared_ptr<const Value> val);

    OptionDetails(const OptionDetails& rhs);

    OptionDetails(OptionDetails&& rhs) = default;

    CXXOPTS_NODISCARD
    const String&
    description() const;

    CXXOPTS_NODISCARD
    const Value&
    value() const;

    CXXOPTS_NODISCARD
    std::shared_ptr<Value>
    make_storage() const;

    CXXOPTS_NODISCARD
    const std::string&
    short_name() const;

    CXXOPTS_NODISCARD
    const std::string&
    long_name() const;

    size_t
    hash() const noexcept;

  private:
    std::string short_;
    std::string long_;
    String desc_;
    std::shared_ptr<const Value> value_;
    int count_;
    size_t hash_;
  };

  struct HelpOptionDetails {
    std::string s;
    std::string l;
    String desc;
    std::string default_value;
    std::string implicit_value;
    std::string arg_help;
    bool has_implicit;
    bool has_default;
    bool is_container;
    bool is_boolean;
  };

  struct HelpGroupDetails {
    std::string name;
    std::string description;
    std::vector<HelpOptionDetails> options;
  };

  class OptionValue {
  public:
    void
    parse(
      const std::shared_ptr<const OptionDetails>& details,
      const std::string& text);

    void
    parse_default(const std::shared_ptr<const OptionDetails>& details);

    void
    parse_no_value(const std::shared_ptr<const OptionDetails>& details);

    CXXOPTS_NODISCARD
    size_t
    count() const noexcept;

    // TODO: maybe default options should count towards the number of arguments
    CXXOPTS_NODISCARD
    bool
    has_default() const noexcept;

    CXXOPTS_NODISCARD
    bool
    has_value() const noexcept;

    template <typename T>
    const T&
    as() const {
      if (!has_value()) {
        throw_or_mimic<option_has_no_value_error>(
          long_name_ == nullptr ? "" : *long_name_);
      }
#ifdef CXXOPTS_NO_RTTI
      return static_cast<const values::standard_value<T>&>(*value_).get();
#else
      return dynamic_cast<const values::standard_value<T>&>(*value_).get();
#endif
    }

  private:
    void
    ensure_value(const std::shared_ptr<const OptionDetails>& details);

    const std::string* long_name_{nullptr};
    // Holding this pointer is safe, since OptionValue's only exist
    // in key-value pairs, where the key has the string we point to.
    std::shared_ptr<Value> value_;
    size_t count_{0};
    bool default_{false};
  };

  class KeyValue {
  public:
    KeyValue(std::string key_, std::string value_);

    CXXOPTS_NODISCARD
    const std::string&
    key() const;

    CXXOPTS_NODISCARD
    const std::string&
    value() const;

    template <typename T>
    T
    as() const {
      T result;
      values::parse_value(value_, result);
      return result;
    }

  private:
    const std::string key_;
    const std::string value_;
  };

  /// Maps option name to hash of the name.
  using NameHashMap = std::unordered_map<std::string, size_t>;
  /// Maps hash of an option name to the option value.
  using ParsedHashMap = std::unordered_map<size_t, OptionValue>;

  class ParseResult {
  public:
    ParseResult() = default;
    ParseResult(const ParseResult&) = default;
    ParseResult(
        NameHashMap&& keys,
        ParsedHashMap&& values,
        std::vector<KeyValue> sequential,
        std::vector<std::string>&& unmatched_args);

    ParseResult& operator=(ParseResult&&) = default;
    ParseResult& operator=(const ParseResult&) = default;

    size_t
    count(const std::string& o) const;

    const OptionValue&
    operator[](const std::string& option) const;

    const std::vector<KeyValue>&
    arguments() const;

    const std::vector<std::string>&
    unmatched() const;

  private:
    NameHashMap keys_;
    ParsedHashMap values_;
    std::vector<KeyValue> sequential_;
    std::vector<std::string> unmatched_;
  };

  struct Option {
    Option(
      std::string opts,
      std::string desc,
      std::shared_ptr<const Value> value = ::cxxopts::value<bool>(),
      std::string arg_help = "");

    std::string opts_;
    std::string desc_;
    std::shared_ptr<const Value> value_;
    std::string arg_help_;
  };

  using OptionMap = std::unordered_map<std::string, std::shared_ptr<OptionDetails>>;
  using PositionalList = std::vector<std::string>;

  class Options {
  public:
    class OptionAdder {
    public:
      OptionAdder(const std::string group, Options& options);

      OptionAdder&
      operator() (
        const std::string& opts,
        const std::string& desc,
        const std::shared_ptr<const Value>& value = ::cxxopts::value<bool>(),
        const std::string arg_help = {});

    private:
      const std::string group_;
      Options& options_;
    };

  public:
    explicit Options(std::string program, std::string help_string = {});

    Options&
    positional_help(std::string help_text);

    Options&
    custom_help(std::string help_text);

    Options&
    show_positional_help();

    Options&
    allow_unrecognised_options();

    Options&
    set_width(size_t width);

    Options&
    set_tab_expansion(bool expansion=true);

    ParseResult
    parse(int argc, const char* const* argv);

    OptionAdder
    add_options(std::string group = {});

    void
    add_options(
      const std::string& group,
      std::initializer_list<Option> options);

    void
    add_option(
      const std::string& group,
      const Option& option);

    void
    add_option(
      const std::string& group,
      const std::string& s,
      const std::string& l,
      std::string desc,
      const std::shared_ptr<const Value>& value,
      std::string arg_help);

    //parse positional arguments into the given option
    void
    parse_positional(std::string option);

    void
    parse_positional(std::vector<std::string> options);

    void
    parse_positional(std::initializer_list<std::string> options);

    template <typename Iterator>
    void
    parse_positional(Iterator begin, Iterator end) {
      parse_positional(std::vector<std::string>{begin, end});
    }

    std::string
    help(const std::vector<std::string>& groups = {}) const;

    std::vector<std::string>
    groups() const;

    const HelpGroupDetails&
    group_help(const std::string& group) const;

  private:
    void
    add_one_option(
      const std::string& option,
      const std::shared_ptr<OptionDetails>& details);

    String
    help_one_group(const std::string& group) const;

    void
    generate_group_help(
      String& result,
      const std::vector<std::string>& groups) const;

    void
    generate_all_groups_help(String& result) const;

    const std::string program_;
    const String help_string_{};
    std::string custom_help_;
    std::string positional_help_;
    size_t width_;
    bool show_positional_;
    bool allow_unrecognised_;
    bool tab_expansion_;

    std::shared_ptr<OptionMap> options_;
    PositionalList positional_;
    std::unordered_set<std::string> positional_set_;

    /// Mapping from groups to help options.
    std::map<std::string, HelpGroupDetails> help_;

    std::list<OptionDetails> option_list_;
    std::unordered_map<std::string, decltype(option_list_)::iterator>
      option_map_;
  };
} // namespace cxxopts

#endif //CXXOPTS_HPP_INCLUDED
