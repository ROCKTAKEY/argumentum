﻿// Copyright (c) 2018, 2019 Marko Mahnič
// License: MPL2. See LICENSE in the root of the project.

#include "../src/argparser.h"
#include "../src/helpformatter.h"

#include <algorithm>
#include <gtest/gtest.h>
#include <sstream>

using namespace argparse;

namespace {
static const bool KEEPEMPTY = true;
std::vector<std::string_view> splitLines( std::string_view text, bool keepEmpty = false )
{
   std::vector<std::string_view> output;
   size_t start = 0;
   auto delims = "\n\r";

   auto isWinEol = [&text]( auto pos ) { return text[pos] == '\r' && text[pos + 1] == '\n'; };

   while ( start < text.size() ) {
      const auto stop = text.find_first_of( delims, start );

      if ( keepEmpty || start != stop )
         output.emplace_back( text.substr( start, stop - start ) );

      if ( stop == std::string_view::npos )
         break;

      start = stop + ( isWinEol( stop ) ? 2 : 1 );
   }

   return output;
}

bool strHasText( std::string_view line, std::string_view text )
{
   return line.find( text ) != std::string::npos;
}

bool strHasTexts( std::string_view line, std::vector<std::string_view> texts )
{
   if ( texts.empty() )
      return true;
   auto it = std::begin( texts );
   size_t pos = line.find( *it );
   while ( it != std::end( texts ) && pos != std::string::npos ) {
      if ( ++it != std::end( texts ) )
         pos = line.find( *it, pos + 1 );
   }
   return pos != std::string::npos;
}

TEST( Utility_strHasText, shouldFindTextInString )
{
   auto line = "some short line";
   EXPECT_TRUE( strHasText( line, "some" ) );
   EXPECT_TRUE( strHasText( line, "short" ) );
   EXPECT_TRUE( strHasText( line, "line" ) );
   EXPECT_FALSE( strHasText( line, "long" ) );
}

TEST( Utility_strHasTexts, shouldFindMultipleTextsInString )
{
   auto line = "some short line";
   EXPECT_TRUE( strHasTexts( line, { "some" } ) );
   EXPECT_TRUE( strHasTexts( line, { "some", "short" } ) );
   EXPECT_TRUE( strHasTexts( line, { "some", "line" } ) );
   EXPECT_TRUE( strHasTexts( line, { "line" } ) );
   EXPECT_FALSE( strHasTexts( line, { "long" } ) );
}

TEST( Utility_strHasTexts, shouldFindMultipleTextsInStringInOrder )
{
   auto line = "some short line";
   EXPECT_TRUE( strHasTexts( line, { "some" } ) );
   EXPECT_TRUE( strHasTexts( line, { "some", "short" } ) );
   EXPECT_FALSE( strHasTexts( line, { "short", "some" } ) );
   EXPECT_FALSE( strHasTexts( line, { "line", "line" } ) );
}

}   // namespace

TEST( ArgumentParserHelpTest, shouldAcceptArgumentHelpStrings )
{
   std::string str;
   std::vector<std::string> args;

   auto parser = argument_parser{};
   parser.add_argument( str, "-s" ).nargs( 1 ).help( "some value" );
   parser.add_argument( args, "args" ).minargs( 0 ).help( "some arguments" );

   auto res = parser.describe_argument( "-s" );
   EXPECT_EQ( "-s", res.short_name );
   EXPECT_EQ( "", res.long_name );
   EXPECT_EQ( "some value", res.help );
   EXPECT_FALSE( res.is_positional() );

   res = parser.describe_argument( "args" );
   EXPECT_EQ( "", res.short_name );
   EXPECT_EQ( "args", res.long_name );
   EXPECT_EQ( "some arguments", res.help );
   EXPECT_TRUE( res.is_positional() );

   EXPECT_THROW( parser.describe_argument( "--unknown" ), std::invalid_argument );
}

TEST( ArgumentParserHelpTest, shouldSetProgramName )
{
   auto parser = argument_parser{};
   parser.config().program( "testing-testing" );

   auto& config = parser.getConfig();
   EXPECT_EQ( "testing-testing", config.program );
}

