#include "catch.hpp"
#include "cxxopts.hpp"

#include <cstring>
#include <initializer_list>
#include <list>

namespace {

class Argv {
public:
  Argv(std::initializer_list<const char*> args)
    : argc_(static_cast<int>(args.size()))
    , argv_(new const char*[args.size()])
  {
    int i = 0;
    auto iter = args.begin();
    while (iter != args.end()) {
      auto len = strlen(*iter) + 1;
      auto ptr = std::unique_ptr<char[]>(new char[len]);

      strcpy(ptr.get(), *iter);
      args_.push_back(std::move(ptr));
      argv_.get()[i] = args_.back().get();

      ++iter;
      ++i;
    }
  }

  const char** argv() const {
    return argv_.get();
  }

  int argc() const {
    return argc_;
  }

private:
  const int argc_;
  std::unique_ptr<const char*[]> argv_{};
  std::vector<std::unique_ptr<char[]>> args_{};
};

template <typename T>
bool validate_value_parser(const std::string& text, T&& expected) {
  T value;
  cxxopts::detail::parse_value(text, value);
  return value == expected;
}

} // namespace


TEST_CASE("Value", "traits") {
  CHECK(!cxxopts::value<int>()->is_boolean());
  CHECK(!cxxopts::value<std::string>()->is_boolean());

  CHECK(cxxopts::value<bool>()->is_boolean());

  CHECK(!cxxopts::value<std::string>()->is_container());
  CHECK(!cxxopts::value<bool>()->is_container());

  CHECK(cxxopts::value<std::vector<int>>()->is_container());
  CHECK(cxxopts::value<std::vector<std::string>>()->is_container());
}


TEST_CASE("Booleans", "[boolean]") {
  cxxopts::options options("parses_floats", "parses floats correctly");
  options.add_options()
    ("bool", "A Boolean", cxxopts::value<bool>())
    ("debug", "Debugging", cxxopts::value<bool>())
    ("timing", "Timing", cxxopts::value<bool>())
    ("verbose", "Verbose", cxxopts::value<bool>())
    ("dry-run", "Dry Run", cxxopts::value<bool>())
    ("noExplicitDefault", "No Explicit Default", cxxopts::value<bool>())
    ("defaultTrue", "Timing", cxxopts::value<bool>()->default_value("true"))
    ("defaultFalse", "Timing", cxxopts::value<bool>()->default_value("false"))
    ("others", "Other arguments", cxxopts::value<std::vector<std::string>>())
    ;
  options.parse_positional("others");

  const Argv argv({
    "booleans", "--bool=false", "--debug=true", "--timing", "--verbose=1",
    "--dry-run=0", "extra"
  });

  const auto result = options.parse(argv.argc(), argv.argv());

  REQUIRE(result.count("bool") == 1);
  REQUIRE(result.count("debug") == 1);
  REQUIRE(result.count("timing") == 1);
  REQUIRE(result.count("verbose") == 1);
  REQUIRE(result.count("dry-run") == 1);
  REQUIRE(result.count("noExplicitDefault") == 0);
  REQUIRE(result.count("defaultTrue") == 0);
  REQUIRE(result.count("defaultFalse") == 0);

  CHECK(result["bool"].as<bool>() == false);
  CHECK(result["debug"].as<bool>() == true);
  CHECK(result["timing"].as<bool>() == true);
  CHECK(result["verbose"].as<bool>() == true);
  CHECK(result["dry-run"].as<bool>() == false);
  CHECK(result["noExplicitDefault"].as<bool>() == false);
  CHECK(result["defaultTrue"].as<bool>() == true);
  CHECK(result["defaultFalse"].as<bool>() == false);

  REQUIRE(result.count("others") == 1);
}

TEST_CASE("Parse ingeger", "[integer]") {
  CHECK(validate_value_parser<int8_t>("-1", -1));
  CHECK(validate_value_parser<int8_t>("+1", +1));
}

TEST_CASE("Parse invalid ingeger", "[integer]") {
  using namespace cxxopts::detail;

  int8_t si;

  CHECK_THROWS_AS((parse_value("", si)), cxxopts::argument_incorrect_type&);
  CHECK_THROWS_AS((parse_value("-", si)), cxxopts::argument_incorrect_type&);
  CHECK_THROWS_AS((parse_value("+", si)), cxxopts::argument_incorrect_type&);
  CHECK_THROWS_AS((parse_value("-0x", si)), cxxopts::argument_incorrect_type&);
  CHECK_THROWS_AS((parse_value("+0x", si)), cxxopts::argument_incorrect_type&);
  CHECK_THROWS_AS((parse_value("0x", si)), cxxopts::argument_incorrect_type&);
}

