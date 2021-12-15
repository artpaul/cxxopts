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

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <string.h>

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
static inline
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


argument_incorrect_type::argument_incorrect_type(
    const std::string& arg,
    const std::string& type
  )
  : parse_error(
    "Argument " + LQUOTE + arg + RQUOTE + " failed to parse" +
    (type.empty() ? std::string() : (": " + type + " expected")))
{
}


namespace detail {
namespace {

template <typename T, bool B>
struct signed_check;

template <typename T>
struct signed_check<T, true> {
  template <typename U>
  bool
  operator()(const U u, const bool negative) const noexcept {
    return ((static_cast<U>(std::numeric_limits<T>::min()) >= u) && negative) ||
            (static_cast<U>(std::numeric_limits<T>::max()) >= u);
  }
};

template <typename T>
struct signed_check<T, false> {
  template <typename U>
  bool
  operator()(const U, const bool) const noexcept {
    return true;
  }
};

template <typename T, typename U>
bool
check_signed_range(const U value, const bool negative) noexcept {
  return signed_check<T, std::numeric_limits<T>::is_signed>()(value, negative);
}

template <typename R, typename T>
bool
checked_negate(R& r, T&& t, std::true_type) noexcept {
  // if we got to here, then `t` is a positive number that fits into
  // `R`. So to avoid MSVC C4146, we first cast it to `R`.
  // See https://github.com/jarro2783/cxxopts/issues/62 for more details.
  r = static_cast<R>(-static_cast<R>(t-1)-1);
  return true;
}

template <typename R, typename T>
bool
checked_negate(R&, T&&, std::false_type) noexcept {
  return false;
}

bool
parse_uint64(const std::string& text, uint64_t& value, bool& negative) noexcept {
  const char* p = text.c_str();
  // String should not be empty.
  if (*p == 0) {
    return false;
  }
  // Parse sign.
  if (*p == '+') {
    ++p;
  } else if (*p == '-') {
    negative = true;
    ++p;
  }
  // Not an integer value.
  if (*p == 0) {
    return false;
  } else {
    value = 0;
  }
  // Hex number.
  if (*p == '0' && *(p + 1) == 'x') {
    p += 2;
    if (*p == 0) {
      return false;
    }
    for (; *p; ++p) {
      uint64_t digit = 0;

      if (*p >= '0' && *p <= '9') {
        digit = static_cast<uint64_t>(*p - '0');
      } else if (*p >= 'a' && *p <= 'f') {
        digit = static_cast<uint64_t>(*p - 'a' + 10);
      } else if (*p >= 'A' && *p <= 'F') {
        digit = static_cast<uint64_t>(*p - 'A' + 10);
      } else {
        return false;
      }

      const uint64_t next = value * 16u + digit;
      if (value > next) {
        return false;
      } else {
        value = next;
      }
    }
  // Decimal number.
  } else {
    for (; *p; ++p) {
      uint64_t digit = 0;

      if (*p >= '0' && *p <= '9') {
        digit = static_cast<uint64_t>(*p - '0');
      } else {
        return false;
      }

      const uint64_t next = value * 10u + digit;
      if (value > next) {
        return false;
      } else {
        value = next;
      }
    }
  }
  return true;
}

template <typename T>
void
integer_parser(const std::string& text, T& value) {
  using US = typename std::make_unsigned<T>::type;

  uint64_t u64_result{0};
  US result{0};
  bool negative{false};

  // Parse text to the uint64_t value.
  if (!parse_uint64(text, u64_result, negative)) {
    detail::throw_or_mimic<argument_incorrect_type>(text, "integer");
  }
  // Check unsigned overflow.
  if (u64_result > std::numeric_limits<US>::max()) {
    detail::throw_or_mimic<argument_incorrect_type>(text, "integer");
  } else {
    result = static_cast<US>(u64_result);
  }
  // Check signed overflow.
  if (!check_signed_range<T>(result, negative)) {
    detail::throw_or_mimic<argument_incorrect_type>(text, "integer");
  }
  // Negate value.
  if (negative) {
    if (!checked_negate<T>(value, result,
        std::integral_constant<bool, std::numeric_limits<T>::is_signed>()))
    {
      detail::throw_or_mimic<argument_incorrect_type>(text, "integer");
    }
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
  switch (text.size()) {
  case 1: {
    const char ch = text[0];
    if (ch == '1' || ch == 't' || ch == 'T') {
      value = true;
      return;
    }
    if (ch == '0' || ch == 'f' || ch == 'F') {
      value = false;
      return;
    }
    break;
  }
  case 4:
    if ((text[0] == 't' || text[0] == 'T') &&
        (text[1] == 'r' && text[2] == 'u' && text[3] == 'e'))
    {
      value = true;
      return;
    }
    break;
  case 5:
    if ((text[0] == 'f' || text[0] == 'F') &&
        (text[1] == 'a' && text[2] == 'l' && text[3] == 's' && text[4] == 'e'))
    {
      value = false;
      return;
    }
    break;
  }
  detail::throw_or_mimic<argument_incorrect_type>(text, "bool");
}

void
parse_value(const std::string& text, char& c) {
  if (text.length() != 1) {
    detail::throw_or_mimic<argument_incorrect_type>(text, "char");
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
    std::shared_ptr<detail::value_base> val
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

std::shared_ptr<detail::value_base>
option_details::value() const {
    return value_;
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
  const std::shared_ptr<option_details>& details,
  const std::string& text)
{
  ensure_value(details);
  ++count_;
  value_->parse(text);
  long_name_ = details->long_name();
}

void
option_value::parse_default(
  const std::shared_ptr<option_details>& details)
{
  ensure_value(details);
  default_ = true;
  long_name_ = details->long_name();
  value_->parse();
}

void
option_value::parse_no_value(
  const std::shared_ptr<option_details>& details)
{
  long_name_ = details->long_name();
}

size_t
option_value::count() const noexcept {
  return count_;
}

bool
option_value::has_default() const noexcept {
  return default_;
}

bool
option_value::has_value() const noexcept {
  return value_ != nullptr;
}

void
option_value::ensure_value(const std::shared_ptr<option_details>& details) {
  if (value_ == nullptr) {
    value_ = details->value();
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
    std::vector<std::string>&& unmatched_args,
    size_t consumed
  )
  : keys_(std::move(keys))
  , values_(std::move(values))
  , sequential_(std::move(sequential))
  , unmatched_(std::move(unmatched_args))
  , consumed_arguments_(consumed)
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
    detail::throw_or_mimic<option_not_present_error>(option);
  }

  const auto vi = values_.find(ki->second);
  if (vi == values_.end()) {
    detail::throw_or_mimic<option_not_present_error>(option);
  }

  return vi->second;
}

const std::vector<parse_result::key_value>&
parse_result::arguments() const {
  return sequential_;
}

size_t
parse_result::consumed() const {
  return consumed_arguments_;
}

const std::vector<std::string>&
parse_result::unmatched() const {
  return unmatched_;
}


class options::option_parser {
  using positional_list_iterator = positional_list::const_iterator;

  struct option_data {
    std::string name{};
    std::string value{};
    bool is_long{false};
    bool has_value{false};
  };

public:
  option_parser(
      const option_map& options,
      const positional_list& positional,
      bool allow_unrecognised,
      bool stop_on_positional
    )
    : options_(options)
    , positional_(positional)
    , allow_unrecognised_(allow_unrecognised)
    , stop_on_positional_(stop_on_positional)
  {
  }

  parse_result
  parse(const int argc, const char* const* argv) {
    int current = 1;
    auto next_positional = positional_.begin();
    std::vector<std::string> unmatched;

    while (current < argc) {
      if (strcmp(argv[current], "--") == 0) {
        // Skip dash-dash argument.
        ++current;
        if (stop_on_positional_) {
          break;
        }
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

      option_data result;

      if (!parse_argument(argv[current], result)) {
        // Not a flag.
        // But if it starts with a `-`, then it's an error.
        if (argv[current][0] == '-' && argv[current][1] != '\0') {
          if (!allow_unrecognised_) {
            detail::throw_or_mimic<option_syntax_error>(argv[current]);
          }
        }
        if (stop_on_positional_) {
          break;
        }
        // If true is returned here then it was consumed, otherwise it is
        // ignored.
        if (!consume_positional(argv[current], next_positional)) {
          unmatched.emplace_back(argv[current]);
        }
        // If we return from here then it was parsed successfully, so continue.
      } else {
        // Short or long option?
        if (result.is_long == false) {
          const std::string& seq = result.name;
          // Iterate over the sequence of short options.
          for (std::size_t i = 0; i != seq.size(); ++i) {
            const std::string name(1, seq[i]);
            const auto oi = options_.find(name);

            if (oi == options_.end()) {
              if (allow_unrecognised_) {
                unmatched.push_back(std::string("-") + seq[i]);
                continue;
              }
              // Error.
              detail::throw_or_mimic<option_not_exists_error>(name);
            }

            const auto& opt = oi->second;
            if (i + 1 == seq.size()) {
              // It must be the last argument.
              checked_parse_arg(argc, argv, current, opt, name);
            } else if (opt->value()->has_implicit()) {
              parse_option(opt, name, opt->value()->get_implicit_value());
            } else {
              parse_option(opt, name, seq.substr(i + 1));
              break;
            }
          }
        } else {
          const std::string& name = result.name;
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
            detail::throw_or_mimic<option_not_exists_error>(name);
          }

          const auto& opt = oi->second;
          // Equal sign provided for the long option?
          if (result.has_value) {
            // Parse the option given.
            parse_option(opt, name, result.value);
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

      if (value->has_default()) {
        if (!store.count() && !store.has_default()) {
          parse_default(detail);
        }
      } else {
        parse_no_value(detail);
      }

      if (value->has_env() && !store.count()){
        if (const char* env = std::getenv(value->get_env_var().c_str())) {
          store.parse(detail, std::string(env));
        }
      }
    }

    parse_result::name_hash_map keys;
    // Finalize aliases.
    for (const auto& option: options_) {
      const auto& detail = option.second;
      const auto hash = detail->hash();

      if (detail->short_name().size()) {
        keys[detail->short_name()] = hash;
      }
      if (detail->long_name().size()) {
        keys[detail->long_name()] = hash;
      }
    }

    assert(stop_on_positional_ || argc == current || argc == 0);

    return parse_result(
      std::move(keys),
      std::move(parsed_),
      std::move(sequential_),
      std::move(unmatched),
      current);
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
        detail::throw_or_mimic<option_not_exists_error>(*next);
      }
      if (oi->second->value()->is_container()) {
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

  bool
  is_dash_dash_or_option_name(const char* const arg) const {
      // The dash-dash symbol has a special meaning and cannot
      // be interpreted as an option value.
      if (strcmp(arg, "--") == 0) {
        return true;
      }

      option_data result;
      // The argument does not match an option format
      // so that it can be safely consumed as a value.
      if (!parse_argument(arg, result)) {
        return false;
      }

      auto check_name = [&] (const std::string& opt) {
        return options_.find(opt) != options_.end();
      };
      // Check that the argument does not match any
      // existing option.
      if (result.is_long) {
        return check_name(result.name);
      } else {
        return check_name(result.name.substr(0, 1));
      }

      return false;
  }

  void
  checked_parse_arg(
    const int argc,
    const char* const* argv,
    int& current,
    const std::shared_ptr<option_details>& value,
    const std::string& name)
  {
    auto parse_implicit = [&] () {
      if (value->value()->has_implicit()) {
        parse_option(value, name, value->value()->get_implicit_value());
      } else {
        detail::throw_or_mimic<missing_argument_error>(name);
      }
    };

    if (current + 1 == argc || value->value()->get_no_value()) {
      // Last argument or the option without value.
      parse_implicit();
    } else {
      const char* const arg = argv[current + 1];
      // Check that we do not silently consume any option as a value
      // of another option.
      if (arg[0] == '-' && is_dash_dash_or_option_name(arg)) {
        parse_implicit();
      } else {
        // Parse argument as a value for the option.
        parse_option(value, name, arg);
        ++current;
      }
    }
  }

  bool
  parse_argument(const std::string& text, option_data& data) const {
    const char* p = text.c_str();
    // String should not be empty and should starts with '-'.
    if (*p == 0 || *p != '-') {
      return false;
    } else {
      ++p;
    }
    // Long option starts with '--'.
    if (*p == '-') {
      ++p;
      if (isalnum(*p) && *(p + 1) != 0) {
        data.is_long = true;
        data.name += *p;
        ++p;
      } else {
        return false;
      }
      for (; *p; ++p) {
        if (*p == '=') {
          ++p;
          data.has_value = true;
          data.value.assign(p, text.c_str() + text.size());
          break;
        }
        if (*p == '-' || *p == '_' || isalnum(*p)) {
          data.name += *p;
        } else {
          return false;
        }
      }
      return data.name.size() > 1;
    // Short option.
    } else {
      // Single char short option.
      if (*(p + 1) == 0) {
        if (*p == '?' || isalnum(*p)) {
          data.name = *p;
          return true;
        }
        return false;
      }
      // Multiple short options.
      for (; *p; ++p) {
        if (isalnum(*p)) {
          data.name += *p;
        } else {
          return false;
        }
      }
      return true;
    }
    return false;
  }

  void
  parse_option(
    const std::shared_ptr<option_details>& value,
    const std::string&,
    const std::string& arg)
  {
    parsed_[value->hash()].parse(value, arg);
    sequential_.emplace_back(value->long_name(), arg);
  }

  void
  parse_default(const std::shared_ptr<option_details>& details) {
    parsed_[details->hash()].parse_default(details);
  }

  void
  parse_no_value(const std::shared_ptr<option_details>& details) {
    parsed_[details->hash()].parse_no_value(details);
  }

private:
  const option_map& options_;
  const positional_list& positional_;
  const bool allow_unrecognised_;
  const bool stop_on_positional_;

  std::vector<parse_result::key_value> sequential_{};
  parse_result::parsed_hash_map parsed_{};
};

options::options(std::string program, std::string help_string)
  : program_(std::move(program))
  , help_string_(to_local_string(std::move(help_string)))
  , custom_help_("[OPTION...]")
  , positional_help_("positional parameters")
{
}

options&
options::allow_unrecognised_options(const bool value) {
  allow_unrecognised_ = value;
  return *this;
}

options&
options::custom_help(std::string help_text) {
  custom_help_ = std::move(help_text);
  return *this;
}

options&
options::positional_help(std::string help_text) {
  positional_help_ = std::move(help_text);
  return *this;
}

options&
options::show_positional_help(const bool value) {
  show_positional_ = value;
  return *this;
}

options&
options::set_tab_expansion(bool expansion) {
  tab_expansion_ = expansion;
  return *this;
}

options&
options::set_width(size_t width) {
  width_ = width;
  return *this;
}

options&
options::stop_on_positional(const bool value) {
  stop_on_positional_ = value;
  return *this;
}

void
options::add_options(
  const std::string& group,
  std::initializer_list<option> opts)
{
  option_adder adder(group, *this);
  for (const auto& opt: opts) {
    adder(opt.opts_, opt.desc_, opt.value_, opt.arg_help_);
  }
}

void
options::parse_positional(std::vector<std::string> opts) {
  positional_ = std::move(opts);
  positional_set_ = std::unordered_set<std::string>(
    positional_.begin(), positional_.end()
  );
}

parse_result
options::parse(int argc, const char* const* argv) const {
  return option_parser(
    options_, positional_,
    allow_unrecognised_, stop_on_positional_).parse(argc, argv);
}

void
options::add_option(const std::string& group, const option& opt) {
    add_options(group, {opt});
}

void
options::add_option(
  const std::string& group,
  std::string s,
  std::string l,
  std::string desc,
  const std::shared_ptr<detail::value_base>& value,
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
  help_option_details help_opt;
  help_opt.s = s;
  help_opt.l = l;
  help_opt.desc = std::move(string_desc);
  help_opt.default_value = value->get_default_value();
  help_opt.implicit_value = value->get_implicit_value();
  help_opt.arg_help = std::move(arg_help);
  help_opt.has_implicit = value->has_implicit();
  help_opt.has_default = value->has_default();
  help_opt.is_container = value->is_container();
  help_opt.is_boolean = value->is_boolean();
  help_[group].options.push_back(std::move(help_opt));
}

void
options::add_one_option(
  const std::string& name,
  const std::shared_ptr<option_details>& details)
{
  const auto in = options_.emplace(name, details);

  if (!in.second) {
    detail::throw_or_mimic<option_exists_error>(name);
  }
}

cxx_string
options::format_option(const help_option_details& o) const {
  const auto& s = o.s;
  const auto& l = o.l;

  cxx_string result = "  ";

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
  using option_help = std::vector<std::pair<cxx_string, cxx_string>>;

  auto group = help_.find(g);
  if (group == help_.end()) {
    return cxx_string();
  }

  option_help format;
  size_t longest = 0;
  cxx_string result;

  if (!g.empty()) {
    result += to_local_string(g);
    result += '\n';
  }

  for (const auto& o : group->second.options) {
    if (!show_positional_ && positional_set_.find(o.l) != positional_set_.end())
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
    if (!show_positional_ && positional_set_.find(o.l) != positional_set_.end())
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

  if (!empty(help_string_)) {
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
  const std::shared_ptr<detail::value_base>& value,
  std::string arg_help)
{
  std::string s;
  std::string l;
  if (parse_option_specifier(opts, s, l)) {
    assert(s.empty() || s.size() == 1);
    assert(l.empty() || l.size() > 1);

    options_.add_option(
      group_,
      std::move(s),
      std::move(l),
      desc,
      value,
      std::move(arg_help));
  } else {
    detail::throw_or_mimic<invalid_option_format_error>(opts);
  }
  return *this;
}

bool
options::option_adder::parse_option_specifier(
  const std::string& text,
  std::string& s,
  std::string& l) const
{
  const char* p = text.c_str();
  if (*p == 0) {
    return false;
  } else {
    s.clear();
    l.clear();
  }
  // Short option.
  if (*(p + 1) == 0 || *(p + 1) == ',') {
    if (*p == '?' || isalnum(*p)) {
      s = *p;
      ++p;
    } else {
      return false;
    }
  }
  // Skip comma.
  if (*p == ',') {
    if (s.empty()) {
      return false;
    }
    ++p;
  }
  // Skip spaces.
  while (*p && *p == ' ') {
    ++p;
  }
  // Valid specifier without long option.
  if (*p == 0) {
    return true;
  } else {
    l.reserve((text.c_str() + text.size()) - p);
  }
  // First char of an option name should be alnum.
  if (isalnum(*p)) {
    l += *p;
    ++p;
  }
  for (; *p; ++p) {
    if (*p == '-' || *p == '_' || isalnum(*p)) {
      l += *p;
    } else {
      return false;
    }
  }
  return l.size() > 1;
}

} // namespace cxxopts