TEST( ArgumentParserHelpTest, shouldSetProgramDescription )
{
   auto parser = argument_parser{};
   parser.config().description( "An example." );

   auto& config = parser.getConfig();
   EXPECT_EQ( "An example.", config.description );
}

TEST( ArgumentParserHelpTest, shouldSetProgramUsage )
{
   auto parser = argument_parser{};
   parser.config().usage( "example [options] [arguments]" );

   auto& config = parser.getConfig();
   EXPECT_EQ( "example [options] [arguments]", config.usage );
}

TEST( ArgumentParserHelpTest, shouldReturnDescriptionsOfAllArguments )
{
   std::string str;
   long depth;
   std::vector<std::string> args;

   auto parser = argument_parser{};
   parser.add_argument( str, "-s" ).nargs( 1 ).help( "some string" );
   parser.add_argument( depth, "-d", "--depth" ).nargs( 1 ).help( "some depth" );
   parser.add_argument( args, "args" ).minargs( 0 ).help( "some arguments" );

   auto descrs = parser.describe_arguments();
   EXPECT_EQ( 3, descrs.size() );
   EXPECT_EQ( 1, std::count_if( std::begin( descrs ), std::end( descrs ), []( auto&& d ) {
      return d.is_positional();
   } ) );
}

namespace {

class TestOptions : public argparse::Options
{
public:
   std::string str;
   long depth;
   long width;
   std::vector<std::string> args;

public:
   void add_arguments( argument_parser& parser ) override
   {
      parser.config()
            .program( "testing-format" )
            .description( "Format testing." )
            .usage( "testing-format [options]" )
            .epilog( "More about testing." );

      parser.add_argument( str, "-s" ).nargs( 1 ).help( "some string" );
      parser.add_argument( depth, "-d", "--depth" ).nargs( 1 ).help( "some depth" );
      parser.add_argument( width, "", "--width" ).nargs( 1 ).help( "some width" );
      parser.add_argument( args, "args" ).minargs( 0 ).help( "some arguments" );
   }
};

template<typename P, typename F>
std::string getTestHelp( P&& parser, F&& formatter )
{
   std::stringstream strout;
   formatter.format( parser, strout );
   return strout.str();
}

std::string getTestHelp()
{
   auto parser = argument_parser{};
   auto pOpt = std::make_shared<TestOptions>();
   parser.add_arguments( pOpt );

   return getTestHelp( parser, HelpFormatter() );
}

TEST( ArgumentParserHelpTest, shouldOutputHelpToStream )
{
   auto help = getTestHelp();

   auto parts = std::vector<std::string>{ "testing-format", "Format testing.",
      "testing-format [options]", "-s", "some string", "-d", "--depth", "some depth", "--width",
      "some width", "args", "some arguments", "More about testing." };

   for ( auto& p : parts )
      EXPECT_TRUE( strHasText( help, p ) ) << "Missing: " << p;
}

TEST( ArgumentParserHelpTest, shouldFormatDescriptionsToTheSameColumn )
{
   int dummy;
   auto parser = argument_parser{};
   parser.add_argument( dummy, "-s", "--parameter" ).nargs( 0 ).help( "some string" );
   parser.add_argument( dummy, "-x", "--parameterX" ).nargs( 0 ).help( "some depth" );
   parser.add_argument( dummy, "-y", "--parameterXX" ).nargs( 0 ).help( "some width" );
   parser.add_argument( dummy, "args" ).nargs( 0 ).help( "some arguments" );

   auto help = getTestHelp( parser, HelpFormatter() );
   auto helpLines = splitLines( help );

   auto parts =
         std::vector<std::string>{ "some string", "some depth", "some width", "some arguments" };

   auto findColumn = [&helpLines]( auto&& text ) -> size_t {
      for ( auto&& l : helpLines ) {
         auto pos = l.find( text );
         if ( pos != std::string::npos )
            return pos;
      }
      return std::string::npos;
   };

   auto column = findColumn( parts[0] );
   ASSERT_NE( std::string::npos, column );
   for ( auto& p : parts )
      EXPECT_EQ( column, findColumn( p ) ) << "Not aligned: " << p;
}
}   // namespace