TEST_CASE("Overflow on boundary", "[integer]") {
  using namespace cxxopts::detail;

  int8_t si;
  uint8_t ui;

  CHECK_THROWS_AS((parse_value("128", si)), cxxopts::argument_incorrect_type&);
  CHECK_THROWS_AS((parse_value("-129", si)), cxxopts::argument_incorrect_type&);
  CHECK_THROWS_AS((parse_value("256", ui)), cxxopts::argument_incorrect_type&);
  CHECK_THROWS_AS((parse_value("-0x81", si)), cxxopts::argument_incorrect_type&);
  CHECK_THROWS_AS((parse_value("0x80", si)), cxxopts::argument_incorrect_type&);
  CHECK_THROWS_AS((parse_value("0x100", ui)), cxxopts::argument_incorrect_type&);
}

TEST_CASE("Integer overflow", "[integer]") {
  using namespace cxxopts::detail;

  int integer = 0;
  CHECK_THROWS_AS((parse_value("23423423423", integer)), cxxopts::argument_incorrect_type&);
  CHECK_THROWS_AS((parse_value("234234234234", integer)), cxxopts::argument_incorrect_type&);
}

TEST_CASE("Integers", "[integer]") {
  cxxopts::options options("parses_integers", "parses integers correctly");
  options.add_options()
    ("positional", "Integers", cxxopts::value<std::vector<int>>());
  options.parse_positional("positional");

  const Argv argv({"ints", "--", "5", "6", "-6", "0", "0xab", "0xAf", "0x0"});
  const auto result = options.parse(argv.argc(), argv.argv());

  REQUIRE(result.count("positional") == 7);

  const auto& positional = result["positional"].as<std::vector<int>>();
  REQUIRE(positional.size() == 7);
  CHECK(positional[0] == 5);
  CHECK(positional[1] == 6);
  CHECK(positional[2] == -6);
  CHECK(positional[3] == 0);
  CHECK(positional[4] == 0xab);
  CHECK(positional[5] == 0xaf);
  CHECK(positional[6] == 0x0);
}

TEST_CASE("Leading zero integers", "[integer]") {
  cxxopts::options options("parses_integers", "parses integers correctly");
  options.add_options()
    ("positional", "Integers", cxxopts::value<std::vector<int>>());
  options.parse_positional("positional");

  const Argv argv({"ints", "--", "05", "06", "0x0ab", "0x0001"});
  const auto result = options.parse(argv.argc(), argv.argv());

  REQUIRE(result.count("positional") == 4);

  const auto& positional = result["positional"].as<std::vector<int>>();
  REQUIRE(positional.size() == 4);
  CHECK(positional[0] == 5);
  CHECK(positional[1] == 6);
  CHECK(positional[2] == 0xab);
  CHECK(positional[3] == 0x1);
}

TEST_CASE("Unsigned integers", "[integer]") {
  cxxopts::options options("parses_unsigned", "detects unsigned errors");
  options.add_options()
    ("positional", "Integers", cxxopts::value<std::vector<unsigned int>>());
  options.parse_positional("positional");

  const Argv argv({"ints", "--", "-2"});

  CHECK_THROWS_AS(options.parse(argv.argc(), argv.argv()),
    cxxopts::argument_incorrect_type&);
}

TEST_CASE("Integer bounds", "[integer]") {
  cxxopts::options options("integer_boundaries", "check min/max integer");
  options.add_options()
    ("positional", "Integers", cxxopts::value<std::vector<int8_t>>());
  options.parse_positional("positional");

  SECTION("No overflow") {
    const Argv argv({"ints", "--", "127", "-128", "0x7f", "-0x80", "0x7e"});
    const auto& result = options.parse(argv.argc(), argv.argv());

    REQUIRE(result.count("positional") == 5);

    const auto& positional = result["positional"].as<std::vector<int8_t>>();
    CHECK(positional[0] == 127);
    CHECK(positional[1] == -128);
    CHECK(positional[2] == 0x7f);
    CHECK(positional[3] == -0x80);
    CHECK(positional[4] == 0x7e);
  }
}

TEST_CASE("Reject overflow", "[integer]") {
  cxxopts::options options("reject_overflow", "rejects overflowing integers");
  options.add_options()
    ("positional", "Integers", cxxopts::value<std::vector<int8_t>>());
  options.parse_positional("positional");

  const Argv argv({"ints", "--", "128"});

  CHECK_THROWS_AS(options.parse(argv.argc(), argv.argv()),
    cxxopts::argument_incorrect_type&);
}

TEST_CASE("Invalid integers", "[integer]") {
  cxxopts::options options("invalid_integers", "rejects invalid integers");
  options.add_options()
    ("positional", "Integers", cxxopts::value<std::vector<int>>());
  options.parse_positional("positional");

  const Argv argv({"ints", "--", "Ae"});

  CHECK_THROWS_AS(options.parse(argv.argc(), argv.argv()),
    cxxopts::argument_incorrect_type&);
}


TEST_CASE("Floats", "[float]") {
  cxxopts::options options("parses_floats", "parses floats correctly");
  options.add_options()
    ("double", "Double precision", cxxopts::value<double>())
    ("positional", "Floats", cxxopts::value<std::vector<float>>());
  options.parse_positional("positional");

  const Argv argv({
    "floats", "--double", "0.5", "--", "4", "-4", "1.5e6", "-1.5e6"});
  const auto result = options.parse(argv.argc(), argv.argv());

  REQUIRE(result.count("double") == 1);
  REQUIRE(result.count("positional") == 4);

  CHECK(result["double"].as<double>() == 0.5);

  const auto& positional = result["positional"].as<std::vector<float>>();
  CHECK(positional[0] == 4);
  CHECK(positional[1] == -4);
  CHECK(positional[2] == 1.5e6);
  CHECK(positional[3] == -1.5e6);
}


