[![Build Status](https://github.com/artpaul/cxxopts/actions/workflows/cmake.yml/badge.svg)](https://github.com/artpaul/cxxopts/actions/workflows/cmake.yml)

# About

This is a lightweight C++ option parser library, supporting the standard GNU
style syntax for options.

# Features

Below are a few of the features which cxxopts supports:
* **Flags / Switches** (i.e. bool fields)
  - Both short and long versions supported (i.e. `-f` and `--flag` respectively)
  - Supports combining short versions (i.e. `-fBgoZ` is the same as `-f -B -g -o -Z`)
  - Supports multiple occurrences (i.e. `-vvv` or `-v -v -v`)
  - Supports `-?`
* **Positional Arguments**
  - Supports the Unix `--` meaning, only positional arguments follow
  - Supports partial parsing (i.e. `cmd <options> sub-command <options> <positional>`)
* **Option Arguments** (i.e. those that take values)
  - Both short and long versions supported (i.e. `-o value`, `-ovalue` and `--option value` or `--option=value` respectively)
  - Supports multiple values (i.e. `-o <val1> -o <val2>`)
  - Supports delimited values (i.e. `--option=val1,val2,val3`)
  - Supports custom parsers
  - Supports vector of vector values (i.e `--opt=1,2,3 --opt=4,5,6`)
* **Groups**
  - Arguments can be made part of a group for the purposes of displaying help messages
* **Default Values**
  - Supports default value from ENV variable
* **Requirements**
  - No external dependencies and only C++ 11 compatible compiler is required

# Quick start

Options can be given as:

    --long
    --long=<argument>
    --long <argument>
    -a
    -ab
    -abc <argument>
    -c<argument>

where `-c` takes an argument, but `-a` and `-b` do not.

Additionally, anything after `--` will be parsed as a positional argument.

## Basics

```cpp
#include <cxxopts.hpp>
```

Create a `cxxopts::options` instance.

```cpp
cxxopts::options options("MyProgram", "One line description of MyProgram");
```

Then use `add_options`.

```cpp
options.add_options()
  ("d,debug", "Enable debugging") // a bool parameter
  ("i,integer", "Int param", cxxopts::value<int>())
  ("f,file", "File name", cxxopts::value<std::string>())
  ("v,verbose", "Verbose output", cxxopts::value<bool>()->default_value("false"))
  ;
```

Options are declared with a long and an optional short option. A description
must be provided. The third argument is the value, if omitted it is boolean.
Any type can be given as long as it can be parsed, with operator>>.

To parse the command line do:

```cpp
auto result = options.parse(argc, argv);
```

To retrieve an option use `result.count("option")` to get the number of times
it appeared, and

```cpp
result["opt"].as<type>()
```

to get its value. If "opt" doesn't exist, or isn't of the right type, then an
exception will be thrown.

## Boolean values

Boolean options have a default implicit value of `"true"`, which can be
overridden. The effect is that writing `-o` by itself will set option `o` to
`true`. However, they can also be written with various strings using `=value`.
There is no way to disambiguate positional arguments from the value following
a boolean, so we have chosen that they will be positional arguments, and
therefore, `-o false` does not work.

## Vector values

Parsing of list of values in form of an `std::vector<T>` is also supported, as long as `T`
can be parsed. To separate single values in a list the ',' is used by default.
Ensure that you use no whitespaces between values because those would be interpreted
as the next command-line option. Example for a command-line option
that can be parsed as a `std::vector<double>`:

~~~
--my_list=1,-2.1,3,4.5
~~~

To set up custom separation character (semicolon, for example) use:

```cpp
cxxopts::value<std::vector<std::string>>()->delimiter(';')
```

## Value from ENV variable

When a parameter is not set, a value will be fetched from an environment variable (if such variable is defined).

```cpp
cxxopts::value<int>()->env("MY_VAR")
```

## Default and implicit values

An option can be declared with a default or an implicit value, or both.

A default value is the value that an option takes when it is not specified
on the command line. The following specifies a default value for an option:

```cpp
cxxopts::value<std::string>()->default_value("value")
```

An implicit value is the value that an option takes when it is given on the
command line without an argument. The following specifies an implicit value:

```cpp
cxxopts::value<std::string>()->implicit_value("implicit")
```

If an option had both, then not specifying it would give the value `"value"`,
writing it on the command line as `--option` would give the value `"implicit"`,
and writing `--option=another` would give it the value `"another"`.

Note that the default and implicit value is always stored as a string,
regardless of the type that you want to store it in. It will be parsed as
though it was given on the command line.

## Options specified multiple times

The same option can be specified several times, with different arguments, which will all
be recorded in order of appearance. An example:

~~~
--use train --use bus --use ferry
~~~

this is supported through the use of a vector of value for the option:

~~~
options.add_options()
  ("use", "Usable means of transport", cxxopts::value<std::vector<std::string>>())
~~~

## Positional Arguments

Positional arguments are those given without a preceding flag and can be used
alongside non-positional arguments. There may be multiple positional arguments,
and the final positional argument may be a container type to hold a list of all
remaining positionals.

To set up positional arguments, first declare the options, then configure a
set of those arguments as positional like:

```cpp
options.add_options()
  ("script", "The script file to execute", cxxopts::value<std::string>())
  ("server", "The server to execute on", cxxopts::value<std::string>())
  ("filenames", "The filename(s) to process", cxxopts::value<std::vector<std::string>>());

options.parse_positional("script", "server", "filenames");

// Parse options the usual way
options.parse(argc, argv);
```

For example, parsing the following arguments:
~~~
my_script.py my_server.com file1.txt file2.txt file3.txt
~~~
will result in parsed arguments like the following table:

| Field         | Value                                     |
| ------------- | ----------------------------------------- |
| `"script"`    | `"my_script.py"`                          |
| `"server"`    | `"my_server.com"`                         |
| `"filenames"` | `{"file1.txt", "file2.txt", "file3.txt"}` |

## Unrecognised arguments

You can allow unrecognised arguments to be skipped. This applies to both
positional arguments that are not parsed into another option, and `--`
arguments that do not match an argument that you specify. This is done by
calling:

```cpp
options.allow_unrecognised_options();
```

and in the result object they are retrieved with:

```cpp
result.unmatched()
```

## Exceptions

Exceptional situations throw C++ exceptions. There are two types of
exceptions: errors defining the options and errors when parsing a list of
arguments. All exceptions derive from `cxxopts::option_error`. Errors
defining options derive from `cxxopts::spec_error` and errors
parsing arguments derive from `cxxopts::parse_error`.

All exceptions define a `what()` function to get a printable string
explaining the error.

## Help groups

Options can be placed into groups for the purposes of displaying help messages.
To place options in a group, pass the group as a string to `add_options`. Then,
when displaying the help, pass the groups that you would like displayed as a
vector to the `help` function.

## Custom help

The string after the program name on the first line of the help can be
completely replaced by calling `options.custom_help`. Note that you might
also want to override the positional help by calling `options.positional_help`.

## Custom parsers

```cpp
template <>
struct cxxopts::value_parser<custom_type> {
  using value_type = custom_type;
  /// Is this a container type?
  static constexpr bool is_container = <true> | <false>;

  void parse(const cxxopts::parse_context& ctx, const std::string& text, custom_type& value) {
    // parse value from text here
  }
};
```

# Command / subcommand pattern

In case then a program has some global options and specific sets of options
for each subcommand, the following technique can be used.

Enable stop on the first positional feature:
```cpp
options.stop_on_positional()
```
parse global options:
```cpp
auto result = options.parse(argc, argv);
```
adjust values of argc and argv:
```cpp
argc -= result.consumed();
argv += result.consumed();
```
In the end, argv will point to the name of the subcommand.

# Example

Putting all together:
```cpp
#include <cxxopts.hpp>

int main(int argc, char** argv) {
  cxxopts::options options("test", "A brief description");

  options.add_options()
    ("b,bar", "Param bar", cxxopts::value<std::string>())
    ("d,debug", "Enable debugging", cxxopts::value<bool>()->default_value("false"))
    ("f,foo", "Param foo", cxxopts::value<int>()->default_value("10"))
    ("h,help", "Print usage");

  auto result = options.parse(argc, argv);

  if (result.count("help")) {
    std::cout << options.help() << std::endl;
    exit(0);
  }
  bool debug = result["debug"].as<bool>();
  std::string bar;
  if (result.count("bar"))
    bar = result["bar"].as<std::string>();
  int foo = result["foo"].as<int>();

  return 0;
}
```

## Solving clang-format issue

Currently, there is no way to setup clang-format to format code beautifully with chained call operator. Consider to use initializer_list syntax:
```cpp
options.add_options("group name", {
  {"b,bar", "Param bar", cxxopts::value<std::string>()},
  {"d,debug", "Enable debugging", cxxopts::value<bool>()->default_value("false")},
  {"f,foo", "Param foo", cxxopts::value<int>()->default_value("10")},
  {"h,help", "Print usage"}
});
```

# Linking

This is a header only library.

# Release versions

Note that `master` is generally a work in progress, and you probably want to use a
tagged release version.
