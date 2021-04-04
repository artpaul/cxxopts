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

#include "cxxopts.hpp"

#include <cstdlib>
#include <regex>

//when we ask cxxopts to use Unicode, help strings are processed using ICU,
//which results in the correct lengths being computed for strings when they
//are formatted for the help output
//it is necessary to make sure that <unicode/unistr.h> can be found by the
//compiler, and that icu-uc is linked in to the binary.

#ifdef CXXOPTS_USE_UNICODE

namespace cxxopts {

static inline
cxx_string
to_local_string(std::string s) {
  return icu::UnicodeString::fromUTF8(std::move(s));
}

class unicode_string_iterator
  : public std::iterator<std::forward_iterator_tag, int32_t>
{
public:
  unicode_string_iterator(const icu::UnicodeString* string, int32_t pos)
    : s(string)
    , i(pos)
  {
  }

  value_type
  operator*() const {
    return s->char32At(i);
  }

  bool
  operator==(const unicode_string_iterator& rhs) const {
    return s == rhs.s && i == rhs.i;
  }

  bool
  operator!=(const unicode_string_iterator& rhs) const {
    return !(*this == rhs);
  }

  unicode_string_iterator&
  operator++() {
    ++i;
    return *this;
  }

  unicode_string_iterator
  operator+(int32_t v) {
    return unicode_string_iterator(s, i + v);
  }

private:
  const icu::UnicodeString* s;
  int32_t i;
};

static inline
cxx_string&
string_append(cxx_string&s, cxx_string a) {
  return s.append(std::move(a));
}

static inline
cxx_string&
string_append(cxx_string& s, size_t n, UChar32 c) {
  for (size_t i = 0; i != n; ++i) {
    s.append(c);
  }

  return s;
}

template <typename Iterator>
static inline
cxx_string&
string_append(cxx_string& s, Iterator begin, Iterator end) {
  while (begin != end) {
    s.append(*begin);
    ++begin;
  }

  return s;
}

static inline
size_t
string_length(const cxx_string& s) {
  return s.length();
}

static inline
std::string
to_utf8_string(const cxx_string& s) {
  std::string result;
  s.toUTF8String(result);

  return result;
}

static inline
bool
empty(const cxx_string& s) {
  return s.isEmpty();
}

} // namespace cxxopts

namespace std {

cxxopts::unicode_string_iterator
begin(const icu::UnicodeString& s) {
  return cxxopts::unicode_string_iterator(&s, 0);
}

cxxopts::unicode_string_iterator
end(const icu::UnicodeString& s) {
  return cxxopts::unicode_string_iterator(&s, s.length());
}

} // namespace std

//ifdef CXXOPTS_USE_UNICODE
#else

namespace cxxopts {

template <typename T>
static
T
to_local_string(T&& t) {
  return std::forward<T>(t);
}

static inline
size_t
string_length(const cxx_string& s) {
  return s.length();
}

static inline
cxx_string&
string_append(cxx_string&s, const cxx_string& a) {
  return s.append(a);
}

static inline
cxx_string&
string_append(cxx_string& s, size_t n, char c) {
  return s.append(n, c);
}

template <typename Iterator>
static inline
cxx_string&
string_append(cxx_string& s, Iterator begin, Iterator end) {
  return s.append(begin, end);
}

template <typename T>
static inline
std::string
to_utf8_string(T&& t) {
  return std::forward<T>(t);
}

static inline
bool
empty(const std::string& s) {
  return s.empty();
}

} // namespace cxxopts

//ifdef CXXOPTS_USE_UNICODE
#endif