TEST( ArgumentParserHelpTest, shouldSetHelpEpilog )
{
   auto parser = argument_parser{};
   parser.config().epilog( "This comes after help." );

   auto& config = parser.getConfig();
   EXPECT_EQ( "This comes after help.", config.epilog );
}

TEST( ArgumentParserHelpTest, shouldReformatLongDescriptions )
{
   std::string loremipsum;
   auto parser = argument_parser{};
   parser.add_argument( loremipsum, "--lorem-ipsum" )
         .nargs( 1 )
         .help( "Lorem ipsum dolor sit amet, consectetur adipiscing elit, "
                "sed do eiusmod tempor incididunt ut labore et dolore magna "
                "aliqua. Ut enim ad minim veniam, quis nostrud exercitation "
                "ullamco laboris nisi ut aliquip ex ea commodo consequat." );

   auto formatter = HelpFormatter();
   formatter.setTextWidth( 60 );
   auto help = getTestHelp( parser, formatter );
   auto lines = splitLines( help );

   for ( auto line : lines )
      EXPECT_GE( 60, line.size() );
}

TEST( ArgumentParserHelpTest, shouldLimitTheWidthOfReformattedDescriptions )
{
   std::string loremipsum;
   auto parser = argument_parser{};
   parser.add_argument( loremipsum, "--lorem-ipsum-x-with-a-longer-name" )
         .nargs( 1 )
         .help( "xxxxx xxxxx xxxxx xxx xxxx, xxxxxxxxxxx xxxxxxxxxx xxxx, "
                "xxx xx xxxxxxx xxxxxx xxxxxxxxxx xx xxxxxx xx xxxxxx xxxxx "
                "xxxxxx. xx xxxx xx xxxxx xxxxxx, xxxx xxxxxxx xxxxxxxxxxxx "
                "xxxxxxx xxxxxxx xxxx xx xxxxxxx xx xx xxxxxxx xxxxxxxxx." );

   auto formatter = HelpFormatter();
   formatter.setTextWidth( 60 );
   formatter.setMaxDescriptionIndent( 20 );
   auto help = getTestHelp( parser, formatter );
   auto lines = splitLines( help );

   for ( auto line : lines ) {
      EXPECT_GE( 60, line.size() );
      auto pos = line.find( "xx" );
      if ( pos != std::string::npos ) {
         EXPECT_LE( 20, pos );
         EXPECT_GT( 22, pos );
      }
   }
}

TEST( ArgumentParserHelpTest, shouldKeepSourceParagraphsInDescriptions )
{
   std::string loremipsum;
   auto parser = argument_parser{};
   parser.add_argument( loremipsum, "--paragraph" ).nargs( 1 ).help( "xxxxx.\n\nyyyy" );

   auto formatter = HelpFormatter();
   formatter.setTextWidth( 60 );
   formatter.setMaxDescriptionIndent( 20 );
   auto help = getTestHelp( parser, formatter );
   auto lines = splitLines( help, KEEPEMPTY );

   int lx = -1;
   int ly = -1;
   int i = 0;
   for ( auto line : lines ) {
      if ( strHasText( line, "xxxx" ) )
         lx = i;
      if ( strHasText( line, "yyyy" ) )
         ly = i;
      ++i;
   }

   EXPECT_EQ( ly, lx + 2 );
}

TEST( ArgumentParserHelpTest, shouldDescribeOptionArguments )
{
   std::string str;
   auto parser = argument_parser{};
   parser.add_argument( str, "-a" ).nargs( 2 );
   parser.add_argument( str, "--bees" ).minargs( 1 );
   parser.add_argument( str, "-c" ).minargs( 0 );
   parser.add_argument( str, "-d" ).minargs( 2 );
   parser.add_argument( str, "-e" ).maxargs( 3 );
   parser.add_argument( str, "-f" ).maxargs( 1 );

   auto res = parser.describe_argument( "-a" );
   EXPECT_EQ( "A A", res.arguments );

   res = parser.describe_argument( "--bees" );
   EXPECT_EQ( "BEES [BEES ...]", res.arguments );

   res = parser.describe_argument( "-c" );
   EXPECT_EQ( "[C ...]", res.arguments );

   res = parser.describe_argument( "-d" );
   EXPECT_EQ( "D D [D ...]", res.arguments );

   res = parser.describe_argument( "-e" );
   EXPECT_EQ( "[E {0..3}]", res.arguments );

   res = parser.describe_argument( "-f" );
   EXPECT_EQ( "[F]", res.arguments );
}

