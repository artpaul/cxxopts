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
String
toLocalString(std::string s) {
  return icu::UnicodeString::fromUTF8(std::move(s));
}

class UnicodeStringIterator
  : public std::iterator<std::forward_iterator_tag, int32_t>
{
public:
  UnicodeStringIterator(const icu::UnicodeString* string, int32_t pos)
    : s(string)
    , i(pos)
  {
  }

  value_type
  operator*() const {
    return s->char32At(i);
  }

  bool
  operator==(const UnicodeStringIterator& rhs) const {
    return s == rhs.s && i == rhs.i;
  }

  bool
  operator!=(const UnicodeStringIterator& rhs) const {
    return !(*this == rhs);
  }

  UnicodeStringIterator&
  operator++() {
    ++i;
    return *this;
  }

  UnicodeStringIterator
  operator+(int32_t v) {
    return UnicodeStringIterator(s, i + v);
  }

  private:
  const icu::UnicodeString* s;
  int32_t i;
};

static inline
String&
stringAppend(String&s, String a) {
  return s.append(std::move(a));
}

static inline
String&
stringAppend(String& s, size_t n, UChar32 c) {
  for (size_t i = 0; i != n; ++i) {
    s.append(c);
  }

  return s;
}

template <typename Iterator>
static inline
String&
stringAppend(String& s, Iterator begin, Iterator end) {
  while (begin != end) {
    s.append(*begin);
    ++begin;
  }

  return s;
}

static inline
size_t
stringLength(const String& s) {
  return s.length();
}

static inline
std::string
toUTF8String(const String& s) {
  std::string result;
  s.toUTF8String(result);

  return result;
}

static inline
bool
empty(const String& s) {
  return s.isEmpty();
}

} // namespace cxxopts

namespace std {

cxxopts::UnicodeStringIterator
begin(const icu::UnicodeString& s) {
  return cxxopts::UnicodeStringIterator(&s, 0);
}

cxxopts::UnicodeStringIterator
end(const icu::UnicodeString& s) {
  return cxxopts::UnicodeStringIterator(&s, s.length());
}

} // namespace std

//ifdef CXXOPTS_USE_UNICODE
#else

