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
#include <iterator>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <type_traits>
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

#if __cplusplus >= 200809L
# define CXXOPTS_NORETURN [[noreturn]]
#else
# define CXXOPTS_NORETURN
#endif

#ifndef CXXOPTS_VECTOR_DELIMITER
# define CXXOPTS_VECTOR_DELIMITER ','
#endif

#define CXXOPTS__VERSION_MAJOR 4
#define CXXOPTS__VERSION_MINOR 0
#define CXXOPTS__VERSION_PATCH 0

namespace cxxopts {

class options;
class parse_result;

template <typename T>
struct value_parser;

static constexpr struct {
  uint8_t major, minor, patch;
} version = {
  CXXOPTS__VERSION_MAJOR,
  CXXOPTS__VERSION_MINOR,
  CXXOPTS__VERSION_PATCH
};

#ifdef CXXOPTS_USE_UNICODE
  using cxx_string = icu::UnicodeString;
#else
  using cxx_string = std::string;
#endif

/**
* \defgroup Exceptions
* @{
*/

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

class option_not_present_error : public parse_error {
public:
  explicit option_not_present_error(const std::string& option);
};

class argument_incorrect_type : public parse_error {
public:
  explicit argument_incorrect_type(
    const std::string& arg,
    const std::string& type = {});
};

class option_has_no_value_error : public option_error {
public:
  explicit option_has_no_value_error(const std::string& name);
};

template <typename T, typename ... Args>
CXXOPTS_NORETURN
void throw_or_mimic(Args&& ... args) {
  static_assert(std::is_base_of<std::exception, T>::value,
                "throw_or_mimic only works on std::exception and "
                "deriving classes");

#ifndef CXXOPTS_NO_EXCEPTIONS
  // If CXXOPTS_NO_EXCEPTIONS is not defined, just throw
  throw T{std::forward<Args>(args)...};
#else
  // Otherwise manually instantiate the exception, print what() to stderr,
  // and exit
  T exception{std::forward<Args>(args)...};
  std::cerr << exception.what() << std::endl;
  std::exit(EXIT_FAILURE);
#endif
}

/**@}*/


/**
* \defgroup Values setup and parsing
* @{
*/

/**
 * Settings for customizing parser behaviour.
 */
struct parse_context {
  char delimiter{CXXOPTS_VECTOR_DELIMITER};
};

namespace detail {

#if defined(__GNUC__)
// GNU GCC with -Weffc++ will issue a warning regarding the upcoming class, we want to silence it:
// warning: base class 'class std::enable_shared_from_this<cxxopts::value_base>' has accessible non-virtual destructor
# pragma GCC diagnostic ignored "-Wnon-virtual-dtor"
# pragma GCC diagnostic push
// This will be ignored under other compilers like LLVM clang.
#endif
class value_base : public std::enable_shared_from_this<value_base> {
public:
  value_base() = default;

  virtual ~value_base() = default;

  /** Returns whether the default value was set. */
  bool
  has_default() const noexcept {
    return default_;
  }

  /** Returns whether the env variable was set. */
  bool
  has_env() const noexcept {
    return env_;
  }

  /** Returns whether the implicit value was set. */
  bool
  has_implicit() const noexcept {
    return implicit_;
  }

  /** Returns default value. */
  std::string
  get_default_value() const {
    return default_value_;
  }

  /** Returns env variable. */
  std::string
  get_env_var() const {
    return env_var_;
  }

  /** Returns implicit value. */
  std::string
  get_implicit_value() const {
    return implicit_value_;
  }

  /**
   * Sets default value.
   *
   * Templated version is used to avoid implicit conversion 0u or nullptr
   * to a string, that leads to runtime error.
   */
  template <typename T>
  std::shared_ptr<value_base>
  default_value(T&& value) {
    default_ = true;
    default_value_.assign(std::forward<T>(value));
    return shared_from_this();
  }

  /** Sets delimiter for list values. */
  std::shared_ptr<value_base>
  delimiter(const char del) {
    parse_ctx_.delimiter = del;
    return shared_from_this();
  }

  /** Sets env variable. */
  template <typename T>
  std::shared_ptr<value_base>
  env(T&& var) {
    env_ = true;
    env_var_.assign(std::forward<T>(var));
    return shared_from_this();
  }