TEST( ArgumentParserHelpTest, shouldOutputOptionArguments )
{
   std::string str;
   auto parser = argument_parser{};
   parser.add_argument( str, "--bees" ).minargs( 1 );

   auto formatter = HelpFormatter();
   formatter.setTextWidth( 60 );
   formatter.setMaxDescriptionIndent( 20 );
   auto help = getTestHelp( parser, formatter );
   auto lines = splitLines( help, KEEPEMPTY );

   for ( auto line : lines ) {
      auto optpos = line.find( "--bees" );
      if ( optpos == std::string::npos )
         continue;

      auto argspos = line.find( "BEES [BEES ...]" );
      ASSERT_NE( std::string::npos, argspos );
      EXPECT_LT( optpos, argspos );
   }
}

TEST( ArgumentParserHelpTest, shouldChangeOptionMetavarName )
{
   std::string str;
   auto parser = argument_parser{};
   parser.add_argument( str, "--bees" ).minargs( 1 ).metavar( "WORK" );

   auto formatter = HelpFormatter();
   formatter.setTextWidth( 60 );
   formatter.setMaxDescriptionIndent( 20 );
   auto help = getTestHelp( parser, formatter );
   auto lines = splitLines( help, KEEPEMPTY );

   for ( auto line : lines ) {
      auto optpos = line.find( "--bees" );
      if ( optpos == std::string::npos )
         continue;

      auto argspos = line.find( "WORK [WORK ...]" );
      ASSERT_NE( std::string::npos, argspos );
      EXPECT_LT( optpos, argspos );
   }
}

TEST( ArgumentParserHelpTest, shouldDescribePositionalArguments )
{
   std::string str;
   auto parser = argument_parser{};
   parser.add_argument( str, "a" ).nargs( 2 );
   parser.add_argument( str, "bees" ).minargs( 1 );
   parser.add_argument( str, "c" ).minargs( 0 );
   parser.add_argument( str, "d" ).minargs( 2 );
   parser.add_argument( str, "e" ).maxargs( 3 );
   parser.add_argument( str, "f" ).maxargs( 1 );

   auto res = parser.describe_argument( "a" );
   EXPECT_EQ( "a a", res.arguments );

   res = parser.describe_argument( "bees" );
   EXPECT_EQ( "bees [bees ...]", res.arguments );

   res = parser.describe_argument( "c" );
   EXPECT_EQ( "[c ...]", res.arguments );

   res = parser.describe_argument( "d" );
   EXPECT_EQ( "d d [d ...]", res.arguments );

   res = parser.describe_argument( "e" );
   EXPECT_EQ( "[e {0..3}]", res.arguments );

   res = parser.describe_argument( "f" );
   EXPECT_EQ( "[f]", res.arguments );
}

TEST( ArgumentParserHelpTest, shouldOutputPositionalArguments )
{
   std::string str;
   auto parser = argument_parser{};
   parser.add_argument( str, "aaa" ).nargs( 3 ).help( "Triple a." );

   auto formatter = HelpFormatter();
   formatter.setTextWidth( 60 );
   formatter.setMaxDescriptionIndent( 20 );
   auto help = getTestHelp( parser, formatter );
   auto lines = splitLines( help, KEEPEMPTY );

   bool hasA = false;
   bool hasAA = false;
   for ( auto line : lines ) {
      if ( line.find( "aaa" ) != std::string::npos )
         hasA = true;
      if ( line.find( "aaa aaa" ) != std::string::npos )
         hasAA = true;
   }
   EXPECT_TRUE( hasA );
   EXPECT_FALSE( hasAA );
}