TEST_CASE("Empty arguments", "[options]") {
  cxxopts::options options("test", " - test empty arguments");
  options.add_options()
    ("?,help", "show help")
    ("v", "verbose");

  Argv argv({});
  CHECK_NOTHROW(options.parse(argv.argc(), argv.argv()));
}

TEST_CASE("Question mark only", "[options]") {
  cxxopts::options options("test_short", " - test question mark");
  options.add_options()("?", "show help");

  const Argv argv({"test_short", "-?"});
  CHECK(options.parse(argv.argc(), argv.argv()).count("?") == 1);
}

TEST_CASE("Question mark help", "[options]") {
  cxxopts::options options("test_short", " - test question mark");

  options.add_options()
    ("?,help", "show help")
    ("v", "verbose");

  SECTION("Valid use of question mark") {
    Argv argv({"test_short", "-?"});
    CHECK(options.parse(argv.argc(), argv.argv()).count("help") == 1);
  }

  SECTION("Invalid use of qestion mark") {
    Argv argv({"test_short", "-v?"});
    CHECK_THROWS_AS(options.parse(argv.argc(), argv.argv()), cxxopts::option_syntax_error&);
  }
}

TEST_CASE("Basic options", "[options]") {
  cxxopts::options options("tester", " - test basic options");

  options.add_options()
    ("long", "a long option")
    ("s,short", "a short option")
    ("value", "an option with a value", cxxopts::value<std::string>())
    ("a,av", "a short option with a value", cxxopts::value<std::string>())
    ("6,six", "a short number option")
    ("p, space", "an option with space between short and long")
    ("nothing", "won't exist", cxxopts::value<std::string>())
    ;

  const Argv argv({
    "tester",
    "--long",
    "-s",
    "--value",
    "value",
    "-a",
    "b",
    "-6",
    "-p",
    "--space",
  });

  const auto result = options.parse(argv.argc(), argv.argv());

  CHECK(result.count("long") == 1);
  CHECK(result.count("s") == 1);
  CHECK(result.count("value") == 1);
  CHECK(result.count("a") == 1);
  CHECK(result["value"].as<std::string>() == "value");
  CHECK(result["a"].as<std::string>() == "b");
  CHECK(result.count("6") == 1);
  CHECK(result.count("p") == 2);
  CHECK(result.count("space") == 2);

  const auto& arguments = result.arguments();
  REQUIRE(arguments.size() == 7);
  CHECK(arguments[0].key() == "long");
  CHECK(arguments[0].value() == "true");
  CHECK(arguments[0].as<bool>() == true);

  CHECK(arguments[1].key() == "short");
  CHECK(arguments[2].key() == "value");
  CHECK(arguments[3].key() == "av");

  CHECK_THROWS_AS(result["nothing"].as<std::string>(), cxxopts::option_has_no_value_error&);
}

TEST_CASE("Return parse result", "[parse result]") {
  auto parse = [&] () {
    const Argv argv({"test_short", "-a", "value"});
    cxxopts::options spec("test_short", " - test short options");
    spec.add_options()
      ("a", "a short option", cxxopts::value<std::string>());
    return spec.parse(argv.argc(), argv.argv());
  };

  const auto result = parse();
  CHECK(result.count("a") == 1);
  CHECK(result["a"].as<std::string>() == "value");
}

TEST_CASE("Subcommand options", "[options]") {
  const Argv argv({"test_subcommand", "-a", "value", "subcmd", "-a", "-x"});

  cxxopts::options options("test_subcommand", " - test subcommand options");
  options
    .stop_on_positional()
    .add_options()
      ("a", "a short option", cxxopts::value<std::string>());

  const auto result = options.parse(argv.argc(), argv.argv());
  CHECK(result.count("a") == 1);
  CHECK(result["a"].as<std::string>() == "value");
  CHECK(result.consumed() == 3);
}

TEST_CASE("Short options", "[options]") {
  cxxopts::options options("test_short", " - test short options");

  options.add_options()
    ("a", "a short option", cxxopts::value<std::string>());

  const Argv argv({"test_short", "-a", "value"});
  const auto result = options.parse(argv.argc(), argv.argv());

  CHECK(result.count("a") == 1);
  CHECK(result["a"].as<std::string>() == "value");
  CHECK(result.consumed() == argv.argc());

  REQUIRE_THROWS_AS(options.add_options()("", "nothing option"),
    cxxopts::invalid_option_format_error&);
}