namespace cxxopts {

#ifdef _WIN32
  static const std::string LQUOTE("\'");
  static const std::string RQUOTE("\'");
#else
  static const std::string LQUOTE("‘");
  static const std::string RQUOTE("’");
#endif

static constexpr size_t OPTION_LONGEST = 30;
static constexpr size_t OPTION_DESC_GAP = 2;
static constexpr size_t OPTION_PADDING = 2;

static const std::basic_regex<char> option_matcher
  ("--([[:alnum:]][-_[:alnum:]]+)(=(.*))?|-([[:alnum:]]+)");

static const std::basic_regex<char> option_specifier
    ("(([[:alnum:]]),)?[ ]*([[:alnum:]][-_[:alnum:]]*)?");

static const std::basic_regex<char> integer_pattern
  ("(-)?(0x)?([0-9a-zA-Z]+)|((0x)?0)");
static const std::basic_regex<char> truthy_pattern
  ("(t|T)(rue)?|1");
static const std::basic_regex<char> falsy_pattern
  ("(f|F)(alse)?|0");


option_error::option_error(const std::string& what_arg)
  : std::runtime_error(what_arg)
{
}


spec_error::spec_error(const std::string& what_arg)
  : option_error(what_arg)
{
}


parse_error::parse_error(const std::string& what_arg)
  : option_error(what_arg)
{
}


option_exists_error::option_exists_error(const std::string& option)
  : spec_error(
      "Option " + LQUOTE + option + RQUOTE + " already exists")
{
}


invalid_option_format_error::invalid_option_format_error(
    const std::string& format
  )
  : spec_error("Invalid option format " + LQUOTE + format + RQUOTE)
{
}


option_syntax_error::option_syntax_error(const std::string& text)
  : parse_error("Argument " + LQUOTE + text + RQUOTE +
    " starts with a - but has incorrect syntax")
{
}


option_not_exists_error::option_not_exists_error(const std::string& option)
  : parse_error("Option " + LQUOTE + option + RQUOTE + " does not exist")
{
}


missing_argument_error::missing_argument_error(const std::string& option)
  : parse_error(
    "Option " + LQUOTE + option + RQUOTE + " is missing an argument")
{
}


option_requires_argument_error::option_requires_argument_error(
    const std::string& option
  )
  : parse_error("Option " + LQUOTE + option + RQUOTE + " requires an argument")
{
}


option_not_has_argument_error::option_not_has_argument_error(
    const std::string& option,
    const std::string& arg
  )
  : parse_error(
      "Option " + LQUOTE + option + RQUOTE +
      " does not take an argument, but argument " +
      LQUOTE + arg + RQUOTE + " given")
{
}


option_not_present_error::option_not_present_error(
    const std::string& option
  )
  : parse_error("Option " + LQUOTE + option + RQUOTE + " not present")
{
}


option_has_no_value_error::option_has_no_value_error(
    const std::string& name
  )
  : option_error(
      name.empty()
        ? "Option has no value"
        : "Option " + LQUOTE + name + RQUOTE + " has no value"
    )
{
}


argument_incorrect_type::argument_incorrect_type(const std::string& arg)
  : parse_error(
    "Argument " + LQUOTE + arg + RQUOTE + " failed to parse")
{
}


namespace detail {
namespace {

template <typename T, bool B>
struct signed_check;

template <typename T>
struct signed_check<T, true> {
  template <typename U>
  void
  operator()(bool negative, U u, const std::string& text) {
    if (negative) {
      if (u > static_cast<U>((std::numeric_limits<T>::min)())) {
        throw_or_mimic<argument_incorrect_type>(text);
      }
    } else {
      if (u > static_cast<U>((std::numeric_limits<T>::max)())) {
        throw_or_mimic<argument_incorrect_type>(text);
      }
    }
  }
};

template <typename T>
struct signed_check<T, false> {
  template <typename U>
  void
  operator()(bool, U, const std::string&) const noexcept {}
};

template <typename T, typename U>
void
check_signed_range(bool negative, U value, const std::string& text) {
  signed_check<T, std::numeric_limits<T>::is_signed>()(negative, value, text);
}

template <typename R, typename T>
void
checked_negate(R& r, T&& t, const std::string&, std::true_type) {
  // if we got to here, then `t` is a positive number that fits into
  // `R`. So to avoid MSVC C4146, we first cast it to `R`.
  // See https://github.com/jarro2783/cxxopts/issues/62 for more details.
  r = static_cast<R>(-static_cast<R>(t-1)-1);
}

template <typename R, typename T>
void
checked_negate(R&, T&&, const std::string& text, std::false_type) {
  throw_or_mimic<argument_incorrect_type>(text);
}

template <typename T>
void
integer_parser(const std::string& text, T& value) {
  std::smatch match;
  std::regex_match(text, match, integer_pattern);

  if (match.length() == 0) {
    throw_or_mimic<argument_incorrect_type>(text);
  }

  if (match.length(4) > 0) {
    value = 0;
    return;
  }

  using US = typename std::make_unsigned<T>::type;

  constexpr bool is_signed = std::numeric_limits<T>::is_signed;
  const bool negative = match.length(1) > 0;
  const uint8_t base = match.length(2) > 0 ? 16 : 10;

  auto value_match = match[3];

  US result = 0;

  for (auto iter = value_match.first; iter != value_match.second; ++iter) {
    US digit = 0;

    if (*iter >= '0' && *iter <= '9') {
      digit = static_cast<US>(*iter - '0');
    } else if (base == 16 && *iter >= 'a' && *iter <= 'f') {
      digit = static_cast<US>(*iter - 'a' + 10);
    } else if (base == 16 && *iter >= 'A' && *iter <= 'F') {
      digit = static_cast<US>(*iter - 'A' + 10);
    } else {
      throw_or_mimic<argument_incorrect_type>(text);
    }

    const US next = static_cast<US>(result * base + digit);
    if (result > next) {
      throw_or_mimic<argument_incorrect_type>(text);
    }

    result = next;
  }

  check_signed_range<T>(negative, result, text);

  if (negative) {
    checked_negate<T>(
      value,
      result,
      text,
      std::integral_constant<bool, is_signed>());
  } else {
    value = static_cast<T>(result);
  }
}

} // namespace

void
parse_value(const std::string& text, uint8_t& value) {
  integer_parser(text, value);
}

void
parse_value(const std::string& text, int8_t& value) {
  integer_parser(text, value);
}

void
parse_value(const std::string& text, uint16_t& value) {
  integer_parser(text, value);
}

void
parse_value(const std::string& text, int16_t& value) {
  integer_parser(text, value);
}

void
parse_value(const std::string& text, uint32_t& value) {
  integer_parser(text, value);
}

void
parse_value(const std::string& text, int32_t& value) {
  integer_parser(text, value);
}

void
parse_value(const std::string& text, uint64_t& value) {
  integer_parser(text, value);
}

void
parse_value(const std::string& text, int64_t& value) {
  integer_parser(text, value);
}

void
parse_value(const std::string& text, bool& value) {
  std::smatch result;
  std::regex_match(text, result, truthy_pattern);

  if (!result.empty()) {
    value = true;
    return;
  }

  std::regex_match(text, result, falsy_pattern);
  if (!result.empty()) {
    value = false;
    return;
  }

  throw_or_mimic<argument_incorrect_type>(text);
}

void parse_value(const std::string& text, char& c) {
  if (text.length() != 1) {
    throw_or_mimic<argument_incorrect_type>(text);
  }

  c = text[0];
}

void
parse_value(const std::string& text, std::string& value) {
  value = text;
}

} // namespace detail


option_details::option_details(
    std::string short_name,
    std::string long_name,
    cxx_string desc,
    std::shared_ptr<const detail::value_base> val
  )
  : short_(std::move(short_name))
  , long_(std::move(long_name))
  , desc_(std::move(desc))
  , hash_(std::hash<std::string>{}(long_ + short_))
  , value_(std::move(val))
{
}

const cxx_string&
option_details::description() const {
  return desc_;
}

const detail::value_base&
option_details::value() const {
    return *value_;
}

std::shared_ptr<detail::value_base>
option_details::make_storage() const {
  return value_->clone();
}

const std::string&
option_details::short_name() const {
  return short_;
}

const std::string&
option_details::long_name() const {
  return long_;
}

size_t
option_details::hash() const noexcept {
  return hash_;
}


void
option_value::parse(
  const std::shared_ptr<const option_details>& details,
  const std::string& text)
{
  ensure_value(details);
  ++count_;
  value_->parse(text);
  long_name_ = &details->long_name();
}

void
option_value::parse_default(
  const std::shared_ptr<const option_details>& details)
{
  ensure_value(details);
  default_ = true;
  long_name_ = &details->long_name();
  value_->parse();
}

void
option_value::parse_no_value(
  const std::shared_ptr<const option_details>& details)
{
  long_name_ = &details->long_name();
}

#if defined(__GNUC__)
#if __GNUC__ <= 10 && __GNUC_MINOR__ <= 1
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Werror=null-dereference"
#endif
#endif

size_t
option_value::count() const noexcept {
  return count_;
}

#if defined(__GNUC__)
#if __GNUC__ <= 10 && __GNUC_MINOR__ <= 1
#pragma GCC diagnostic pop
#endif
#endif

bool
option_value::has_default() const noexcept {
  return default_;
}

bool
option_value::has_value() const noexcept {
  return value_ != nullptr;
}

void
option_value::ensure_value(const std::shared_ptr<const option_details>& details) {
  if (value_ == nullptr) {
    value_ = details->make_storage();
  }
}


parse_result::key_value::key_value(std::string key, std::string value)
  : key_(std::move(key))
  , value_(std::move(value))
{
}

const std::string&
parse_result::key_value::key() const {
  return key_;
}

const std::string&
parse_result::key_value::value() const {
  return value_;
}


parse_result::parse_result(
    name_hash_map&& keys,
    parsed_hash_map&& values,
    std::vector<key_value>&& sequential,
    std::vector<std::string>&& unmatched_args
  )
  : keys_(std::move(keys))
  , values_(std::move(values))
  , sequential_(std::move(sequential))
  , unmatched_(std::move(unmatched_args))
{
}

size_t
parse_result::count(const std::string& o) const {
  const auto ki = keys_.find(o);
  if (ki == keys_.end()) {
    return 0;
  }

  const auto vi = values_.find(ki->second);
  if (vi == values_.end()) {
    return 0;
  }

  return vi->second.count();
}

bool
parse_result::has(const std::string& o) const {
  return count(o) != 0;
}

const option_value&
parse_result::operator[](const std::string& option) const {
  const auto ki = keys_.find(option);
  if (ki == keys_.end()) {
    throw_or_mimic<option_not_present_error>(option);
  }

  const auto vi = values_.find(ki->second);
  if (vi == values_.end()) {
    throw_or_mimic<option_not_present_error>(option);
  }

  return vi->second;
}

const std::vector<parse_result::key_value>&
parse_result::arguments() const {
  return sequential_;
}

const std::vector<std::string>&
parse_result::unmatched() const {
  return unmatched_;
}


option::option(
    std::string opts,
    std::string desc,
    std::shared_ptr<const detail::value_base> value,
    std::string arg_help
  )
  : opts_(std::move(opts))
  , desc_(std::move(desc))
  , value_(std::move(value))
  , arg_help_(std::move(arg_help))
{
}

class options::option_parser {
  using positional_list_iterator = positional_list::const_iterator;

public:
  option_parser(
      const option_map& options,
      const positional_list& positional,
      bool allow_unrecognised
    )
    : options_(options)
    , positional_(positional)
    , allow_unrecognised_(allow_unrecognised)
  {
  }