// TODO: The metavar for the positional argument must be the same as the name of
// the option, oterwise we loose the connection between usage and description.
//   a) forbid / ignore metavar()
//   b) also change the name in metavar()
//   c) display name and metavar in description: name(metavar)

TEST( ArgumentParserHelpTest, shouldSplitOptionalAndMandatoryArguments )
{
   int dummy;
   auto parser = argument_parser{};
   parser.add_argument( dummy, "--yes" ).nargs( 0 ).required( true ).help( "req:true" );
   parser.add_argument( dummy, "--no" ).nargs( 0 ).required( false ).help( "req:false" );

   auto help = getTestHelp( parser, HelpFormatter() );
   auto helpLines = splitLines( help );

   bool hasRequired = false;
   bool hasOptional = false;
   enum class EBlock { other, required, optional };
   auto block = EBlock::required;
   std::map<std::string, EBlock> found;

   for ( auto line : helpLines ) {
      if ( strHasText( line, "optional arguments" ) ) {
         hasOptional = true;
         block = EBlock::optional;
      }
      if ( strHasText( line, "required arguments" ) ) {
         hasRequired = true;
         block = EBlock::required;
      }
      for ( auto param : { "--yes", "--no" } ) {
         if ( strHasText( line, param ) )
            found[param] = block;
      }
   }

   EXPECT_TRUE( hasOptional );
   EXPECT_TRUE( hasRequired );
   EXPECT_EQ( EBlock::required, found["--yes"] );
   EXPECT_EQ( EBlock::optional, found["--no"] );
}

TEST( ArgumentParserHelpTest, shouldSortParametersByGroups )
{
   int dummy;
   auto parser = argument_parser{};
   parser.add_argument( dummy, "--no" ).nargs( 0 ).required( false ).help( "default:no" );
   parser.add_argument( dummy, "--yes" ).nargs( 0 ).required( true ).help( "default:yes" );
   parser.add_argument( dummy, "positional" ).nargs( 0 ).help( "default:positional" );
   parser.add_group( "simple" );
   parser.add_argument( dummy, "--first" ).nargs( 0 ).help( "simple:first" );
   parser.add_argument( dummy, "--second" ).nargs( 0 ).help( "simple:second" );
   parser.add_argument( dummy, "simplicity" ).help( "simple:simplicity" );
   parser.add_exclusive_group( "exclusive" );
   parser.add_argument( dummy, "--on" ).nargs( 0 ).help( "exclusive:on" );
   parser.add_argument( dummy, "--off" ).nargs( 0 ).help( "exclusive:off" );
   parser.add_group( "last" );
   parser.add_argument( dummy, "--last" ).nargs( 0 ).help( "last:last" );
   parser.end_group();
   parser.add_argument( dummy, "--maybe" ).nargs( 0 ).required( false ).help( "default:maybe" );

   auto help = getTestHelp( parser, HelpFormatter() );
   auto helpLines = splitLines( help );

   auto opts = std::set<std::string>{ "--no", "--yes", "positional", "--first", "--second",
      "simplicity", "--on", "--off", "--last", "--maybe" };
   std::map<std::string, int> foundOpts;
   int i = 0;
   for ( auto line : helpLines ) {
      for ( auto opt : opts ) {
         if ( strHasText( line, opt ) ) {
            foundOpts[opt] = i;
            opts.erase( opt );
            break;
         }
      }
      ++i;
   }

   EXPECT_EQ( 0, opts.size() );   // all options were found

   // Expected group order: Positional, Required, Optional; by name: Exclusive, Last, Simple
   EXPECT_LT( foundOpts["positional"], foundOpts["--yes"] );
   EXPECT_LT( foundOpts["--yes"], foundOpts["--no"] );
   EXPECT_LT( foundOpts["--yes"], foundOpts["--maybe"] );
   EXPECT_LT( foundOpts["--no"], foundOpts["--off"] );
   EXPECT_LT( foundOpts["--maybe"], foundOpts["--off"] );
   EXPECT_LT( foundOpts["--maybe"], foundOpts["--on"] );
   EXPECT_LT( foundOpts["--on"], foundOpts["--off"] );
   EXPECT_LT( foundOpts["--on"], foundOpts["--last"] );
   EXPECT_LT( foundOpts["--off"], foundOpts["--last"] );
   EXPECT_LT( foundOpts["--last"], foundOpts["simplicity"] );
   EXPECT_LT( foundOpts["simplicity"], foundOpts["--first"] );
   EXPECT_LT( foundOpts["--first"], foundOpts["--second"] );
}

