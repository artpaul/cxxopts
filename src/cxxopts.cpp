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
  inline
  String
  toLocalString(std::string s) {
    return icu::UnicodeString::fromUTF8(std::move(s));
  }

  class UnicodeStringIterator : public
    std::iterator<std::forward_iterator_tag, int32_t>
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

  inline
  String&
  stringAppend(String&s, String a) {
    return s.append(std::move(a));
  }

  inline
  String&
  stringAppend(String& s, size_t n, UChar32 c) {
    for (size_t i = 0; i != n; ++i) {
      s.append(c);
    }

    return s;
  }

  template <typename Iterator>
  String&
  stringAppend(String& s, Iterator begin, Iterator end) {
    while (begin != end) {
      s.append(*begin);
      ++begin;
    }

    return s;
  }

  inline
  size_t
  stringLength(const String& s) {
    return s.length();
  }

  inline
  std::string
  toUTF8String(const String& s) {
    std::string result;
    s.toUTF8String(result);

    return result;
  }

  inline
  bool
  empty(const String& s) {
    return s.isEmpty();
  }
}

namespace std {
  inline
  cxxopts::UnicodeStringIterator
  begin(const icu::UnicodeString& s) {
    return cxxopts::UnicodeStringIterator(&s, 0);
  }

  inline
  cxxopts::UnicodeStringIterator
  end(const icu::UnicodeString& s) {
    return cxxopts::UnicodeStringIterator(&s, s.length());
  }
}

//ifdef CXXOPTS_USE_UNICODE
#else

namespace cxxopts {
  template <typename T>
  T
  toLocalString(T&& t) {
    return std::forward<T>(t);
  }

  inline
  size_t
  stringLength(const String& s) {
    return s.length();
  }

  inline
  String&
  stringAppend(String&s, const String& a) {
    return s.append(a);
  }

  inline
  String&
  stringAppend(String& s, size_t n, char c) {
    return s.append(n, c);
  }

  template <typename Iterator>
  String&
  stringAppend(String& s, Iterator begin, Iterator end) {
    return s.append(begin, end);
  }

  template <typename T>
  std::string
  toUTF8String(T&& t) {
    return std::forward<T>(t);
  }