  parse_result
  parse(int argc, const char* const* argv) {
    int current = 1;
    auto next_positional = positional_.begin();
    parse_result::name_hash_map keys;
    std::vector<std::string> unmatched;

    while (current != argc) {
      if (strcmp(argv[current], "--") == 0) {
        // Skip dash-dash argument.
        ++current;
        // Try to consume all remaining arguments as positional.
        for (; current < argc; ++current) {
          if (!consume_positional(argv[current], next_positional)) {
            break;
          }
        }
        // Adjust argv for any that couldn't be swallowed.
        for (; current != argc; ++current) {
          unmatched.emplace_back(argv[current]);
        }
        break;
      }

      std::cmatch result;
      std::regex_match(argv[current], result, option_matcher);

      if (result.empty()) {
        //not a flag

        // But if it starts with a `-`, then it's an error.
        if (argv[current][0] == '-' && argv[current][1] != '\0') {
          if (!allow_unrecognised_) {
            throw_or_mimic<option_syntax_error>(argv[current]);
          }
        }

        // If true is returned here then it was consumed, otherwise it is
        // ignored.
        if (!consume_positional(argv[current], next_positional)) {
          unmatched.emplace_back(argv[current]);
        }
        // If we return from here then it was parsed successfully, so continue.
      } else {
        // Short or long option?
        if (result[4].length() != 0) {
          const std::string& seq = result[4];
          // Iterate over the sequence of short options.
          for (std::size_t i = 0; i != seq.size(); ++i) {
            const std::string name(1, seq[i]);
            const auto oi = options_.find(name);

            if (oi == options_.end()) {
              if (allow_unrecognised_) {
                continue;
              }
              // Error.
              throw_or_mimic<option_not_exists_error>(name);
            }

            const auto& opt = oi->second;
            if (i + 1 == seq.size()) {
              // It must be the last argument.
              checked_parse_arg(argc, argv, current, opt, name);
            } else if (opt->value().has_implicit()) {
              parse_option(opt, name, opt->value().get_implicit_value());
            } else {
              // Error.
              throw_or_mimic<option_requires_argument_error>(name);
            }
          }
        } else if (result[1].length() != 0) {
          const std::string& name = result[1];
          const auto oi = options_.find(name);

          if (oi == options_.end()) {
            if (allow_unrecognised_) {
              // Keep unrecognised options in argument list,
              // skip to next argument.
              unmatched.emplace_back(argv[current]);
              ++current;
              continue;
            }
            // Error.
            throw_or_mimic<option_not_exists_error>(name);
          }

          const auto& opt = oi->second;
          // Equal sign provided for the long option?
          if (result[2].length() != 0) {
            // Parse the option given.
            parse_option(opt, name, result[3]);
          } else {
            // Parse the next argument.
            checked_parse_arg(argc, argv, current, opt, name);
          }
        }
      }

      ++current;
    }

    for (auto& opt : options_) {
      auto& detail = opt.second;
      const auto& value = detail->value();

      auto& store = parsed_[detail->hash()];

      if (value.has_default()) {
        if (!store.count() && !store.has_default()) {
          parse_default(detail);
        }
      } else {
        parse_no_value(detail);
      }

      if (value.has_env() && !store.count()){
        if (const char* env = std::getenv(value.get_env_var().c_str())) {
          store.parse(detail, std::string(env));
        }
      }
    }

    // Finalize aliases.
    for (auto& option: options_) {
      const auto& detail = *option.second;
      const auto hash = detail.hash();
      keys[detail.short_name()] = hash;
      keys[detail.long_name()] = hash;

      parsed_.emplace(hash, option_value());
    }

    return parse_result(
      std::move(keys),
      std::move(parsed_),
      std::move(sequential_),
      std::move(unmatched));
  }

private:
  void
  add_to_option(
    option_map::const_iterator iter,
    const std::string& option,
    const std::string& arg)
  {
    parse_option(iter->second, option, arg);
  }