TEST( ArgumentParserHelpTest, shouldOutputGroupTitle )
{
   int dummy;
   auto parser = argument_parser{};
   parser.config().description( "Should output group title." );
   parser.add_argument( dummy, "--default" ).nargs( 0 ).help( "default:default" );
   parser.add_group( "simple" ).title( "Simple group" );
   parser.add_argument( dummy, "--first" ).nargs( 0 ).help( "simple:first" );
   parser.add_argument( dummy, "--second" ).nargs( 0 ).help( "simple:second" );
   parser.add_exclusive_group( "exclusive" ).title( "Exclusive group" );
   parser.add_argument( dummy, "--third" ).nargs( 0 ).help( "exclusive:third" );

   auto help = getTestHelp( parser, HelpFormatter() );
   auto helpLines = splitLines( help, KEEPEMPTY );
   bool hasSimple = false;
   bool hasExclusive = false;
   for ( auto line : helpLines ) {
      if ( strHasText( line, "Simple group:" ) )
         hasSimple = true;
      if ( strHasText( line, "Exclusive group:" ) )
         hasExclusive = true;
   }

   EXPECT_TRUE( hasSimple );
   EXPECT_TRUE( hasExclusive );
}

TEST( ArgumentParserHelpTest, shouldOutputGroupDescription )
{
   int dummy;
   auto parser = argument_parser{};
   parser.config().description( "Should output group description." );
   parser.add_argument( dummy, "--default" ).nargs( 0 ).help( "default:default" );
   parser.add_group( "simple" ).description( "Simple group." );
   parser.add_argument( dummy, "--first" ).nargs( 0 ).help( "simple:first" );
   parser.add_argument( dummy, "--second" ).nargs( 0 ).help( "simple:second" );
   parser.add_exclusive_group( "exclusive" ).description( "Exclusive group." );
   parser.add_argument( dummy, "--third" ).nargs( 0 ).help( "exclusive:third" );

   auto help = getTestHelp( parser, HelpFormatter() );
   auto helpLines = splitLines( help, KEEPEMPTY );
   bool hasSimple = false;
   bool hasExclusive = false;
   for ( auto line : helpLines ) {
      if ( strHasText( line, "Simple group." ) )
         hasSimple = true;
      if ( strHasText( line, "Exclusive group." ) )
         hasExclusive = true;
   }

   EXPECT_TRUE( hasSimple );
   EXPECT_TRUE( hasExclusive );
}

namespace {
struct CmdOneOptions : public argparse::Options
{
   std::optional<std::string> str;
   std::optional<long> count;

   void add_arguments( argument_parser& parser ) override
   {
      parser.add_argument( str, "-s" ).nargs( 1 );
      parser.add_argument( count, "-n" ).nargs( 1 );
   }
};

struct CmdTwoOptions : public argparse::Options
{
   std::optional<std::string> str;
   std::optional<long> count;

   void add_arguments( argument_parser& parser ) override
   {
      parser.add_argument( str, "--string" ).nargs( 1 );
      parser.add_argument( count, "--count" ).nargs( 1 );
   }
};

struct GlobalOptions : public argparse::Options
{
   std::optional<std::string> global;
   void add_arguments( argument_parser& parser ) override
   {
      parser.add_argument( global, "str" ).nargs( 1 ).required( true );
   }
};

struct TestCommandOptions : public argparse::Options
{
   std::shared_ptr<GlobalOptions> pGlobal;
   std::shared_ptr<CmdOneOptions> pCmdOne;
   std::shared_ptr<CmdTwoOptions> pCmdTwo;