  /**
   * Sets implicit value.
   *
   * Templated version is used to avoid implicit conversion 0u or nullptr
   * to a string, that leads to runtime error.
   */
  template <typename T>
  std::shared_ptr<value_base>
  implicit_value(T&& value) {
    implicit_ = true;
    implicit_value_.assign(std::forward<T>(value));
    return shared_from_this();
  }

  /** Clears implicit value. */
  std::shared_ptr<value_base>
  no_implicit_value() {
    implicit_ = false;
    implicit_value_.clear();
    return shared_from_this();
  }

  /** Returns whether the type of the value is boolean. */
  bool
  is_boolean() const noexcept {
    return do_is_boolean();
  }

  /** Returns whether the type of the value is container. */
  bool
  is_container() const noexcept {
    return do_is_container();
  }

  /** Parses the given text into the value. */
  void
  parse(const std::string& text) const {
    return do_parse(parse_ctx_, text);
  }

  /** Parses the default value. */
  void
  parse() const {
    return do_parse(parse_ctx_, default_value_);
  }

protected:
  virtual bool
  do_is_boolean() const noexcept = 0;

  virtual bool
  do_is_container() const noexcept = 0;

  virtual void
  do_parse(const parse_context& ctx, const std::string& text) const = 0;

  void
  set_default_and_implicit() {
    if (is_boolean()) {
      default_ = true;
      default_value_ = "false";
      implicit_ = true;
      implicit_value_ = "true";
    }
  }

private:
  std::string default_value_{};
  std::string env_var_{};
  std::string implicit_value_{};
  parse_context parse_ctx_{};

  bool default_{false};
  bool env_{false};
  bool implicit_{false};
};
#if defined(__GNUC__)
# pragma GCC diagnostic pop
#endif

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
  std::istringstream in{text};
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
class basic_value : public value_base {
  using parser_type = value_parser<T>;
public:
  basic_value()
    : result_(new T{})
    , store_(result_.get())
  {
    set_default_and_implicit();
  }

  explicit basic_value(T* const t)
    : store_(t)
  {
    set_default_and_implicit();
  }

  const T&
  get() const {
    return *store_;
  }

protected:
  bool
  do_is_boolean() const noexcept final override {
    return std::is_same<T, bool>::value;
  }

  bool
  do_is_container() const noexcept final override {
    return parser_type::is_container;
  }

  void
  do_parse(const parse_context& ctx, const std::string& text) const override {
    parser_type().parse(ctx, text, *store_);
  }

private:
  basic_value(const basic_value& rhs) = delete;
  basic_value& operator=(const basic_value& rhs) = delete;

private:
  std::unique_ptr<T> result_{};
  T* store_{};
};

} // namespace detail

/**
 * A parser for values of type T.
 */
template <typename T>
struct value_parser {
  using value_type = T;
  /// By default, value can not act as container.
  static constexpr bool is_container = false;

  void parse(const parse_context&, const std::string& text, T& value) {
    detail::parse_value(text, value);
  }
};

template <typename T>
struct value_parser<std::vector<T>> {
  using value_type = T;
  /// Value of type std::vector<T> can act as container.
  static constexpr bool is_container = true;

  void parse(
    const parse_context& ctx,
    const std::string& text,
    std::vector<T>& value)
  {
    using parser_type = value_parser<T>;

    static_assert(!parser_type::is_container ||
                  !value_parser<typename parser_type::value_type>::is_container,
                  "dimensions of a container type should not exceed 2");

    if (text.empty()) {
      value.push_back(T());
    } else if (parser_type::is_container) {
      T v;
      parser_type().parse(ctx, text, v);
      value.emplace_back(std::move(v));
    } else {
      std::istringstream in{text};
      std::string token;
      while (!in.eof() && std::getline(in, token, ctx.delimiter)) {
        T v;
        parser_type().parse(ctx, token, v);
        value.emplace_back(std::move(v));
      }
    }
  }
};

/**
 * Creates value holder for the specific type.
 */
template <typename T>
std::shared_ptr<detail::basic_value<T>>
value() {
  return std::make_shared<detail::basic_value<T>>();
}

/**
 * Creates value holder for the specific type.
 */
template <typename T>
std::shared_ptr<detail::basic_value<T>>
value(T& t) {
  return std::make_shared<detail::basic_value<T>>(&t);
}

/**@}*/

class option_details {
public:
  option_details(
    std::string short_name,
    std::string long_name,
    cxx_string desc,
    std::shared_ptr<detail::value_base> val);