TEST_CASE("Short options without space", "[options]") {
  cxxopts::options options("test_short", " - test short options without space");

  options.add_options()
    ("x", "a short option")
    ("a", "a short option", cxxopts::value<std::string>());

  const Argv argv({"test_short", "-xxavalue"});
  const auto result = options.parse(argv.argc(), argv.argv());

  CHECK(result.count("x") == 2);
  CHECK(result.count("a") == 1);
  CHECK(result["a"].as<std::string>() == "value");
}

TEST_CASE("No positional", "[positional]") {
  cxxopts::options options("test", " - test no positional options");

  const Argv argv({"test", "a", "b", "def"});
  const auto result = options.parse(argv.argc(), argv.argv());

  CHECK(result.consumed() == 4);
  CHECK(result.unmatched().size() == 3);
}

TEST_CASE("All positional", "[positional]") {
  std::vector<std::string> positional;

  cxxopts::options options("test", " - test all positional");
  options.add_options()
    ("positional", "Positional parameters",
      cxxopts::value<std::vector<std::string>>(positional));

  std::list<std::string> pos_names = {"positional"};
  options.parse_positional(pos_names.begin(), pos_names.end());

  const Argv argv({"tester", "a", "b", "c"});
  const auto result = options.parse(argv.argc(), argv.argv());

  CHECK(result.unmatched().size() == 0);
  REQUIRE(positional.size() == 3);

  CHECK(positional[0] == "a");
  CHECK(positional[1] == "b");
  CHECK(positional[2] == "c");
}

TEST_CASE("Some positional explicit", "[positional]") {
  cxxopts::options options("positional_explicit", " - test positional");

  options.add_options()
    ("input", "Input file", cxxopts::value<std::string>())
    ("output", "Output file", cxxopts::value<std::string>())
    ("positional", "Positional parameters",
      cxxopts::value<std::vector<std::string>>())
  ;
  options.parse_positional({"input", "output", "positional"});

  const Argv argv({"tester", "--output", "a", "b", "c", "d"});
  const auto result = options.parse(argv.argc(), argv.argv());

  CHECK(result.unmatched().size() == 0);
  CHECK(result.count("output"));
  CHECK(result["input"].as<std::string>() == "b");
  CHECK(result["output"].as<std::string>() == "a");

  auto& positional = result["positional"].as<std::vector<std::string>>();

  REQUIRE(positional.size() == 2);
  CHECK(positional[0] == "c");
  CHECK(positional[1] == "d");
}

TEST_CASE("No positional with extras", "[positional]") {
  cxxopts::options options("posargmaster", "shows incorrect handling");
  options.add_options()
    ("dummy", "oh no", cxxopts::value<std::string>())
    ;

  const Argv argv({"extras", "--", "a", "b", "c", "d"});
  const auto result = options.parse(argv.argc(), argv.argv());

  auto& unmatched = result.unmatched();
  CHECK((unmatched == std::vector<std::string>{"a", "b", "c", "d"}));
}

TEST_CASE("Positional not valid", "[positional]") {
  cxxopts::options options("positional_invalid", "invalid positional argument");
  options.add_options()
    ("long", "a long option", cxxopts::value<std::string>())
    ;
  options.parse_positional("something");

  const Argv argv({"foobar", "bar", "baz"});

  CHECK_THROWS_AS(options.parse(argv.argc(), argv.argv()),
    cxxopts::option_not_exists_error&);
}

TEST_CASE("Positional varargs", "[positional]") {
  cxxopts::options options("positional_varargs", "positional varargs");
  options.add_options()
    ("a", "a", cxxopts::value<std::string>())
    ("b", "b", cxxopts::value<std::string>())
    ("c", "c", cxxopts::value<std::vector<std::string>>());
  options.parse_positional("a", "b", "c");

  const Argv argv({"foobar", "1", "2", "3", "4"});
  const auto result = options.parse(argv.argc(), argv.argv());

  REQUIRE(result.count("a") == 1);
  REQUIRE(result.count("b") == 1);
  REQUIRE(result.count("c") == 2);
}

TEST_CASE("Positional with empty arguments", "[positional]") {
  cxxopts::options options("program", "positional with empty argument");
  options.add_options()
    ("long", "a long option", cxxopts::value<std::string>())
    ("program", "program to run", cxxopts::value<std::string>())
    ("programArgs", "program arguments", cxxopts::value<std::vector<std::string>>())
    ;
  options.parse_positional("program", "programArgs");

  const Argv argv({
    "foobar", "--long", "long_value", "--",
    "someProgram", "ab", "-c", "d", "--ef", "gh", "--ijk=lm", "n", "", "o", });
  const std::vector<std::string> expected({
    "ab", "-c", "d", "--ef", "gh", "--ijk=lm", "n", "", "o", });

  const auto result = options.parse(argv.argc(), argv.argv());
  const auto actual = result["programArgs"].as<std::vector<std::string>>();

  REQUIRE(result.count("program") == 1);
  REQUIRE(result["program"].as<std::string>() == "someProgram");
  REQUIRE(result.count("programArgs") == expected.size());
  REQUIRE(actual == expected);
}