  bool
  consume_positional(const std::string& a, positional_list_iterator& next) {
    for (; next != positional_.end(); ++next) {
      const auto oi = options_.find(*next);
      if (oi == options_.end()) {
        throw_or_mimic<option_not_exists_error>(*next);
      }
      if (oi->second->value().is_container()) {
        add_to_option(oi, *next, a);
        return true;
      }
      if (parsed_[oi->second->hash()].count() == 0) {
        add_to_option(oi, *next, a);
        ++next;
        return true;
      }
    }
    return false;
  }

  void
  checked_parse_arg(
    int argc,
    const char* const* argv,
    int& current,
    const std::shared_ptr<option_details>& value,
    const std::string& name)
  {
    if (current + 1 >= argc) {
      if (value->value().has_implicit()) {
        parse_option(value, name, value->value().get_implicit_value());
      } else {
        throw_or_mimic<missing_argument_error>(name);
      }
    } else {
      if (value->value().has_implicit()) {
        parse_option(value, name, value->value().get_implicit_value());
      } else {
        parse_option(value, name, argv[current + 1]);
        ++current;
      }
    }
  }

  void
  parse_option(
    const std::shared_ptr<option_details>& value,
    const std::string&,
    const std::string& arg = {})
  {
    parsed_[value->hash()].parse(value, arg);
    sequential_.emplace_back(value->long_name(), arg);
  }