  CXXOPTS_NODISCARD
  const cxx_string&
  description() const;

  CXXOPTS_NODISCARD
  std::shared_ptr<detail::value_base>
  value() const;

  CXXOPTS_NODISCARD
  const std::string&
  short_name() const;

  CXXOPTS_NODISCARD
  const std::string&
  long_name() const;

  size_t
  hash() const noexcept;

private:
  /// Short name of the option.
  const std::string short_;
  /// Long name of the option.
  const std::string long_;
  /// Description of the option.
  const cxx_string desc_;
  const size_t hash_;
  const std::shared_ptr<detail::value_base> value_;
};

/**
 * Parsed value of an option.
 */
class option_value {
public:
  /**
   * A number of occurrences of the option value in
   * the command line arguments.
   */
  CXXOPTS_NODISCARD
  size_t
  count() const noexcept;

  // TODO: maybe default options should count towards the number of arguments
  CXXOPTS_NODISCARD
  bool
  has_default() const noexcept;

  /**
   * Returns true if there was a value for the option in the
   * command line arguments.
   */
  CXXOPTS_NODISCARD
  bool
  has_value() const noexcept;

  /**
   * Casts option value to the specific type.
   */
  template <typename T>
  const T&
  as() const {
    if (!has_value()) {
      throw_or_mimic<option_has_no_value_error>(long_name_);
    }
#ifdef CXXOPTS_NO_RTTI
    return static_cast<const detail::basic_value<T>&>(*value_).get();
#else
    return dynamic_cast<const detail::basic_value<T>&>(*value_).get();
#endif
  }

public:
  /**
   * Parses option value from the given text.
   */
  void
  parse(
    const std::shared_ptr<option_details>& details,
    const std::string& text);

  /**
   * Parses option value from the default value.
   */
  void
  parse_default(const std::shared_ptr<option_details>& details);

  void
  parse_no_value(const std::shared_ptr<option_details>& details);

private:
  void
  ensure_value(const std::shared_ptr<option_details>& details);

  std::string long_name_{};
  // Holding this pointer is safe, since option_value's only exist
  // in key-value pairs, where the key has the string we point to.
  std::shared_ptr<detail::value_base> value_{};
  size_t count_{0};
  bool default_{false};
};

/**
 * Provides the result of parsing of the command line arguments.
 */
class parse_result {
public:
  /// Maps option name to hash of the name.
  using name_hash_map = std::unordered_map<std::string, size_t>;
  /// Maps hash of an option name to the option value.
  using parsed_hash_map = std::unordered_map<size_t, option_value>;

  class key_value {
  public:
    key_value(std::string key, std::string value);

    CXXOPTS_NODISCARD
    const std::string& key() const;

    CXXOPTS_NODISCARD
    const std::string& value() const;

    /**
    * Parses the value to a variable of the specific type.
    */
    template <typename T>
    T as() const {
      T result;
      value_parser<T>().parse(parse_context(), value_, result);
      return result;
    }

  private:
    const std::string key_;
    const std::string value_;
  };

public:
  parse_result() = default;
  parse_result(const parse_result&) = default;
  parse_result(parse_result&&) = default;
  parse_result(
      name_hash_map&& keys,
      parsed_hash_map&& values,
      std::vector<key_value>&& sequential,
      std::vector<std::string>&& unmatched_args,
      size_t consumed);

  parse_result& operator=(const parse_result&) = default;
  parse_result& operator=(parse_result&&) = default;

  /**
   * Returns a number of occurrences of the option in
   * the command line arguments.
   */
  size_t
  count(const std::string& name) const;

  bool
  has(const std::string& name) const;

  const option_value&
  operator[](const std::string& name) const;

  /**
   * Returns list of recognized options with non empty value.
   */
  const std::vector<key_value>&
  arguments() const;

  /**
   * Returns number of consumed command line arguments.
   */
  size_t
  consumed() const;

  /**
   * Returns list of unmatched arguments.
   */
  const std::vector<std::string>&
  unmatched() const;

private:
  name_hash_map keys_{};
  parsed_hash_map values_{};
  std::vector<key_value> sequential_{};
  /// List of arguments that did not match any defined option.
  std::vector<std::string> unmatched_{};
  /// Number of consument command line arguments.
  size_t consumed_arguments_{0};
};

class option {
public:
  option(
    std::string opts,
    std::string desc,
    std::shared_ptr<detail::value_base> value = ::cxxopts::value<bool>(),
    std::string arg_help = {});

private:
  friend class ::cxxopts::options;