TEST_CASE("Positional after flag", "[positional]") {
  cxxopts::options options("test", "positional after flag");
  options.add_options()
    ("f,flag", "boolean flag", cxxopts::value<bool>())
    ("param", "name of smth", cxxopts::value<std::string>());
  options.parse_positional("param");

  const Argv args({"test", "-f", "name"});
  const auto result = options.parse(args.argc(), args.argv());

  CHECK(result.has("flag"));
  CHECK(result.has("param"));
  CHECK(result["param"].as<std::string>() == "name");
}

TEST_CASE("Empty with implicit value", "[implicit]") {
  cxxopts::options options("empty_implicit", "doesn't handle empty");
  options.add_options()
    ("implicit", "Has implicit", cxxopts::value<std::string>()
      ->implicit_value("foo"));

  SECTION("Assign empty") {
    const Argv av({"implicit", "--implicit="});
    const auto result = options.parse(av.argc(), av.argv());

    REQUIRE(result.count("implicit") == 1);
    REQUIRE(result["implicit"].as<std::string>() == "");
  }

  SECTION("Empty argument") {
    const Argv av({"implicit", "--implicit", ""});
    const auto result = options.parse(av.argc(), av.argv());

    REQUIRE(result.count("implicit") == 1);
    REQUIRE(result["implicit"].as<std::string>() == "");
  }
}

TEST_CASE("Option with implicit value", "[implicit]") {
  cxxopts::options options("implicit", "implicit value");
  options.add_options()
    ("o,output", "path", cxxopts::value<std::string>()->implicit_value("a.out"))
    ("f,flag", "flag");

  SECTION("No value") {
    const Argv av({"implicit", "--output"});
    const auto result = options.parse(av.argc(), av.argv());

    REQUIRE(result.has("output"));
    REQUIRE(result["output"].as<std::string>() == "a.out");
  }

  SECTION("Implicit option plus another option") {
    const Argv av({"implicit", "--output", "--flag"});
    const auto result = options.parse(av.argc(), av.argv());

    REQUIRE(result.has("output"));
    REQUIRE(result["output"].as<std::string>() == "a.out");
  }

  SECTION("Implicit option with dash-dash") {
    const Argv av({"implicit", "--output", "--", "tmp"});
    const auto result = options.parse(av.argc(), av.argv());

    REQUIRE(result.has("output"));
    REQUIRE(result["output"].as<std::string>() == "a.out");
  }

  SECTION("Value in next argument (short option)") {
    const Argv av({"implicit", "-o", "test"});
    const auto result = options.parse(av.argc(), av.argv());

    REQUIRE(result.has("output"));
    REQUIRE(result["output"].as<std::string>() == "test");
  }

  SECTION("Value in next argument (long option)") {
    const Argv av({"implicit", "--output", "test"});
    const auto result = options.parse(av.argc(), av.argv());

    REQUIRE(result.has("output"));
    REQUIRE(result["output"].as<std::string>() == "test");
  }

  SECTION("Option with assign") {
    const Argv av({"implicit", "--output=test"});
    const auto result = options.parse(av.argc(), av.argv());

    REQUIRE(result.has("output"));
    REQUIRE(result["output"].as<std::string>() == "test");
  }

  SECTION("Option and option-like argument") {
    const Argv av({"implicit", "--output", "--test"});
    const auto result = options.parse(av.argc(), av.argv());

    REQUIRE(result.has("output"));
    REQUIRE(result["output"].as<std::string>() == "--test");
  }
}

TEST_CASE("Boolean without implicit value", "[implicit]") {
  cxxopts::options options("no_implicit", "bool without an implicit value");
  options.add_options()
    ("bool", "Boolean without implicit", cxxopts::value<bool>()
      ->no_implicit_value());

  SECTION("When no value provided") {
    const Argv av({"no_implicit", "--bool"});

    CHECK_THROWS_AS(options.parse(av.argc(), av.argv()),
      cxxopts::missing_argument_error&);
  }

  SECTION("With equal-separated true") {
    const Argv av({"no_implicit", "--bool=true"});
    const auto result = options.parse(av.argc(), av.argv());

    CHECK(result.count("bool") == 1);
    CHECK(result["bool"].as<bool>() == true);
  }

  SECTION("With equal-separated false") {
    const Argv av({"no_implicit", "--bool=false"});
    const auto result = options.parse(av.argc(), av.argv());

    CHECK(result.count("bool") == 1);
    CHECK(result["bool"].as<bool>() == false);
  }

  SECTION("With space-separated true") {
    const Argv av({"no_implicit", "--bool", "true"});
    const auto result = options.parse(av.argc(), av.argv());

    CHECK(result.count("bool") == 1);
    CHECK(result["bool"].as<bool>() == true);
  }

  SECTION("With space-separated false") {
    const Argv av({"no_implicit", "--bool", "false"});
    const auto result = options.parse(av.argc(), av.argv());

    CHECK(result.count("bool") == 1);
    CHECK(result["bool"].as<bool>() == false);
  }
}