  void
  parse_default(const std::shared_ptr<option_details>& details) {
    // TODO: remove the duplicate code here
    parsed_[details->hash()].parse_default(details);
  }

  void
  parse_no_value(const std::shared_ptr<option_details>& details) {
    parsed_[details->hash()].parse_no_value(details);
  }

private:
  const option_map& options_;
  const positional_list& positional_;
  const bool allow_unrecognised_{};

  std::vector<parse_result::key_value> sequential_;
  parse_result::parsed_hash_map parsed_;
};

options::options(std::string program, std::string help_string)
  : program_(std::move(program))
  , help_string_(to_local_string(std::move(help_string)))
  , custom_help_("[OPTION...]")
  , positional_help_("positional parameters")
  , width_(76)
  , show_positional_(false)
  , allow_unrecognised_(false)
  , tab_expansion_(false)
{
}

options&
options::positional_help(std::string help_text) {
  positional_help_ = std::move(help_text);
  return *this;
}

options&
options::custom_help(std::string help_text) {
  custom_help_ = std::move(help_text);
  return *this;
}

options&
options::show_positional_help() {
  show_positional_ = true;
  return *this;
}

options&
options::allow_unrecognised_options() {
  allow_unrecognised_ = true;
  return *this;
}

options&
options::set_width(size_t width) {
  width_ = width;
  return *this;
}

options&
options::set_tab_expansion(bool expansion) {
  tab_expansion_ = expansion;
  return *this;
}

void
options::add_options(
  const std::string& group,
  std::initializer_list<option> options)
{
  option_adder adder(group, *this);
  for (const auto& opt: options) {
    adder(opt.opts_, opt.desc_, opt.value_, opt.arg_help_);
  }
}

void
options::parse_positional(std::vector<std::string> options) {
  positional_ = std::move(options);
  positional_set_ = std::unordered_set<std::string>(
    positional_.begin(), positional_.end()
  );
}

parse_result
options::parse(int argc, const char* const* argv) const {
  return option_parser(options_, positional_, allow_unrecognised_)
    .parse(argc, argv);
}

void
options::add_option(const std::string& group, const option& opt) {
    add_options(group, {opt});
}

void
options::add_option(
  const std::string& group,
  const std::string& s,
  const std::string& l,
  std::string desc,
  const std::shared_ptr<const detail::value_base>& value,
  std::string arg_help)
{
  auto string_desc = to_local_string(std::move(desc));
  auto details = std::make_shared<option_details>(s, l, string_desc, value);

  if (!s.empty()) {
    add_one_option(s, details);
  }
  if (!l.empty()) {
    add_one_option(l, details);
  }

  // Add the help details.
  auto& options = help_[group];

  options.options.emplace_back(help_option_details{s, l, string_desc,
      value->get_default_value(), value->get_implicit_value(),
      std::move(arg_help),
      value->has_implicit(), value->has_default(),
      value->is_container(), value->is_boolean()});
}

void
options::add_one_option(
  const std::string& name,
  const std::shared_ptr<option_details>& details)
{
  const auto in = options_.emplace(name, details);

  if (!in.second) {
    throw_or_mimic<option_exists_error>(name);
  }
}

cxx_string
options::format_option(const help_option_details& o) const {
  const auto& s = o.s;
  const auto& l = o.l;

  cxx_string result(OPTION_PADDING, ' ');

  if (!s.empty()) {
    result += "-" + to_local_string(s);
    if (!l.empty()) {
      result += ",";
    }
  } else {
    result += "   ";
  }

  if (!l.empty()) {
    result += " --" + to_local_string(l);
  }

  auto arg = !o.arg_help.empty() ? to_local_string(o.arg_help) : "arg";

  if (!o.is_boolean) {
    if (o.has_implicit) {
      result += " [=" + arg + "(=" + to_local_string(o.implicit_value) + ")]";
    } else {
      result += " " + arg;
    }
  }

  return result;
}

cxx_string
options::format_description(
  const help_option_details& o,
  size_t start,
  size_t allowed,
  bool tab_expansion) const
{
  auto desc = o.desc;

  if (o.has_default && (!o.is_boolean || o.default_value != "false")) {
    if(!o.default_value.empty()) {
      desc += to_local_string(" (default: " + o.default_value + ")");
    } else {
      desc += to_local_string(" (default: \"\")");
    }
  }

  cxx_string result;

  if (tab_expansion) {
    cxx_string desc2;
    size_t size = 0;
    for (auto c = std::begin(desc); c != std::end(desc); ++c) {
      if (*c == '\n') {
        desc2 += *c;
        size = 0;
      } else if (*c == '\t') {
        auto skip = 8 - size % 8;
        string_append(desc2, skip, ' ');
        size += skip;
      } else {
        desc2 += *c;
        ++size;
      }
    }
    desc = desc2;
  }

  desc += " ";

  auto current = std::begin(desc);
  auto previous = current;
  auto startLine = current;
  auto lastSpace = current;

  auto size = size_t{};

  bool appendNewLine;
  bool onlyWhiteSpace = true;

  while (current != std::end(desc)) {
    appendNewLine = false;

    if (std::isblank(*previous)) {
      lastSpace = current;
    }

    if (!std::isblank(*current)) {
      onlyWhiteSpace = false;
    }

    while (*current == '\n') {
      previous = current;
      ++current;
      appendNewLine = true;
    }

    if (!appendNewLine && size >= allowed) {
      if (lastSpace != startLine) {
        current = lastSpace;
        previous = current;
      }
      appendNewLine = true;
    }

    if (appendNewLine) {
      string_append(result, startLine, current);
      startLine = current;
      lastSpace = current;

      if (*previous != '\n') {
        string_append(result, "\n");
      }

      string_append(result, start, ' ');

      if (*previous != '\n') {
        string_append(result, lastSpace, current);
      }

      onlyWhiteSpace = true;
      size = 0;
    }

    previous = current;
    ++current;
    ++size;
  }

  //append whatever is left but ignore whitespace
  if (!onlyWhiteSpace) {
    string_append(result, startLine, previous);
  }

  return result;
}

cxx_string
options::help_one_group(const std::string& g) const {
  using OptionHelp = std::vector<std::pair<cxx_string, cxx_string>>;

  auto group = help_.find(g);
  if (group == help_.end()) {
    return "";
  }

  OptionHelp format;
  size_t longest = 0;
  cxx_string result;

  if (!g.empty()) {
    result += to_local_string(g);
    result += '\n';
  }

  for (const auto& o : group->second.options) {
    if (positional_set_.find(o.l) != positional_set_.end() &&
        !show_positional_)
    {
      continue;
    }

    auto s = format_option(o);
    longest = std::max(longest, string_length(s));
    format.push_back(std::make_pair(s, cxx_string()));
  }
  longest = std::min(longest, OPTION_LONGEST);

  //widest allowed description -- min 10 chars for helptext/line
  size_t allowed = 10;
  if (width_ > allowed + longest + OPTION_DESC_GAP) {
    allowed = width_ - longest - OPTION_DESC_GAP;
  }

  auto fiter = format.begin();
  for (const auto& o : group->second.options) {
    if (positional_set_.find(o.l) != positional_set_.end() &&
        !show_positional_)
    {
      continue;
    }

    auto d = format_description(
      o, longest + OPTION_DESC_GAP,
      allowed,
      tab_expansion_);

    result += fiter->first;
    if (string_length(fiter->first) > longest) {
      result += '\n';
      result += to_local_string(std::string(longest + OPTION_DESC_GAP, ' '));
    }
    else {
      result += to_local_string(std::string(longest + OPTION_DESC_GAP -
        string_length(fiter->first),
        ' '));
    }
    result += d;
    result += '\n';

    ++fiter;
  }

  return result;
}

void
options::generate_group_help(
  cxx_string& result,
  const std::vector<std::string>& print_groups) const
{
  for (size_t i = 0; i != print_groups.size(); ++i) {
    const cxx_string& group_help_text = help_one_group(print_groups[i]);
    if (empty(group_help_text)) {
      continue;
    }
    result += group_help_text;
    if (i < print_groups.size() - 1) {
      result += '\n';
    }
  }
}

void
options::generate_all_groups_help(cxx_string& result) const {
  generate_group_help(result, groups());
}

std::string
options::help(const std::vector<std::string>& help_groups) const {
  cxx_string result;

  if (!help_string_.empty()) {
    result += help_string_;
    result += '\n';
  }

  result += "usage: ";
  result += to_local_string(program_);
  result += " ";
  result += to_local_string(custom_help_);

  if (!positional_.empty() && !positional_help_.empty()) {
    result += " ";
    result += to_local_string(positional_help_);
  }

  result += "\n\n";

  if (help_groups.empty()) {
    generate_all_groups_help(result);
  } else {
    generate_group_help(result, help_groups);
  }

  return to_utf8_string(result);
}

std::vector<std::string>
options::groups() const {
  std::vector<std::string> names;

  std::transform(help_.begin(), help_.end(), std::back_inserter(names),
    [] (const std::map<std::string, help_group_details>::value_type& pair) {
      return pair.first;
    }
  );

  return names;
}

const options::help_group_details&
options::group_help(const std::string& group) const {
  return help_.at(group);
}

options::option_adder
options::add_options(std::string group)
{
  return option_adder(std::move(group), *this);
}


options::option_adder::option_adder(std::string group, options& options)
  : group_(std::move(group))
  , options_(options)
{
}

options::option_adder&
options::option_adder::operator()(
  const std::string& opts,
  const std::string& desc,
  const std::shared_ptr<const detail::value_base>& value,
  std::string arg_help)
{
  std::match_results<const char*> result;
  std::regex_match(opts.c_str(), result, option_specifier);

  if (result.empty()) {
    throw_or_mimic<invalid_option_format_error>(opts);
  }

  const auto& short_match = result[2];
  const auto& long_match = result[3];

  if (!short_match.length() && !long_match.length()) {
    throw_or_mimic<invalid_option_format_error>(opts);
  } else if (long_match.length() == 1 && short_match.length()) {
    throw_or_mimic<invalid_option_format_error>(opts);
  }

  auto option_names = [] (
    const std::sub_match<const char*>& short_,
    const std::sub_match<const char*>& long_) {
    if (long_.length() == 1) {
      return std::make_tuple(long_.str(), short_.str());
    }
    return std::make_tuple(short_.str(), long_.str());
  } (short_match, long_match);

  options_.add_option(
    group_,
    std::get<0>(option_names),
    std::get<1>(option_names),
    desc,
    value,
    std::move(arg_help));

  return *this;
}

} // namespace cxxopts