namespace cxxopts {

template <typename T>
static
T
toLocalString(T&& t) {
  return std::forward<T>(t);
}

static inline
size_t
stringLength(const String& s) {
  return s.length();
}

static inline
String&
stringAppend(String&s, const String& a) {
  return s.append(a);
}

static inline
String&
stringAppend(String& s, size_t n, char c) {
  return s.append(n, c);
}

template <typename Iterator>
static inline
String&
stringAppend(String& s, Iterator begin, Iterator end) {
  return s.append(begin, end);
}

template <typename T>
static inline
std::string
toUTF8String(T&& t) {
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
namespace {

#ifdef _WIN32
  const std::string LQUOTE("\'");
  const std::string RQUOTE("\'");
#else
  const std::string LQUOTE("‘");
  const std::string RQUOTE("’");
#endif

constexpr size_t OPTION_LONGEST = 30;
constexpr size_t OPTION_DESC_GAP = 2;
constexpr size_t OPTION_PADDING = 2;

const std::basic_regex<char> option_matcher
  ("--([[:alnum:]][-_[:alnum:]]+)(=(.*))?|-([[:alnum:]]+)");

const std::basic_regex<char> option_specifier
    ("(([[:alnum:]]),)?[ ]*([[:alnum:]][-_[:alnum:]]*)?");

const std::basic_regex<char> integer_pattern
  ("(-)?(0x)?([0-9a-zA-Z]+)|((0x)?0)");
const std::basic_regex<char> truthy_pattern
  ("(t|T)(rue)?|1");
const std::basic_regex<char> falsy_pattern
  ("(f|F)(alse)?|0");

String
format_option(const HelpOptionDetails& o) {
  const auto& s = o.s;
  const auto& l = o.l;

  String result(OPTION_PADDING, ' ');

  if (!s.empty()) {
    result += "-" + toLocalString(s);
    if (!l.empty()) {
      result += ",";
    }
  } else {
    result += "   ";
  }

  if (!l.empty()) {
    result += " --" + toLocalString(l);
  }

  auto arg = !o.arg_help.empty() ? toLocalString(o.arg_help) : "arg";

  if (!o.is_boolean) {
    if (o.has_implicit) {
      result += " [=" + arg + "(=" + toLocalString(o.implicit_value) + ")]";
    } else {
      result += " " + arg;
    }
  }

  return result;
}

String
format_description(
  const HelpOptionDetails& o,
  size_t start,
  size_t allowed,
  bool tab_expansion)
{
  auto desc = o.desc;

  if (o.has_default && (!o.is_boolean || o.default_value != "false")) {
    if(!o.default_value.empty()) {
      desc += toLocalString(" (default: " + o.default_value + ")");
    } else {
      desc += toLocalString(" (default: \"\")");
    }
  }

  String result;

  if (tab_expansion) {
    String desc2;
    size_t size = 0;
    for (auto c = std::begin(desc); c != std::end(desc); ++c) {
      if (*c == '\n') {
        desc2 += *c;
        size = 0;
      } else if (*c == '\t') {
        auto skip = 8 - size % 8;
        stringAppend(desc2, skip, ' ');
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
      stringAppend(result, startLine, current);
      startLine = current;
      lastSpace = current;

      if (*previous != '\n') {
        stringAppend(result, "\n");
      }

      stringAppend(result, start, ' ');

      if (*previous != '\n') {
        stringAppend(result, lastSpace, current);
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
    stringAppend(result, startLine, previous);
  }

  return result;
}

} // namespace

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
    const std::string& option
  )
  : option_error(
      option.empty()
        ? "Option has no value"
        : "Option " + LQUOTE + option + RQUOTE + " has no value"
    )
{
}


argument_incorrect_type::argument_incorrect_type(const std::string& arg)
  : parse_error(
    "Argument " + LQUOTE + arg + RQUOTE + " failed to parse")
{
}


namespace values {
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

} // namespace values


option_details::option_details(
    std::string short_name,
    std::string long_name,
    String desc,
    std::shared_ptr<const value_base> val
  )
  : short_(std::move(short_name))
  , long_(std::move(long_name))
  , desc_(std::move(desc))
  , value_(std::move(val))
  , count_(0)
{
  hash_ = std::hash<std::string>{}(long_ + short_);
}

option_details::option_details(const option_details& rhs)
  : desc_(rhs.desc_)
  , value_(rhs.value_->clone())
  , count_(rhs.count_)
  , hash_(0)
{
}

const String&
option_details::description() const {
  return desc_;
}

const value_base&
option_details::value() const {
    return *value_;
}

std::shared_ptr<value_base>
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
    NameHashMap&& keys,
    ParsedHashMap&& values,
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
    std::shared_ptr<const value_base> value,
    std::string arg_help
  )
  : opts_(std::move(opts))
  , desc_(std::move(desc))
  , value_(std::move(value))
  , arg_help_(std::move(arg_help))
{
}


namespace {

using PositionalListIterator = PositionalList::const_iterator;

class OptionParser {
public:
  OptionParser(
    const OptionMap& options,
    const PositionalList& positional,
    bool allow_unrecognised);

  parse_result
  parse(int argc, const char* const* argv);

  bool
  consume_positional(const std::string& a, PositionalListIterator& next);

  void
  checked_parse_arg(
    int argc,
    const char* const* argv,
    int& current,
    const std::shared_ptr<option_details>& value,
    const std::string& name);

  void
  add_to_option(
    OptionMap::const_iterator iter,
    const std::string& option,
    const std::string& arg);

  void
  parse_option(
    const std::shared_ptr<option_details>& value,
    const std::string& name,
    const std::string& arg = {});

  void
  parse_default(const std::shared_ptr<option_details>& details);

  void
  parse_no_value(const std::shared_ptr<option_details>& details);

private:
  const OptionMap& options_;
  const PositionalList& positional_;
  const bool allow_unrecognised_{};

  std::vector<parse_result::key_value> sequential_;
  ParsedHashMap parsed_;
};

OptionParser::OptionParser(
    const OptionMap& options,
    const PositionalList& positional,
    const bool allow_unrecognised
  )
  : options_(options)
  , positional_(positional)
  , allow_unrecognised_(allow_unrecognised)
{
}

void
OptionParser::parse_default(const std::shared_ptr<option_details>& details) {
  // TODO: remove the duplicate code here
  parsed_[details->hash()].parse_default(details);
}

void
OptionParser::parse_no_value(const std::shared_ptr<option_details>& details) {
  parsed_[details->hash()].parse_no_value(details);
}

void
OptionParser::parse_option(
  const std::shared_ptr<option_details>& value,
  const std::string& /*name*/,
  const std::string& arg)
{
  parsed_[value->hash()].parse(value, arg);
  sequential_.emplace_back(value->long_name(), arg);
}

void
OptionParser::checked_parse_arg(
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
OptionParser::add_to_option(
  OptionMap::const_iterator iter,
  const std::string& option,
  const std::string& arg)
{
  parse_option(iter->second, option, arg);
}

bool
OptionParser::consume_positional(
  const std::string& a,
  PositionalListIterator& next)
{
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

parse_result
OptionParser::parse(int argc, const char* const* argv) {
  int current = 1;
  auto next_positional = positional_.begin();
  NameHashMap keys;
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

} // namespace


Options::Options(std::string program, std::string help_string)
  : program_(std::move(program))
  , help_string_(toLocalString(std::move(help_string)))
  , custom_help_("[OPTION...]")
  , positional_help_("positional parameters")
  , width_(76)
  , show_positional_(false)
  , allow_unrecognised_(false)
  , tab_expansion_(false)
{
}

Options&
Options::positional_help(std::string help_text) {
  positional_help_ = std::move(help_text);
  return *this;
}

Options&
Options::custom_help(std::string help_text) {
  custom_help_ = std::move(help_text);
  return *this;
}

Options&
Options::show_positional_help() {
  show_positional_ = true;
  return *this;
}

Options&
Options::allow_unrecognised_options() {
  allow_unrecognised_ = true;
  return *this;
}

Options&
Options::set_width(size_t width) {
  width_ = width;
  return *this;
}

Options&
Options::set_tab_expansion(bool expansion) {
  tab_expansion_ = expansion;
  return *this;
}

void
Options::add_options(
  const std::string& group,
  std::initializer_list<option> options)
{
  OptionAdder option_adder(group, *this);
  for (const auto& opt: options) {
    option_adder(opt.opts_, opt.desc_, opt.value_, opt.arg_help_);
  }
}

void
Options::parse_positional(std::string option) {
  parse_positional(std::vector<std::string>{std::move(option)});
}

void
Options::parse_positional(std::vector<std::string> options) {
  positional_ = std::move(options);

  positional_set_.insert(positional_.begin(), positional_.end());
}

void
Options::parse_positional(std::initializer_list<std::string> options) {
  parse_positional(std::vector<std::string>(std::move(options)));
}

parse_result
Options::parse(int argc, const char* const* argv) {
  return OptionParser(options_, positional_, allow_unrecognised_)
    .parse(argc, argv);
}

void
Options::add_option(const std::string& group, const option& opt) {
    add_options(group, {opt});
}

void
Options::add_option(
  const std::string& group,
  const std::string& s,
  const std::string& l,
  std::string desc,
  const std::shared_ptr<const value_base>& value,
  std::string arg_help)
{
  auto string_desc = toLocalString(std::move(desc));
  auto details = std::make_shared<option_details>(s, l, string_desc, value);

  if (!s.empty()) {
    add_one_option(s, details);
  }

  if (!l.empty()) {
    add_one_option(l, details);
  }

  // Add the help details.
  auto& options = help_[group];

  options.options.emplace_back(HelpOptionDetails{s, l, string_desc,
      value->get_default_value(), value->get_implicit_value(),
      std::move(arg_help),
      value->has_implicit(), value->has_default(),
      value->is_container(), value->is_boolean()});
}

void
Options::add_one_option(
  const std::string& option,
  const std::shared_ptr<option_details>& details)
{
  const auto in = options_.emplace(option, details);

  if (!in.second) {
    throw_or_mimic<option_exists_error>(option);
  }
}

String
Options::help_one_group(const std::string& g) const {
  using OptionHelp = std::vector<std::pair<String, String>>;

  auto group = help_.find(g);
  if (group == help_.end()) {
    return "";
  }

  OptionHelp format;
  size_t longest = 0;
  String result;

  if (!g.empty()) {
    result += toLocalString(g);
    result += '\n';
  }

  for (const auto& o : group->second.options) {
    if (positional_set_.find(o.l) != positional_set_.end() &&
        !show_positional_)
    {
      continue;
    }

    auto s = format_option(o);
    longest = std::max(longest, stringLength(s));
    format.push_back(std::make_pair(s, String()));
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
    if (stringLength(fiter->first) > longest) {
      result += '\n';
      result += toLocalString(std::string(longest + OPTION_DESC_GAP, ' '));
    }
    else {
      result += toLocalString(std::string(longest + OPTION_DESC_GAP -
        stringLength(fiter->first),
        ' '));
    }
    result += d;
    result += '\n';

    ++fiter;
  }

  return result;
}

void
Options::generate_group_help(
  String& result,
  const std::vector<std::string>& print_groups) const
{
  for (size_t i = 0; i != print_groups.size(); ++i) {
    const String& group_help_text = help_one_group(print_groups[i]);
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
Options::generate_all_groups_help(String& result) const {
  generate_group_help(result, groups());
}

std::string
Options::help(const std::vector<std::string>& help_groups) const {
  String result;

  if (!help_string_.empty()) {
    result += help_string_;
    result += '\n';
  }

  result += "usage: ";
  result += toLocalString(program_);
  result += " ";
  result += toLocalString(custom_help_);

  if (!positional_.empty() && !positional_help_.empty()) {
    result += " ";
    result += toLocalString(positional_help_);
  }

  result += "\n\n";

  if (help_groups.empty()) {
    generate_all_groups_help(result);
  } else {
    generate_group_help(result, help_groups);
  }

  return toUTF8String(result);
}

std::vector<std::string>
Options::groups() const {
  std::vector<std::string> names;

  std::transform(help_.begin(), help_.end(), std::back_inserter(names),
    [] (const std::map<std::string, HelpGroupDetails>::value_type& pair) {
      return pair.first;
    }
  );

  return names;
}

const HelpGroupDetails&
Options::group_help(const std::string& group) const {
  return help_.at(group);
}

Options::OptionAdder
Options::add_options(std::string group)
{
  return OptionAdder(std::move(group), *this);
}


Options::OptionAdder::OptionAdder(std::string group, Options& options)
  : group_(std::move(group))
  , options_(options)
{
}

Options::OptionAdder&
Options::OptionAdder::operator()(
  const std::string& opts,
  const std::string& desc,
  const std::shared_ptr<const value_base>& value,
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