TEST_CASE("Default values", "[default]") {
  cxxopts::options options("defaults", "has defaults");
  options.add_options()
    ("default", "Has implicit", cxxopts::value<int>()->default_value("42"))
    ("v,vector", "Default vector", cxxopts::value<std::vector<int>>()
      ->default_value("1,4"))
    ;

  SECTION("Sets defaults") {
    const Argv argv({"implicit"});
    const auto result = options.parse(argv.argc(), argv.argv());

    CHECK(result.count("default") == 0);
    CHECK(result["default"].as<int>() == 42);

    auto& v = result["vector"].as<std::vector<int>>();
    REQUIRE(v.size() == 2);
    CHECK(v[0] == 1);
    CHECK(v[1] == 4);
  }

  SECTION("When values provided") {
    const Argv argv({"implicit", "--default", "5"});
    const auto result = options.parse(argv.argc(), argv.argv());

    CHECK(result.count("default") == 1);
    CHECK(result["default"].as<int>() == 5);
  }
}

TEST_CASE("Parse into a reference", "[reference]") {
  int value = 0;

  cxxopts::options options("into_reference", "parses into a reference");
  options.add_options()
    ("ref", "A reference", cxxopts::value(value));

  Argv av({"into_reference", "--ref", "42"});

  auto argv = av.argv();
  auto argc = av.argc();

  auto result = options.parse(argc, argv);
  CHECK(result.count("ref") == 1);
  CHECK(value == 42);
}


TEST_CASE("std::vector", "[vector]") {
  std::vector<double> vector;
  cxxopts::options options("vector", " - tests vector");
  options.add_options()
    ("vector", "an vector option", cxxopts::value<std::vector<double>>(vector));

  const Argv argv({"vector", "--vector", "1,-2.1,3,4.5"});

  options.parse(argv.argc(), argv.argv());

  REQUIRE(vector.size() == 4);
  CHECK(vector[0] == 1);
  CHECK(vector[1] == -2.1);
  CHECK(vector[2] == 3);
  CHECK(vector[3] == 4.5);
}

TEST_CASE("Integer vector with empty text", "[vector]") {
  cxxopts::options options("vector", " - tests vector");
  options.add_options()
    ("ints", "vector of ints", cxxopts::value<std::vector<int>>())
    ("values", "a vector positional", cxxopts::value<std::vector<int>>());
  options.parse_positional("values");

  SECTION("Values to opiton") {
    const Argv argv({"vector", "--ints", "1,2,,4"});

    CHECK_THROWS_AS(options.parse(argv.argc(), argv.argv()),
      cxxopts::argument_incorrect_type&);
  }

  SECTION("Positional to option") {
    const Argv argv({"vector", "--", "1", "2", "", "4"});

    CHECK_THROWS_AS(options.parse(argv.argc(), argv.argv()),
      cxxopts::argument_incorrect_type&);
  }
}

#ifdef CXXOPTS_HAS_OPTIONAL
TEST_CASE("std::optional", "[optional]") {
  std::optional<std::string> optional;
  cxxopts::options options("optional", " - tests optional");
  options.add_options()
    ("optional", "an optional option",
      cxxopts::value<std::optional<std::string>>(optional));

  const Argv argv({"optional", "--optional", "foo"});
  options.parse(argv.argc(), argv.argv());

  REQUIRE(optional.has_value());
  CHECK(*optional == "foo");
}
#endif

TEST_CASE("Unrecognised options", "[options]") {
  cxxopts::options options("unknown_options", " - test unknown options");

  options.add_options()
    ("long", "a long option")
    ("s,short", "a short option");

  Argv av({
    "unknown_options",
    "--unknown",
    "--long",
    "-su",
    "--another_unknown",
    "-a",
  });

  auto** argv = av.argv();
  auto argc = av.argc();

  SECTION("Default behaviour") {
    CHECK_THROWS_AS(options.parse(argc, argv), cxxopts::option_not_exists_error&);
  }

  SECTION("After allowing unrecognised options") {
    options.allow_unrecognised_options();
    auto result = options.parse(argc, argv);
    auto& unmatched = result.unmatched();
    CHECK((unmatched == std::vector<std::string>{"--unknown", "-u", "--another_unknown", "-a"}));
  }
}

TEST_CASE("Allow bad short syntax", "[options]") {
  cxxopts::options options("unknown_options", " - test unknown options");

  options.add_options()
    ("long", "a long option")
    ("s,short", "a short option");

  const Argv argv({"unknown_options", "-some_bad_short"});

  SECTION("Default behaviour") {
    CHECK_THROWS_AS(options.parse(argv.argc(), argv.argv()),
      cxxopts::option_syntax_error&);
  }

  SECTION("After allowing unrecognised options") {
    options.allow_unrecognised_options();
    CHECK_NOTHROW(options.parse(argv.argc(), argv.argv()));
    CHECK_THAT(argv.argv()[1], Catch::Equals("-some_bad_short"));
  }
}

TEST_CASE("Option not present", "[options]") {
  cxxopts::options options("not_present", " - test option not present");
  options.add_options()
    ("s", "a short option");

  const Argv argv({"not_present", "-s"});

  SECTION("Default behaviour") {
    cxxopts::parse_result result;
    CHECK_NOTHROW((result = options.parse(argv.argc(), argv.argv())));
    CHECK_THROWS_AS(result["a"], cxxopts::option_not_present_error&);
  }
}