  /// Short and long names of the option.
  const std::string opts_;
  /// Description of the option.
  const std::string desc_;
  const std::shared_ptr<detail::value_base> value_;
  const std::string arg_help_;
};

/**
 * Specification of command line options.
 */
class options {
public:
  struct help_option_details {
    std::string s{};
    std::string l{};
    cxx_string desc{};
    std::string default_value{};
    std::string implicit_value{};
    std::string arg_help{};
    bool has_implicit{};
    bool has_default{};
    bool is_container{};
    bool is_boolean{};
  };

  struct help_group_details {
    std::string name{};
    std::string description{};
    std::vector<help_option_details> options{};
  };

  class option_adder {
  public:
    option_adder(const std::string group, options& options);

    option_adder&
    operator() (
      const std::string& opts,
      const std::string& desc,
      const std::shared_ptr<detail::value_base>& value
        = ::cxxopts::value<bool>(),
      const std::string arg_help = {});

  private:
    const std::string group_;
    options& options_;
  };

public:
  explicit options(std::string program, std::string help_string = {});

  options&
  positional_help(std::string help_text);

  options&
  custom_help(std::string help_text);

  options&
  show_positional_help();

  options&
  allow_unrecognised_options();

  options&
  set_tab_expansion(bool expansion = true);

  options&
  set_width(size_t width);

  /**
   * Stop parsing at first positional argument.
   */
  options&
  stop_on_positional();

  option_adder
  add_options(std::string group = {});

  /**
   * Adds list of options to the specific group.
   */
  void
  add_options(
    const std::string& group,
    std::initializer_list<option> options);

  /**
   * Adds an option to the specific group.
   */
  void
  add_option(
    const std::string& group,
    const option& option);

  template <typename ... Args>
  void
  parse_positional(Args&& ... args) {
    parse_positional(std::vector<std::string>{std::forward<Args>(args)...});
  }

  template <
    typename I,
    typename std::enable_if<
      !std::is_same<typename std::iterator_traits<I>::value_type, void>::value>::type>
  void
  parse_positional(const I begin, const I end) {
    parse_positional(std::vector<std::string>(begin, end));
  }

  void
  parse_positional(std::vector<std::string> options);

  std::string
  help(const std::vector<std::string>& groups = {}) const;

  /**
   * Returns list of the defined groups.
   */
  std::vector<std::string>
  groups() const;

  const help_group_details&
  group_help(const std::string& group) const;

public:
  /**
   * Parses the command line arguments according to the current specification.
   */
  parse_result
  parse(int argc, const char* const* argv) const;

private:
  void
  add_option(
    const std::string& group,
    const std::string& s,
    const std::string& l,
    std::string desc,
    const std::shared_ptr<detail::value_base>& value,
    std::string arg_help);

  void
  add_one_option(
    const std::string& option,
    const std::shared_ptr<option_details>& details);

  cxx_string
  format_option(const help_option_details& o) const;

  cxx_string
  format_description(
    const help_option_details& o,
    size_t start,
    size_t allowed,
    bool tab_expansion) const;

  cxx_string
  help_one_group(const std::string& group) const;

  void
  generate_group_help(
    cxx_string& result,
    const std::vector<std::string>& groups) const;

  void
  generate_all_groups_help(cxx_string& result) const;

private:
  class option_parser;

  using option_map =
    std::unordered_map<std::string, std::shared_ptr<option_details>>;
  using positional_list = std::vector<std::string>;

  const std::string program_;
  const cxx_string help_string_;
  std::string custom_help_{};
  std::string positional_help_;
  size_t width_{76};
  /// Allow consume unrecognized options
  /// instead of throwing an error.
  bool allow_unrecognised_{false};
  /// Show help for options bonded to positional arguments.
  bool show_positional_{false};
  /// Stop parsing at first positional argument.
  bool stop_on_positional_{false};
  /// Replace tab with spaces.
  bool tab_expansion_{false};

  /// Named options.
  /// Short and long names exist as separate entries but
  /// point to the same object.
  option_map options_{};
  /// List of named positional arguments.
  positional_list positional_{};
  std::unordered_set<std::string> positional_set_{};
  /// Mapping from groups to help options.
  std::map<std::string, help_group_details> help_{};
};

} // namespace cxxopts

#endif //CXXOPTS_HPP_INCLUDED