   void add_arguments( argument_parser& parser ) override
   {
      auto pGlobal = std::make_shared<GlobalOptions>();
      parser.add_arguments( pGlobal );

      parser.add_command( "cmdone",
                  [&]() {
                     pCmdOne = std::make_shared<CmdOneOptions>();
                     return pCmdOne;
                  } )
            .help( "Command One description." );

      parser.add_command( "cmdtwo",
                  [&]() {
                     pCmdTwo = std::make_shared<CmdTwoOptions>();
                     return pCmdTwo;
                  } )
            .help( "Command Two description." );
   }
};

}   // namespace

TEST( ArgumentParserCommandHelpTest, shouldOutputCommandSummary )
{
   int dummy;
   auto parser = argument_parser{};
   parser.config().on_exit_return();

   parser.add_arguments( std::make_shared<TestCommandOptions>() );

   auto help = getTestHelp( parser, HelpFormatter() );
   auto helpLines = splitLines( help, KEEPEMPTY );
   bool hasOne = false;
   bool hasTwo = false;
   for ( auto line : helpLines ) {
      if ( strHasTexts( line, { "cmdone", "Command One description." } ) )
         hasOne = true;
      if ( strHasTexts( line, { "cmdtwo", "Command Two description." } ) )
         hasTwo = true;
   }

   EXPECT_TRUE( hasOne );
   EXPECT_TRUE( hasTwo );
}

TEST( ArgumentParserCommandHelpTest, shouldPutUngroupedCommandsUnderCommandsTitle )
{
   int dummy;
   auto parser = argument_parser{};
   parser.config().on_exit_return();

   parser.add_arguments( std::make_shared<TestCommandOptions>() );

   auto help = getTestHelp( parser, HelpFormatter() );
   auto helpLines = splitLines( help, KEEPEMPTY );
   int posPositional = -1;
   int posOne = -1;
   int posTwo = -1;
   int posTitle = -1;

   int i = 0;
   for ( auto line : helpLines ) {
      if ( strHasTexts( line, { "cmdone", "Command One description." } ) )
         posOne = i;
      if ( strHasTexts( line, { "cmdtwo", "Command Two description." } ) )
         posTwo = i;
      if ( strHasText( line, "commands:" ) )
         posTitle = i;
      if ( strHasText( line, "positional arguments:" ) )
         posPositional = i;
      ++i;
   }

   EXPECT_LT( -1, posPositional );
   EXPECT_LT( -1, posOne );
   EXPECT_LT( -1, posTwo );
   EXPECT_LT( -1, posTitle );

   EXPECT_LT( posPositional, posTitle );
   EXPECT_LT( posTitle, posOne );
   EXPECT_LT( posTitle, posTwo );
}

TEST( ArgumentParserCommandHelpTest, shouldBuildDefaultUsage )
{
   int dummy;
   auto parser = argument_parser{};
   parser.config().program( "testing" );
   parser.add_argument( dummy, "--default" ).nargs( 0 );

   auto help = getTestHelp( parser, HelpFormatter() );
   auto helpLines = splitLines( help, KEEPEMPTY );

   auto posUsage = -1;
   int i = 0;
   for ( auto line : helpLines ) {
      if ( strHasTexts( line, { "usage:", "testing", "--default" } ) )
         posUsage = i;
      ++i;
   }

   EXPECT_LT( -1, posUsage );
}

TEST( ArgumentParserCommandHelpTest, shouldPutOptionsBeforePositionalInUsage )
{
   int dummy;
   auto parser = argument_parser{};
   parser.config().program( "testing" );
   parser.add_argument( dummy, "positional" ).nargs( 1 );
   parser.add_argument( dummy, "--option" ).nargs( 0 );

   auto help = getTestHelp( parser, HelpFormatter() );
   auto helpLines = splitLines( help, KEEPEMPTY );

   auto posUsage = -1;
   int i = 0;
   for ( auto line : helpLines ) {
      if ( strHasTexts( line, { "usage:", "testing", "--option", "positional" } ) )
         posUsage = i;
      ++i;
   }

   EXPECT_LT( -1, posUsage );
}