TEST_CASE("Consume option as value", "[options]") {
  cxxopts::options options("long_option", " - test consume option");
  options.add_options()
    ("f,first", "first option", cxxopts::value<std::string>())
    ("s,second", "second option", cxxopts::value<std::string>());

  SECTION("Absent value") {
    const Argv argv({"long_option", "--first", "-s", "sv"});
    CHECK_THROWS_AS(options.parse(argv.argc(), argv.argv()), cxxopts::missing_argument_error&);
  }

  SECTION("Consume dash-dash as value") {
    const Argv argv({"long_option", "--first", "--", "-s", "sv"});
    CHECK_THROWS_AS(options.parse(argv.argc(), argv.argv()), cxxopts::missing_argument_error&);
  }

  SECTION("Correct dash-dash value") {
    const Argv argv({"long_option", "--first=--", "-s", "sv"});
    const auto result = options.parse(argv.argc(), argv.argv());

    CHECK(result.has("first"));
    CHECK(result.has("second"));

    CHECK(result["first"].as<std::string>() == "--");
    CHECK(result["second"].as<std::string>() == "sv");
  }

  SECTION("Correct option-like value") {
    const Argv argv({"long_option", "--first", "-o", "-s", "sv"});
    const auto result = options.parse(argv.argc(), argv.argv());

    CHECK(result.has("first"));
    CHECK(result.has("second"));

    CHECK(result["first"].as<std::string>() == "-o");
    CHECK(result["second"].as<std::string>() == "sv");
  }
}

TEST_CASE("Invalid option syntax", "[options]") {
  cxxopts::options options("invalid_syntax", " - test invalid syntax");

  const Argv argv({"invalid_syntax", "--a"});

  SECTION("Default behaviour") {
    CHECK_THROWS_AS(options.parse(argv.argc(), argv.argv()),
      cxxopts::option_syntax_error&);
  }
}

TEST_CASE("Options empty", "[options]") {
  cxxopts::options options("Options list empty", " - test empty option list");
  options.add_options();
  options.add_options("");
  options.add_options("", {});
  options.add_options("test");

  const Argv argv({"test", "--unknown"});
  CHECK(options.groups().empty());
  CHECK_THROWS_AS(options.parse(argv.argc(), argv.argv()),
    cxxopts::option_not_exists_error&);
}

TEST_CASE("Initializer list with group", "[options]") {
  cxxopts::options options("Initializer list group", " - test initializer list with group");

  options.add_options("", {
    {"a, address", "server address", cxxopts::value<std::string>()->default_value("127.0.0.1")},
    {"p, port",  "server port",  cxxopts::value<std::string>()->default_value("7110"), "PORT"},
  });

  cxxopts::option help{"h,help", "Help"};

  options.add_options("TEST_GROUP", {
    {"t, test", "test option"},
    help
  });

  Argv argv({
      "test",
      "--address",
      "10.0.0.1",
      "-p",
      "8000",
      "-t",
    });
  auto** actual_argv = argv.argv();
  auto argc = argv.argc();
  auto result = options.parse(argc, actual_argv);

  CHECK(options.groups().size() == 2);
  CHECK(result.count("address") == 1);
  CHECK(result.count("port") == 1);
  CHECK(result.count("test") == 1);
  CHECK(result.count("help") == 0);
  CHECK(result["address"].as<std::string>() == "10.0.0.1");
  CHECK(result["port"].as<std::string>() == "8000");
  CHECK(result["test"].as<bool>() == true);
}

TEST_CASE("Option add with add_option(string, Option)", "[options]") {
  cxxopts::options options("Option add with add_option", " - test Option add with add_option(string, Option)");

  cxxopts::option option_1("t,test", "test option", cxxopts::value<int>()->default_value("7"), "TEST");

  options.add_option("", option_1);
  options.add_option("TEST", {"a,aggregate", "test option 2", cxxopts::value<int>(), "AGGREGATE"});

  Argv argv_({
       "test",
       "--test",
       "5",
       "-a",
       "4"
     });
  auto argc = argv_.argc();
  auto** argv = argv_.argv();
  auto result = options.parse(argc, argv);

  CHECK(result.arguments().size()==2);
  CHECK(options.groups().size() == 2);
  CHECK(result.count("address") == 0);
  CHECK(result.count("aggregate") == 1);
  CHECK(result.count("test") == 1);
  CHECK(result["aggregate"].as<int>() == 4);
  CHECK(result["test"].as<int>() == 5);
}

TEST_CASE("Const array", "[const]") {
  const char* const option_list[] = {"empty", "options"};
  cxxopts::options options("Empty options", " - test constness");
  auto result = options.parse(2, option_list);
}