  inline
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

OptionException::OptionException(std::string message)
  : m_message(std::move(message))
{
}

CXXOPTS_NODISCARD
const char*
OptionException::what() const noexcept {
  return m_message.c_str();
}


OptionSpecException::OptionSpecException(std::string message)
  : OptionException(std::move(message))
{
}


OptionParseException::OptionParseException(std::string message)
  : OptionException(std::move(message))
{
}


option_exists_error::option_exists_error(const std::string& option)
  : OptionSpecException(
      "Option " + LQUOTE + option + RQUOTE + " already exists")
{
}


invalid_option_format_error::invalid_option_format_error(
    const std::string& format
  )
  : OptionSpecException("Invalid option format " + LQUOTE + format + RQUOTE)
{
}


option_syntax_exception::option_syntax_exception(const std::string& text)
  : OptionParseException("Argument " + LQUOTE + text + RQUOTE +
    " starts with a - but has incorrect syntax")
{
}


option_not_exists_exception::option_not_exists_exception(
    const std::string& option
  )
  : OptionParseException(
      "Option " + LQUOTE + option + RQUOTE + " does not exist")
{
}


missing_argument_exception::missing_argument_exception(const std::string& option)
  : OptionParseException(
    "Option " + LQUOTE + option + RQUOTE + " is missing an argument")
{
}


option_requires_argument_exception::option_requires_argument_exception(
    const std::string& option
  )
  : OptionParseException(
    "Option " + LQUOTE + option + RQUOTE + " requires an argument")
{
}


option_not_has_argument_exception::option_not_has_argument_exception(
  const std::string& option,
  const std::string& arg)
  : OptionParseException(
      "Option " + LQUOTE + option + RQUOTE +
      " does not take an argument, but argument " +
      LQUOTE + arg + RQUOTE + " given")
{
}


option_not_present_exception::option_not_present_exception(
    const std::string& option
  )
  : OptionParseException("Option " + LQUOTE + option + RQUOTE + " not present")
{
}


option_has_no_value_exception::option_has_no_value_exception(
    const std::string& option
  )
  : OptionException(
      option.empty()
        ? "Option has no value"
        : "Option " + LQUOTE + option + RQUOTE + " has no value"
    )
{
}


argument_incorrect_type::argument_incorrect_type(const std::string& arg)
  : OptionParseException(
    "Argument " + LQUOTE + arg + RQUOTE + " failed to parse")
{
}


option_required_exception::option_required_exception(const std::string& option)
  : OptionParseException(
    "Option " + LQUOTE + option + RQUOTE + " is required but not present")
{
}


namespace values {
namespace {

template <typename T, bool B>
struct SignedCheck;

template <typename T>
struct SignedCheck<T, true> {
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
struct SignedCheck<T, false> {
  template <typename U>
  void
  operator()(bool, U, const std::string&) const noexcept {}
};

template <typename T, typename U>
void
check_signed_range(bool negative, U value, const std::string& text) {
  SignedCheck<T, std::numeric_limits<T>::is_signed>()(negative, value, text);
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


OptionDetails::OptionDetails(
    std::string short_,
    std::string long_,
    String desc,
    std::shared_ptr<const Value> val
  )
  : m_short(std::move(short_))
  , m_long(std::move(long_))
  , m_desc(std::move(desc))
  , m_value(std::move(val))
  , m_count(0)
{
  m_hash = std::hash<std::string>{}(m_long + m_short);
}

OptionDetails::OptionDetails(const OptionDetails& rhs)
  : m_desc(rhs.m_desc)
  , m_value(rhs.m_value->clone())
  , m_count(rhs.m_count)
  , m_hash(0)
{
}

CXXOPTS_NODISCARD
const String&
OptionDetails::description() const {
  return m_desc;
}

CXXOPTS_NODISCARD
const Value&
OptionDetails::value() const {
    return *m_value;
}

CXXOPTS_NODISCARD
std::shared_ptr<Value>
OptionDetails::make_storage() const {
  return m_value->clone();
}

CXXOPTS_NODISCARD
const std::string&
OptionDetails::short_name() const {
  return m_short;
}

CXXOPTS_NODISCARD
const std::string&
OptionDetails::long_name() const {
  return m_long;
}

size_t
OptionDetails::hash() const noexcept {
  return m_hash;
}


void
OptionValue::parse(
  const std::shared_ptr<const OptionDetails>& details,
  const std::string& text)
{
  ensure_value(details);
  ++m_count;
  m_value->parse(text);
  m_long_name = &details->long_name();
}

void
OptionValue::parse_default(
  const std::shared_ptr<const OptionDetails>& details)
{
  ensure_value(details);
  m_default = true;
  m_long_name = &details->long_name();
  m_value->parse();
}

void
OptionValue::parse_no_value(
  const std::shared_ptr<const OptionDetails>& details)
{
  m_long_name = &details->long_name();
}

#if defined(__GNUC__)
#if __GNUC__ <= 10 && __GNUC_MINOR__ <= 1
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Werror=null-dereference"
#endif
#endif

CXXOPTS_NODISCARD
size_t
OptionValue::count() const noexcept {
  return m_count;
}

#if defined(__GNUC__)
#if __GNUC__ <= 10 && __GNUC_MINOR__ <= 1
#pragma GCC diagnostic pop
#endif
#endif

CXXOPTS_NODISCARD
bool
OptionValue::has_default() const noexcept {
  return m_default;
}

CXXOPTS_NODISCARD
bool
OptionValue::has_value() const noexcept {
  return m_value != nullptr;
}

void
OptionValue::ensure_value(const std::shared_ptr<const OptionDetails>& details) {
  if (m_value == nullptr) {
    m_value = details->make_storage();
  }
}


KeyValue::KeyValue(std::string key_, std::string value_)
  : m_key(std::move(key_))
  , m_value(std::move(value_))
{
}

CXXOPTS_NODISCARD
const std::string&
KeyValue::key() const {
  return m_key;
}

CXXOPTS_NODISCARD
const std::string&
KeyValue::value() const {
  return m_value;
}


ParseResult::ParseResult(
    NameHashMap&& keys,
    ParsedHashMap&& values,
    std::vector<KeyValue> sequential,
    std::vector<std::string>&& unmatched_args
  )
  : keys_(std::move(keys))
  , values_(std::move(values))
  , sequential_(std::move(sequential))
  , unmatched_(std::move(unmatched_args))
{
}

size_t
ParseResult::count(const std::string& o) const {
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

const OptionValue&
ParseResult::operator[](const std::string& option) const {
  const auto ki = keys_.find(option);

  if (ki == keys_.end()) {
    throw_or_mimic<option_not_present_exception>(option);
  }

  const auto vi = values_.find(ki->second);

  if (vi == values_.end()) {
    throw_or_mimic<option_not_present_exception>(option);
  }

  return vi->second;
}

const std::vector<KeyValue>&
ParseResult::arguments() const {
  return sequential_;
}

const std::vector<std::string>&
ParseResult::unmatched() const {
  return unmatched_;
}

Option::Option(
    std::string opts,
    std::string desc,
    std::shared_ptr<const Value> value,
    std::string arg_help
  )
  : opts_(std::move(opts))
  , desc_(std::move(desc))
  , value_(std::move(value))
  , arg_help_(std::move(arg_help))
{
}

OptionParser::OptionParser(
    const OptionMap& options,
    const PositionalList& positional,
    bool allow_unrecognised
  )
  : m_options(options)
  , m_positional(positional)
  , m_allow_unrecognised(allow_unrecognised)
{
}

Options::Options(std::string program, std::string help_string)
  : m_program(std::move(program))
  , m_help_string(toLocalString(std::move(help_string)))
  , m_custom_help("[OPTION...]")
  , m_positional_help("positional parameters")
  , m_width(76)
  , m_show_positional(false)
  , m_allow_unrecognised(false)
  , m_tab_expansion(false)
  , m_options(std::make_shared<OptionMap>())
{
}

Options&
Options::positional_help(std::string help_text) {
  m_positional_help = std::move(help_text);
  return *this;
}

Options&
Options::custom_help(std::string help_text) {
  m_custom_help = std::move(help_text);
  return *this;
}

Options&
Options::show_positional_help() {
  m_show_positional = true;
  return *this;
}

Options&
Options::allow_unrecognised_options() {
  m_allow_unrecognised = true;
  return *this;
}

Options&
Options::set_width(size_t width) {
  m_width = width;
  return *this;
}

Options&
Options::set_tab_expansion(bool expansion) {
  m_tab_expansion = expansion;
  return *this;
}

void
Options::add_options(
  const std::string &group,
  std::initializer_list<Option> options)
{
  OptionAdder option_adder(*this, group);
  for (const auto &option: options) {
    option_adder(option.opts_, option.desc_, option.value_, option.arg_help_);
  }
}

void
OptionParser::parse_default(const std::shared_ptr<OptionDetails>& details) {
  // TODO: remove the duplicate code here
  auto& store = m_parsed[details->hash()];
  store.parse_default(details);
}

void
OptionParser::parse_no_value(const std::shared_ptr<OptionDetails>& details) {
  auto& store = m_parsed[details->hash()];
  store.parse_no_value(details);
}

void
OptionParser::parse_option(
  const std::shared_ptr<OptionDetails>& value,
  const std::string& /*name*/,
  const std::string& arg)
{
  auto hash = value->hash();
  auto& result = m_parsed[hash];
  result.parse(value, arg);

  m_sequential.emplace_back(value->long_name(), arg);
}

void
OptionParser::checked_parse_arg(
  int argc,
  const char* const* argv,
  int& current,
  const std::shared_ptr<OptionDetails>& value,
  const std::string& name)
{
  if (current + 1 >= argc) {
    if (value->value().has_implicit()) {
      parse_option(value, name, value->value().get_implicit_value());
    } else {
      throw_or_mimic<missing_argument_exception>(name);
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
  while (next != m_positional.end()) {
    auto iter = m_options.find(*next);
    if (iter != m_options.end()) {
      if (!iter->second->value().is_container()) {
        auto& result = m_parsed[iter->second->hash()];
        if (result.count() == 0) {
          add_to_option(iter, *next, a);
          ++next;
          return true;
        }
        ++next;
        continue;
      }
      add_to_option(iter, *next, a);
      return true;
    }
    throw_or_mimic<option_not_exists_exception>(*next);
  }

  return false;
}

void
Options::parse_positional(std::string option) {
  parse_positional(std::vector<std::string>{std::move(option)});
}

void
Options::parse_positional(std::vector<std::string> options) {
  m_positional = std::move(options);

  m_positional_set.insert(m_positional.begin(), m_positional.end());
}

void
Options::parse_positional(std::initializer_list<std::string> options) {
  parse_positional(std::vector<std::string>(std::move(options)));
}

ParseResult
Options::parse(int argc, const char* const* argv) {
  OptionParser parser(*m_options, m_positional, m_allow_unrecognised);

  return parser.parse(argc, argv);
}

ParseResult
OptionParser::parse(int argc, const char* const* argv) {
  int current = 1;
  bool consume_remaining = false;
  auto next_positional = m_positional.begin();

  std::vector<std::string> unmatched;

  while (current != argc) {
    if (strcmp(argv[current], "--") == 0) {
      consume_remaining = true;
      ++current;
      break;
    }

    std::match_results<const char*> result;
    std::regex_match(argv[current], result, option_matcher);

    if (result.empty()) {
      //not a flag

      // but if it starts with a `-`, then it's an error
      if (argv[current][0] == '-' && argv[current][1] != '\0') {
        if (!m_allow_unrecognised) {
          throw_or_mimic<option_syntax_exception>(argv[current]);
        }
      }

      //if true is returned here then it was consumed, otherwise it is
      //ignored
      if (consume_positional(argv[current], next_positional)) {
        ;
      } else {
        unmatched.emplace_back(argv[current]);
      }
      //if we return from here then it was parsed successfully, so continue
    } else {
      //short or long option?
      if (result[4].length() != 0) {
        const std::string& s = result[4];

        for (std::size_t i = 0; i != s.size(); ++i) {
          std::string name(1, s[i]);
          auto iter = m_options.find(name);

          if (iter == m_options.end()) {
            if (m_allow_unrecognised) {
              continue;
            }
            //error
            throw_or_mimic<option_not_exists_exception>(name);
          }

          auto value = iter->second;

          if (i + 1 == s.size()) {
            //it must be the last argument
            checked_parse_arg(argc, argv, current, value, name);
          } else if (value->value().has_implicit()) {
            parse_option(value, name, value->value().get_implicit_value());
          } else {
            //error
            throw_or_mimic<option_requires_argument_exception>(name);
          }
        }
      } else if (result[1].length() != 0) {
        const std::string& name = result[1];

        auto iter = m_options.find(name);

        if (iter == m_options.end()) {
          if (m_allow_unrecognised) {
            // keep unrecognised options in argument list, skip to next argument
            unmatched.emplace_back(argv[current]);
            ++current;
            continue;
          }
          //error
          throw_or_mimic<option_not_exists_exception>(name);
        }

        auto opt = iter->second;

        //equals provided for long option?
        if (result[2].length() != 0) {
          //parse the option given

          parse_option(opt, name, result[3]);
        } else {
          //parse the next argument
          checked_parse_arg(argc, argv, current, opt, name);
        }
      }
    }

    ++current;
  }

  for (auto& opt : m_options) {
    auto& detail = opt.second;
    const auto& value = detail->value();

    auto& store = m_parsed[detail->hash()];

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

  if (consume_remaining) {
    while (current < argc) {
      if (!consume_positional(argv[current], next_positional)) {
        break;
      }
      ++current;
    }

    //adjust argv for any that couldn't be swallowed
    while (current != argc) {
      unmatched.emplace_back(argv[current]);
      ++current;
    }
  }

  finalise_aliases();

  return ParseResult(
    std::move(m_keys),
    std::move(m_parsed),
    std::move(m_sequential),
    std::move(unmatched));
}

void
OptionParser::finalise_aliases() {
  for (auto& option: m_options) {
    auto& detail = *option.second;
    auto hash = detail.hash();
    m_keys[detail.short_name()] = hash;
    m_keys[detail.long_name()] = hash;

    m_parsed.emplace(hash, OptionValue());
  }
}

void
Options::add_option(
  const std::string& group,
  const Option& option)
{
    add_options(group, {option});
}

void
Options::add_option(
  const std::string& group,
  const std::string& s,
  const std::string& l,
  std::string desc,
  const std::shared_ptr<const Value>& value,
  std::string arg_help)
{
  auto stringDesc = toLocalString(std::move(desc));
  auto option = std::make_shared<OptionDetails>(s, l, stringDesc, value);

  if (!s.empty()) {
    add_one_option(s, option);
  }

  if (!l.empty()) {
    add_one_option(l, option);
  }

  m_option_list.push_front(*option.get());
  auto iter = m_option_list.begin();
  m_option_map[s] = iter;
  m_option_map[l] = iter;

  //add the help details
  auto& options = m_help[group];

  options.options.emplace_back(HelpOptionDetails{s, l, stringDesc,
      value->get_default_value(), value->get_implicit_value(),
      std::move(arg_help),
      value->has_implicit(), value->has_default(),
      value->is_container(), value->is_boolean()});
}

void
Options::add_one_option(
  const std::string& option,
  const std::shared_ptr<OptionDetails>& details)
{
  auto in = m_options->emplace(option, details);

  if (!in.second) {
    throw_or_mimic<option_exists_error>(option);
  }
}

String
Options::help_one_group(const std::string& g) const {
  using OptionHelp = std::vector<std::pair<String, String>>;

  auto group = m_help.find(g);
  if (group == m_help.end()) {
    return "";
  }

  OptionHelp format;
  size_t longest = 0;
  String result;

  if (!g.empty()) {
    result += toLocalString(g + "\n");
  }

  for (const auto& o : group->second.options) {
    if (m_positional_set.find(o.l) != m_positional_set.end() &&
        !m_show_positional)
    {
      continue;
    }

    auto s = format_option(o);
    longest = (std::max)(longest, stringLength(s));
    format.push_back(std::make_pair(s, String()));
  }
  longest = (std::min)(longest, OPTION_LONGEST);

  //widest allowed description -- min 10 chars for helptext/line
  size_t allowed = 10;
  if (m_width > allowed + longest + OPTION_DESC_GAP) {
    allowed = m_width - longest - OPTION_DESC_GAP;
  }

  auto fiter = format.begin();
  for (const auto& o : group->second.options) {
    if (m_positional_set.find(o.l) != m_positional_set.end() &&
        !m_show_positional)
    {
      continue;
    }

    auto d = format_description(
      o, longest + OPTION_DESC_GAP,
      allowed,
      m_tab_expansion);

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
  std::vector<std::string> all_groups;

  std::transform(
    m_help.begin(),
    m_help.end(),
    std::back_inserter(all_groups),
    [] (const std::map<std::string, HelpGroupDetails>::value_type& group) {
      return group.first;
    }
  );

  generate_group_help(result, all_groups);
}

std::string
Options::help(const std::vector<std::string>& help_groups) const {
  std::string result;

  if (!m_help_string.empty()) {
    result += m_help_string + "\n";
  }
  result += "usage: " +
    toLocalString(m_program) + " " + toLocalString(m_custom_help);

  if (!m_positional.empty() && !m_positional_help.empty()) {
    result += " " + toLocalString(m_positional_help);
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
  std::vector<std::string> g;

  std::transform(
    m_help.begin(),
    m_help.end(),
    std::back_inserter(g),
    [] (const std::map<std::string, HelpGroupDetails>::value_type& pair) {
      return pair.first;
    }
  );

  return g;
}

const HelpGroupDetails&
Options::group_help(const std::string& group) const {
  return m_help.at(group);
}

OptionAdder
Options::add_options(std::string group)
{
  return OptionAdder(*this, std::move(group));
}


OptionAdder::OptionAdder(Options& options, std::string group)
  : m_options(options)
  , m_group(std::move(group))
{
}

OptionAdder&
OptionAdder::operator()(
  const std::string& opts,
  const std::string& desc,
  const std::shared_ptr<const Value>& value,
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

  m_options.add_option(
    m_group,
    std::get<0>(option_names),
    std::get<1>(option_names),
    desc,
    value,
    std::move(arg_help));

  return *this;
}

} // namespace cxxopts