TEST( ArgumentParserCommandHelpTest, shouldShowCommandPlaceholderInUsage )
{
   auto parser = argument_parser{};
   parser.config().program( "testing" );

   std::shared_ptr<CmdOneOptions> pCmdOne;
   parser.add_command( "one", [&]() {
      pCmdOne = std::make_shared<CmdOneOptions>();
      return pCmdOne;
   } );

   auto help = getTestHelp( parser, HelpFormatter() );
   auto helpLines = splitLines( help, KEEPEMPTY );

   auto posUsage = -1;
   auto posOne = -1;
   auto posS = -1;
   int i = 0;
   for ( auto line : helpLines ) {
      if ( strHasTexts( line, { "usage:", "testing", "<command> ..." } ) )
         posUsage = i;
      if ( strHasTexts( line, { "usage:", "-s" } ) )
         posS = i;
      if ( strHasTexts( line, { "usage:", "one" } ) )
         posOne = i;
      ++i;
   }

   EXPECT_LT( -1, posUsage );
   EXPECT_EQ( -1, posOne );
   EXPECT_EQ( -1, posS );
}

TEST( ArgumentParserCommandHelpTest, shouldDisplayArgumentCountInUsage )
{
   int dummy;
   auto parser = argument_parser{};
   parser.config().program( "testing" );
   parser.add_argument( dummy, "p" ).nargs( 1 );
   parser.add_argument( dummy, "-o" ).nargs( 0 );
   parser.add_argument( dummy, "-i" ).minargs( 1 );
   parser.add_argument( dummy, "-a" ).maxargs( 2 );

   auto help = getTestHelp( parser, HelpFormatter() );
   auto helpLines = splitLines( help, KEEPEMPTY );

   auto posUsage = -1;
   int i = 0;
   for ( auto line : helpLines ) {
      if ( strHasTexts(
                 line, { "usage:", "testing", "-o", "-i I [I ...]", "-a [A {0..2}]", "p" } ) )
         posUsage = i;
      ++i;
   }

   EXPECT_LT( -1, posUsage );
}

TEST( ArgumentParserCommandHelpTest, shouldDistinguishRequierdOptionsInUsage )
{
   int dummy;
   auto parser = argument_parser{};
   parser.config().program( "testing" );
   parser.add_argument( dummy, "-o" ).nargs( 0 ).required( true );
   parser.add_argument( dummy, "-a" ).maxargs( 2 ).required( false );
   parser.add_argument( dummy, "-n" ).nargs( 0 ).required( false );

   auto help = getTestHelp( parser, HelpFormatter() );
   auto helpLines = splitLines( help, KEEPEMPTY );

   auto posUsage = -1;
   int i = 0;
   for ( auto line : helpLines ) {
      if ( strHasTexts( line, { "usage:", "testing", "-o", "[-a [A {0..2}]]", "[-n]" } ) )
         posUsage = i;
      ++i;
   }

   EXPECT_LT( -1, posUsage );
}

// - argparse/py forbids the use of required keyword with positionals,
//   but positionals can be made optional with nargs='?'
// - we allow required(false) for now
TEST( ArgumentParserCommandHelpTest, shouldDistinguishRequriedPositionalsInUsage )
{
   int dummy;
   auto parser = argument_parser{};
   parser.config().program( "testing" );
   parser.add_argument( dummy, "r" ).nargs( 1 ).required( true );
   parser.add_argument( dummy, "o" ).nargs( 1 ).required( false );
   parser.add_argument( dummy, "x" ).maxargs( 1 ).required( false );

   auto help = getTestHelp( parser, HelpFormatter() );
   auto helpLines = splitLines( help, KEEPEMPTY );

   auto posUsage = -1;
   auto posBad = -1;
   int i = 0;
   for ( auto line : helpLines ) {
      if ( strHasTexts( line, { "usage:", "testing", "r", "[o]", "[x]" } ) )
         posUsage = i;
      if ( strHasTexts( line, { "usage:", "testing", "[[x]]" } ) )
         posBad = i;
      ++i;
   }

   EXPECT_LT( -1, posUsage );
   EXPECT_EQ( -1, posBad );
}