TEST_CASE("Value from ENV variable", "[options]") {
  cxxopts::options options("test", " - if no parameter value is passed");

  options.add_options("", {
    {"b,bar", "bar option", cxxopts::value<int>()->env("CXXOPTS_BAR")},
    {"f,foo", "foo option", cxxopts::value<int>()->env("CXXOPTS_FOO")},
    {"z,baz", "baz option", cxxopts::value<int>()->env("CXXOPTS_BAZ")->default_value("99")},
    {"e,empty", "empty option", cxxopts::value<int>()->env("CXXOPTS_EMPTY")->default_value("1")},
  });

  putenv((char*)"CXXOPTS_FOO=7");
  putenv((char*)"CXXOPTS_BAR=8");
  putenv((char*)"CXXOPTS_BAZ=9");

  const Argv argv({"test", "--foo", "5"});
  const auto result = options.parse(argv.argc(), argv.argv());

  CHECK(result.arguments().size()==1);
  CHECK(options.groups().size() == 1);
  CHECK(result.count("foo") == 1);
  // value passed as an argument should take precedence
  CHECK(result["foo"].as<int>() == 5);
  // from ENV variable
  CHECK(result["bar"].as<int>() == 8);
    // from ENV variable
  CHECK(result["baz"].as<int>() == 9);
  // env not defined, should fallback to default value
  CHECK(result["empty"].as<int>() == 1);
}

using char_pair = std::pair<char, char>;

template <>
struct cxxopts::value_parser<char_pair> {
  static constexpr bool is_container = false;

  void parse(const parse_context&, const std::string& text, char_pair& value) {
    if (text.size() != 3) {
      cxxopts::detail::throw_or_mimic<argument_incorrect_type>(text, "char_pair");
    }
    value.first = text[0];
    value.second = text[2];
  }
};

TEST_CASE("Custom parser", "[parser]") {
  cxxopts::options options("parser", " - test custom parser");
  options.add_options()
    ("f,foo", "foo option", cxxopts::value<std::pair<char,char>>());

  const Argv argv({"test", "--foo", "5=4"});
  const auto result = options.parse(argv.argc(), argv.argv());
  CHECK(result.count("foo") == 1);
  CHECK(result["foo"].as<char_pair>().first == '5');
  CHECK(result["foo"].as<char_pair>().second == '4');
}

TEST_CASE("Custom delimiter", "[parser]") {
  cxxopts::options options("parser", " - test vector of vector");
  options.add_options()
    ("test", "vector input",
      cxxopts::value<std::vector<std::string>>()->delimiter(';'));

  const Argv argv({"test", "--test=a;b;c", "--test=x,y,z"});
  const auto result = options.parse(argv.argc(), argv.argv());
  const auto tests = result["test"].as<std::vector<std::string>>();

  CHECK(result.count("test") == 2);
  CHECK(tests.size() == 4);
  CHECK(tests[0] == "a");
  CHECK(tests[1] == "b");
  CHECK(tests[2] == "c");
  CHECK(tests[3] == "x,y,z");
}

TEST_CASE("Vector of vector", "[parser]") {
  cxxopts::options options("parser", " - test vector of vector");
  options.add_options()
    ("test", "vector input", cxxopts::value<std::vector<std::vector<float>>>());

  const Argv argv({"test", "--test=10.0", "--test=10.0,10.0", "--test=10.0,10.0,10.0"});
  const auto result = options.parse(argv.argc(), argv.argv());
  const auto tests = result["test"].as<std::vector<std::vector<float>>>();

  CHECK(result.count("test") == 3);
  CHECK(tests.size() == 3);
  CHECK(tests[0].size() == 1);
  CHECK(tests[1].size() == 2);
  CHECK(tests[2].size() == 3);
}

TEST_CASE("Long and short options format", "[options]") {
  SECTION("Empty") {
    cxxopts::options options("parser");
    CHECK_THROWS_AS(options.add_options()("", ""),
      cxxopts::invalid_option_format_error&);
  }

  SECTION("Short only") {
    cxxopts::options options("parser");
    options.add_options()("f", "");
  }

  SECTION("Long only") {
    cxxopts::options options("parser");
    options.add_options()("flag", "");
  }

  SECTION("Short and logn") {
    cxxopts::options options("parser");
    options.add_options()("f,flag", "");
  }

  SECTION("Long and short") {
    cxxopts::options options("parser");
    CHECK_THROWS_AS(options.add_options()("flag,f", ""),
      cxxopts::invalid_option_format_error&);
  }

  SECTION("Short with comma") {
    cxxopts::options options("parser");
    options.add_options()("f,", "");
  }

  SECTION("Comma and long") {
    cxxopts::options options("parser");
    CHECK_THROWS_AS(options.add_options()(",flag", ""),
      cxxopts::invalid_option_format_error&);
  }

  SECTION("Long and comma") {
    cxxopts::options options("parser");
    CHECK_THROWS_AS(options.add_options()("flag,", ""),
      cxxopts::invalid_option_format_error&);
  }

  SECTION("Comma only") {
    cxxopts::options options("parser");
    CHECK_THROWS_AS(options.add_options()(",", ""),
      cxxopts::invalid_option_format_error&);
  }
}
