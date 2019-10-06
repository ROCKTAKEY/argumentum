// Copyright (c) 2018, 2019 Marko Mahnič
// License: MPL2. See LICENSE in the root of the project.

#pragma once

#include "convert.h"
#include "helpformatter_i.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace argparse {

class InvalidChoiceError : public std::invalid_argument
{
public:
   InvalidChoiceError( const std::string& value )
      : std::invalid_argument( value )
   {}
};

class ParserTerminated : public std::exception
{
public:
   const std::string arg;
   const int errorCode;

public:
   ParserTerminated( const std::string& arg, int code )
      : arg( arg )
      , errorCode( code )
   {}

   const char* what() const noexcept override
   {
      return "Parsing terminated.";
   }
};

class MixingGroupTypes : public std::runtime_error
{
public:
   MixingGroupTypes( const std::string& groupName )
      : runtime_error( std::string( "Mixing group types in group '" ) + groupName + "'" )
   {}
};

class RequiredExclusiveOption : public std::runtime_error
{
public:
   RequiredExclusiveOption( const std::string& groupName, const std::string& optionName )
      : runtime_error( std::string( "Option '" ) + optionName + "' is required in exclusive group '"
              + groupName + "'" )
   {}
};

class argument_parser;

class Options
{
public:
   virtual void add_arguments( argument_parser& parser ) = 0;
};

class argument_parser
{
public:
   class Value
   {
      int mAssignCount = 0;
      int mOptionAssignCount = 0;
      bool mHasErrors = false;

   public:
      void setValue( const std::string& value )
      {
         ++mAssignCount;
         ++mOptionAssignCount;
         doSetValue( value );
      }

      void markBadArgument()
      {
         // Increase the assign count so that flagValue will not be used.
         ++mOptionAssignCount;
         mHasErrors = true;
      }

      /**
       * The count of assignments through all the options that share this value.
       */
      int getAssignCount() const
      {
         return mAssignCount;
      }

      /**
       * The count of assignments through the current option.
       */
      int getOptionAssignCount() const
      {
         return mOptionAssignCount;
      }

      void onOptionStarted()
      {
         mOptionAssignCount = 0;
      }

      void reset()
      {
         mAssignCount = 0;
         mOptionAssignCount = 0;
         mHasErrors = false;
         doReset();
      }

   protected:
      virtual void doSetValue( const std::string& value ) = 0;
      virtual void doReset()
      {}
   };

   class VoidValue : public Value
   {
   protected:
      void doSetValue( const std::string& value ) override
      {}
   };

   template<typename TValue>
   class ConvertedValue : public Value
   {
   protected:
      using result_t = typename convert_result<TValue>::type;
      using converter_t = std::function<result_t( const std::string& )>;
      TValue& mValue;
      converter_t mConvert = []( const std::string& ) { return {}; };

   public:
      ConvertedValue( TValue& value, converter_t converter )
         : mValue( value )
         , mConvert( converter )
      {}

   protected:
      void doSetValue( const std::string& value ) override
      {
         assign( mValue, value );
      }

      void doReset() override
      {
         mValue = {};
      }

      template<typename TVar>
      void assign( TVar& var, const std::string& value )
      {
         var = mConvert( value );
      }

      template<typename TVar>
      void assign( std::vector<TVar>& var, const std::string& value )
      {
         var.push_back( mConvert( value ) );
      }
   };

   /**
    * The assign-action is executed before @p target.setValue is called.
    *
    * @p target.setValue will be called if assign returns a non-empty value.
    */
   class AssignAction
   {
   public:
      /**
       * Set the the @p value on @p target or return a new string that will be set
       * on @p target the normal way.
       */
      virtual std::optional<std::string> assign( Value& target, const std::string& value )
      {
         return value;
      }
   };

   class OptionGroup
   {
   private:
      std::string mName;
      bool mIsRequired = false;
      bool mIsExclusive = false;

   public:
      OptionGroup( std::string_view name, bool isExclusive )
         : mName( name )
         , mIsExclusive( isExclusive )
      {}

      void setRequired( bool isRequired )
      {
         // The required option can be set only when the group is not yet
         // required.  Because a group can be defined in multiple places, it is
         // required as soon as it is required in one place.
         if ( !mIsRequired )
            mIsRequired = isRequired;
      }

      const std::string& getName() const
      {
         return mName;
      }

      const bool isExclusive() const
      {
         return mIsExclusive;
      }

      bool isRequired() const
      {
         return mIsRequired;
      }
   };

   class GroupConfig
   {
      std::shared_ptr<OptionGroup> mpGroup;

   public:
      GroupConfig( std::shared_ptr<OptionGroup> pGroup )
         : mpGroup( pGroup )
      {}

      GroupConfig& required( bool isRequired = true )
      {
         mpGroup->setRequired( isRequired );
         return *this;
      }
   };

   class Option
   {
   private:
      std::unique_ptr<Value> mpValue;
      std::shared_ptr<AssignAction> mpAssignAction;
      std::string mShortName;
      std::string mLongName;
      std::string mMetavar;
      std::string mHelp;
      std::string mFlagValue = "1";
      std::vector<std::string> mChoices;
      std::shared_ptr<OptionGroup> mpGroup;
      int mMinArgs = 0;
      int mMaxArgs = 0;
      bool mIsRequired = false;
      bool mIsVectorValue = false;

   public:
      template<typename TValue>
      Option( TValue& value )
      {
         if constexpr ( std::is_base_of<Value, TValue>::value ) {
            mpValue = std::make_unique<TValue>( value );
         }
         else {
            using wrap_type = ConvertedValue<TValue>;
            mpValue = std::make_unique<wrap_type>( value, from_string<TValue>::convert );
         }
      }

      template<typename TValue>
      Option( std::vector<TValue>& value )
      {
         using val_vector = std::vector<TValue>;
         if constexpr ( std::is_base_of<Value, TValue>::value ) {
            mpValue = std::make_unique<val_vector>( value );
         }
         else {
            using wrap_type = ConvertedValue<val_vector>;
            mpValue = std::make_unique<wrap_type>( value, from_string<TValue>::convert );
         }

         mIsVectorValue = true;
      }

      void setShortName( std::string_view name )
      {
         mShortName = name;
      }

      void setLongName( std::string_view name )
      {
         mLongName = name;
      }

      void setMetavar( std::string_view varname )
      {
         mMetavar = varname;
      }

      void setHelp( std::string_view help )
      {
         mHelp = help;
      }

      void setNArgs( int count )
      {
         mMinArgs = std::max( 0, count );
         mMaxArgs = mMinArgs;
      }

      void setMinArgs( int count )
      {
         mMinArgs = std::max( 0, count );
         mMaxArgs = -1;
      }

      void setMaxArgs( int count )
      {
         mMinArgs = 0;
         mMaxArgs = std::max( 0, count );
      }

      void setRequired( bool isRequired = true )
      {
         mIsRequired = isRequired;
      }

      void setFlagValue( std::string_view value )
      {
         mFlagValue = value;
      }

      void setChoices( const std::vector<std::string>& choices )
      {
         mChoices = choices;
      }

      void setAction( const std::shared_ptr<AssignAction>& pAction )
      {
         mpAssignAction = pAction;
      }

      void setGroup( const std::shared_ptr<OptionGroup>& pGroup )
      {
         mpGroup = pGroup;
      }

      bool isRequired() const
      {
         return mIsRequired;
      }

      const std::string& getName() const
      {
         return mLongName.empty() ? mShortName : mLongName;
      }

      const std::string& getShortName() const
      {
         return mShortName;
      }

      const std::string& getLongName() const
      {
         return mLongName;
      }

      bool hasName( std::string_view name ) const
      {
         return name == mShortName || name == mLongName;
      }

      const std::string& getRawHelp() const
      {
         return mHelp;
      }

      std::string getMetavar() const
      {
         if ( !mMetavar.empty() )
            return mMetavar;

         auto& name = getName();
         auto pos = name.find_first_not_of( "-" );
         auto metavar = name.substr( pos );
         std::transform( metavar.begin(), metavar.end(), metavar.begin(), toupper );
         return metavar;
      }

      void setValue( const std::string& value )
      {
         if ( !mChoices.empty()
               && std::none_of( mChoices.begin(), mChoices.end(),
                        [&value]( auto v ) { return v == value; } ) ) {
            mpValue->markBadArgument();
            throw InvalidChoiceError( value );
         }

         if ( mpAssignAction ) {
            auto newValue = mpAssignAction->assign( *mpValue, value );
            if ( newValue )
               mpValue->setValue( *newValue );
            return;
         }

         mpValue->setValue( value );
      }

      void resetValue()
      {
         mpValue->reset();
      }

      void onOptionStarted()
      {
         mpValue->onOptionStarted();
      }

      bool acceptsAnyArguments() const
      {
         return mMinArgs > 0 || mMaxArgs != 0;
      }

      bool willAcceptArgument() const
      {
         return mMaxArgs < 0 || mpValue->getOptionAssignCount() < mMaxArgs;
      }

      bool needsMoreArguments() const
      {
         return mpValue->getOptionAssignCount() < mMinArgs;
      }

      bool hasVectorValue() const
      {
         return mIsVectorValue;
      }

      /**
       * @returns true if the value was assigned through any option that shares
       * this option's value.
       */
      bool wasAssigned() const
      {
         return mpValue->getAssignCount() > 0;
      }

      bool wasAssignedThroughThisOption() const
      {
         return mpValue->getOptionAssignCount() > 0;
      }

      const std::string& getFlagValue() const
      {
         return mFlagValue;
      }

      std::tuple<int, int> getArgumentCounts() const
      {
         return std::make_tuple( mMinArgs, mMaxArgs );
      }

      std::shared_ptr<OptionGroup> getGroup() const
      {
         return mpGroup;
      }
   };

   /**
    * OptionConfig is used to configure an option after an option was created with add_argument.
    */
   class OptionConfig
   {
      std::vector<Option>& mOptions;
      size_t mIndex = 0;
      bool mCountWasSet = false;

   public:
      OptionConfig( std::vector<Option>& options, size_t index )
         : mOptions( options )
         , mIndex( index )
      {}

      OptionConfig& setShortName( std::string_view name )
      {
         mOptions[mIndex].setShortName( name );
         return *this;
      }

      OptionConfig& setLongName( std::string_view name )
      {
         mOptions[mIndex].setLongName( name );
         return *this;
      }

      OptionConfig& metavar( std::string_view varname )
      {
         mOptions[mIndex].setMetavar( varname );
         return *this;
      }

      OptionConfig& help( std::string_view help )
      {
         mOptions[mIndex].setHelp( help );
         return *this;
      }

      OptionConfig& nargs( int count )
      {
         ensureCountWasNotSet();
         mOptions[mIndex].setNArgs( count );
         mCountWasSet = true;
         return *this;
      }

      OptionConfig& minargs( int count )
      {
         ensureCountWasNotSet();
         mOptions[mIndex].setMinArgs( count );
         mCountWasSet = true;
         return *this;
      }

      OptionConfig& maxargs( int count )
      {
         ensureCountWasNotSet();
         mOptions[mIndex].setMaxArgs( count );
         mCountWasSet = true;
         return *this;
      }

      OptionConfig& required( bool isRequired = true )
      {
         mOptions[mIndex].setRequired( isRequired );
         return *this;
      }

      OptionConfig& flagValue( std::string_view value )
      {
         mOptions[mIndex].setFlagValue( value );
         return *this;
      }

      OptionConfig& choices( const std::vector<std::string>& choices )
      {
         mOptions[mIndex].setChoices( choices );
         return *this;
      }

      OptionConfig& action( const std::shared_ptr<AssignAction>& pAction )
      {
         mOptions[mIndex].setAction( pAction );
         return *this;
      }

   private:
      void ensureCountWasNotSet() const
      {
         if ( mCountWasSet )
            throw std::invalid_argument( "Only one of nargs, minargs and maxargs can be used." );
      }
   };

   enum EExitMode { EXIT_TERMINATE, EXIT_THROW, EXIT_RETURN };

   class ParserConfig
   {
   public:
      struct Data
      {
         std::string program;
         std::string usage;
         std::string description;
         std::string epilog;
         std::ostream* pOutStream = nullptr;
         EExitMode exitMode = EXIT_TERMINATE;
      };

   private:
      Data mData;

   public:
      const Data& data() const
      {
         return mData;
      }

      ParserConfig& program( std::string_view program )
      {
         mData.program = program;
         return *this;
      }

      ParserConfig& usage( std::string_view usage )
      {
         mData.usage = usage;
         return *this;
      }

      ParserConfig& description( std::string_view description )
      {
         mData.description = description;
         return *this;
      }

      ParserConfig& epilog( std::string_view epilog )
      {
         mData.epilog = epilog;
         return *this;
      }

      // NOTE: The @p stream must outlive the parser.
      ParserConfig& cout( std::ostream& stream )
      {
         mData.pOutStream = &stream;
         return *this;
      }

      ParserConfig& on_exit_terminate()
      {
         mData.exitMode = EXIT_TERMINATE;
         return *this;
      }

      ParserConfig& on_exit_throw()
      {
         mData.exitMode = EXIT_THROW;
         return *this;
      }

      ParserConfig& on_exit_return()
      {
         mData.exitMode = EXIT_RETURN;
         return *this;
      }
   };

   // Errors known by the parser
   enum EError {
      // The option is not known by the argument parser.
      UNKNOWN_OPTION,
      // Multiple options from an exclusive group are present.
      EXCLUSIVE_OPTION,
      // A required option is missing.
      MISSING_OPTION,
      // An option from a required (exclusive) group is missing.
      MISSING_OPTION_GROUP,
      // An required argument is missing.
      MISSING_ARGUMENT,
      // The input argument could not be converted.
      CONVERSION_ERROR,
      // The argument value is not in the set of valid argument values.
      INVALID_CHOICE,
      // Flags do not accept parameters.
      FLAG_PARAMETER,
      // Signal that help was requesetd when on_exit_return is set.
      HELP_REQUESTED
   };

   struct ParseError
   {
      const std::string option;
      const int errorCode;
      ParseError( std::string_view optionName, int code )
         : option( optionName )
         , errorCode( code )
      {}
   };

   struct ParseResult
   {
      std::vector<std::string> ignoredArguments;
      std::vector<ParseError> errors;

   public:
      void clear()
      {
         ignoredArguments.clear();
         errors.clear();
      }
   };

private:
   class Parser
   {
      argument_parser& mArgParser;
      bool mIgnoreOptions = false;
      int mPosition = 0;
      // The active option will receive additional argument(s)
      Option* mpActiveOption = nullptr;
      ParseResult mResult;

   public:
      Parser( argument_parser& argParser )
         : mArgParser( argParser )
      {}

      ParseResult parse( const std::vector<std::string>& args )
      {
         mResult.clear();
         for ( auto& arg : args ) {
            if ( arg == "--" ) {
               mIgnoreOptions = true;
               continue;
            }

            if ( mIgnoreOptions ) {
               addFreeArgument( arg );
               continue;
            }

            auto arg_view = std::string_view( arg );
            if ( arg_view.substr( 0, 2 ) == "--" )
               startOption( arg );
            else if ( arg_view.substr( 0, 1 ) == "-" ) {
               if ( arg.size() == 2 )
                  startOption( arg );
               else {
                  auto opt = std::string{ "--" };
                  for ( int i = 1; i < arg_view.size(); ++i ) {
                     opt[1] = arg_view[i];
                     startOption( opt );
                  }
               }
            }
            else {
               if ( haveActiveOption() ) {
                  auto& option = *mpActiveOption;
                  if ( option.willAcceptArgument() ) {
                     setValue( option, arg );
                     if ( !option.willAcceptArgument() )
                        closeOption();
                  }
               }
               else
                  addFreeArgument( arg );
            }
         }

         if ( haveActiveOption() )
            closeOption();

         return std::move( mResult );
      }

   private:
      void startOption( std::string_view name )
      {
         if ( haveActiveOption() )
            closeOption();

         std::string_view arg;
         auto eqpos = name.find( "=" );
         if ( eqpos != std::string::npos ) {
            arg = name.substr( eqpos + 1 );
            name = name.substr( 0, eqpos );
         }

         auto pOption = findOption( name );
         if ( pOption ) {
            auto& option = *pOption;
            option.onOptionStarted();
            if ( option.willAcceptArgument() )
               mpActiveOption = pOption;
            else
               setValue( option, option.getFlagValue() );

            if ( !arg.empty() ) {
               if ( option.willAcceptArgument() )
                  setValue( option, std::string{ arg } );
               else
                  addError( name, FLAG_PARAMETER );
            }
         }
         else
            addError( name, UNKNOWN_OPTION );
      }

      bool haveActiveOption() const
      {
         return mpActiveOption != nullptr;
      }

      void closeOption()
      {
         if ( haveActiveOption() ) {
            auto& option = *mpActiveOption;
            if ( option.needsMoreArguments() )
               addError( option.getName(), MISSING_ARGUMENT );
            else if ( option.willAcceptArgument() && !option.wasAssignedThroughThisOption() )
               option.setValue( option.getFlagValue() );
         }
         mpActiveOption = nullptr;
      }

      void addFreeArgument( const std::string& arg )
      {
         if ( mPosition < mArgParser.mPositional.size() ) {
            auto& option = mArgParser.mPositional[mPosition];
            if ( option.willAcceptArgument() ) {
               setValue( option, arg );
               return;
            }
            else {
               ++mPosition;
               while ( mPosition < mArgParser.mPositional.size() ) {
                  auto& option = mArgParser.mPositional[mPosition];
                  if ( option.willAcceptArgument() ) {
                     setValue( option, arg );
                     return;
                  }
                  ++mPosition;
               }
            }
         }

         mResult.ignoredArguments.push_back( arg );
      }

      void addError( std::string_view optionName, int errorCode )
      {
         mResult.errors.emplace_back( optionName, errorCode );
      }

      Option* findOption( std::string_view optionName ) const
      {
         for ( auto& option : mArgParser.mOptions )
            if ( option.hasName( optionName ) )
               return &option;

         return nullptr;
      }

      void setValue( Option& option, const std::string& value )
      {
         try {
            option.setValue( value );
         }
         catch ( const InvalidChoiceError& ) {
            addError( option.getName(), INVALID_CHOICE );
         }
         catch ( const std::invalid_argument& ) {
            addError( option.getName(), CONVERSION_ERROR );
         }
         catch ( const std::out_of_range& ) {
            addError( option.getName(), CONVERSION_ERROR );
         }
      }
   };

private:
   ParserConfig mConfig;
   std::vector<Option> mOptions;
   std::vector<Option> mPositional;
   std::set<std::string> mHelpOptionNames;
   std::vector<std::shared_ptr<Options>> mTargets;
   std::map<std::string, std::shared_ptr<OptionGroup>> mGroups;
   std::shared_ptr<OptionGroup> mpActiveGroup;

public:
   /**
    * Get a reference to the parser configuration through which the parser can
    * be configured.
    */
   ParserConfig& config()
   {
      return mConfig;
   }

   /**
    * Get a reference to the parser configuration for inspection.
    */
   const ParserConfig::Data& getConfig() const
   {
      return mConfig.data();
   }

   template<typename TValue, typename = std::enable_if_t<std::is_base_of<Value, TValue>::value>>
   OptionConfig add_argument(
         TValue value, const std::string& name = "", const std::string& altName = "" )
   {
      auto option = Option( value );
      return tryAddArgument( option, { name, altName } );
   }

   /**
    * Add an argument with names @p name and @p altName and store the reference
    * to @p value that will receive the parsed parameter(s).
    */
   template<typename TValue, typename = std::enable_if_t<!std::is_base_of<Value, TValue>::value>>
   OptionConfig add_argument(
         TValue& value, const std::string& name = "", const std::string& altName = "" )
   {
      auto option = Option( value );
      return tryAddArgument( option, { name, altName } );
   }

   /**
    * Add the @p pOptions structure and call its add_arguments method to add
    * the arguments to the parser.  The pointer to @p pOptions is stored in the
    * parser so that the structure outlives the parser.
    */
   void add_arguments( std::shared_ptr<Options> pOptions )
   {
      if ( pOptions ) {
         mTargets.push_back( pOptions );
         pOptions->add_arguments( *this );
      }
   }

   /**
    * Add a special option that will display the help and terminate the parser.
    * If this method is not called, the default help options --help and -h will
    * be used.
    */
   OptionConfig add_help_option()
   {
      return add_help_option( "--help", "-h" );
   }

   OptionConfig add_help_option( const std::string& name )
   {
      return add_help_option( name, "" );
   }

   OptionConfig add_help_option( const std::string& name, const std::string& altName )
   {
      if ( !name.empty() && name[0] != '-' || !altName.empty() && altName[0] != '-' )
         throw std::invalid_argument( "A help argument must be an option." );

      auto value = VoidValue{};
      auto option = Option( value );
      auto optionConfig =
            tryAddArgument( option, { name, altName } ).help( "Print this help message and exit." );

      if ( !name.empty() )
         mHelpOptionNames.insert( name );
      if ( !altName.empty() )
         mHelpOptionNames.insert( altName );

      return optionConfig;
   }

   GroupConfig add_group( const std::string& name )
   {
      auto pGroup = findGroup( name );
      if ( pGroup ) {
         if ( pGroup->isExclusive() )
            throw MixingGroupTypes( name );
         mpActiveGroup = pGroup;
      }
      else
         mpActiveGroup = addGroup( name, false );

      return GroupConfig( mpActiveGroup );
   }

   GroupConfig add_exclusive_group( const std::string& name )
   {
      auto pGroup = findGroup( name );
      if ( pGroup ) {
         if ( !pGroup->isExclusive() )
            throw MixingGroupTypes( name );
         mpActiveGroup = pGroup;
      }
      else
         mpActiveGroup = addGroup( name, true );

      return GroupConfig( mpActiveGroup );
   }

   void end_group()
   {
      mpActiveGroup = nullptr;
   }

   ParseResult parse_args( const std::vector<std::string>& args )
   {
      if ( mHelpOptionNames.empty() )
         add_help_option();

      verifyDefinedOptions();

      for ( auto& option : mOptions )
         option.resetValue();

      for ( auto& option : mPositional )
         option.resetValue();

      for ( auto&& arg : args ) {
         if ( mHelpOptionNames.count( arg ) > 0 ) {
            generate_help();
            return exit_parser( arg, HELP_REQUESTED );
         }
      }

      Parser parser( *this );
      auto result = parser.parse( args );
      reportMissingOptions( result );
      reportExclusiveViolations( result );
      reportMissingGroups( result );
      return result;
   }

   ArgumentHelpResult describe_argument( std::string_view name ) const
   {
      const auto& args = ( name.substr( 0, 1 ) == "-" ) ? mOptions : mPositional;
      for ( auto& opt : args )
         if ( opt.hasName( name ) )
            return describeOption( opt );

      throw std::invalid_argument( "Unknown option" );
   }

   std::vector<ArgumentHelpResult> describe_arguments() const
   {
      std::vector<ArgumentHelpResult> descriptions;

      for ( auto& opt : mOptions )
         descriptions.push_back( describeOption( opt ) );

      for ( auto& opt : mPositional )
         descriptions.push_back( describeOption( opt ) );

      return descriptions;
   }

private:
   void verifyDefinedOptions()
   {
      // A required option can not be in an exclusive group.
      for ( auto& opt : mOptions ) {
         if ( opt.isRequired() ) {
            auto pGroup = opt.getGroup();
            if ( pGroup && pGroup->isExclusive() )
               throw RequiredExclusiveOption( opt.getName(), pGroup->getName() );
         }
      }
   }

   void reportMissingOptions( ParseResult& result )
   {
      for ( auto& option : mOptions )
         if ( option.isRequired() && !option.wasAssigned() )
            result.errors.emplace_back( option.getName(), MISSING_OPTION );

      for ( auto& option : mPositional )
         if ( option.needsMoreArguments() )
            result.errors.emplace_back( option.getName(), MISSING_ARGUMENT );
   }

   void reportExclusiveViolations( ParseResult& result )
   {
      std::map<std::string, std::vector<std::string>> counts;
      for ( auto& option : mOptions ) {
         auto pGroup = option.getGroup();
         if ( pGroup && pGroup->isExclusive() && option.wasAssigned() )
            counts[pGroup->getName()].push_back( option.getName() );
      }

      for ( auto& c : counts )
         if ( c.second.size() > 1 )
            result.errors.emplace_back( c.second.front(), EXCLUSIVE_OPTION );
   }

   void reportMissingGroups( ParseResult& result )
   {
      std::map<std::string, int> counts;
      for ( auto& option : mOptions ) {
         auto pGroup = option.getGroup();
         if ( pGroup && pGroup->isRequired() )
            counts[pGroup->getName()] += option.wasAssigned() ? 1 : 0;
      }

      for ( auto& c : counts )
         if ( c.second < 1 )
            result.errors.emplace_back( c.first, MISSING_OPTION_GROUP );
   }

   OptionConfig tryAddArgument( Option& newOption, std::vector<std::string_view> names )
   {
      // Remove empty names
      auto is_empty = [&]( auto name ) { return name.empty(); };
      names.erase( std::remove_if( names.begin(), names.end(), is_empty ), names.end() );

      if ( names.empty() )
         throw std::invalid_argument( "An argument must have a name." );

      for ( auto& name : names )
         for ( auto ch : name )
            if ( std::isspace( ch ) )
               throw std::invalid_argument( "Argument names must not contain spaces." );

      auto has_dash = []( auto name ) { return name[0] == '-'; };

      auto isOption = [&]( auto names ) -> bool {
         return std::all_of( names.begin(), names.end(), has_dash );
      };

      auto isPositional = [&]( auto names ) -> bool {
         return std::none_of( names.begin(), names.end(), has_dash );
      };

      if ( isPositional( names ) ) {
         mPositional.push_back( std::move( newOption ) );
         auto& option = mPositional.back();
         option.setLongName( names.empty() ? "arg" : names[0] );

         if ( option.hasVectorValue() )
            option.setMinArgs( 0 );
         else
            option.setNArgs( 1 );

         // Positional parameters are required so they can't be in an exclusive
         // group.  We simply ignore them.
         if ( mpActiveGroup && !mpActiveGroup->isExclusive() )
            option.setGroup( mpActiveGroup );

         return { mPositional, mPositional.size() - 1 };
      }
      else if ( isOption( names ) ) {
         mOptions.push_back( std::move( newOption ) );
         auto& option = mOptions.back();

         trySetNames( option, names );

         if ( mpActiveGroup )
            option.setGroup( mpActiveGroup );

         return { mOptions, mOptions.size() - 1 };
      }

      throw std::invalid_argument( "The argument must be either positional or an option." );
   }

   void trySetNames( Option& option, const std::vector<std::string_view>& names ) const
   {
      for ( auto name : names ) {
         if ( name.empty() || name == "-" || name == "--" || name[0] != '-' )
            continue;

         if ( name.substr( 0, 2 ) == "--" )
            option.setLongName( name );
         else if ( name.substr( 0, 1 ) == "-" ) {
            if ( name.size() > 2 )
               throw std::invalid_argument( "Short option name has too many characters." );
            option.setShortName( name );
         }
      }

      if ( option.getName().empty() )
         throw std::invalid_argument( "An option must have a name." );
   }

   std::shared_ptr<OptionGroup> addGroup( std::string name, bool isExclusive )
   {
      std::transform( name.begin(), name.end(), name.begin(), tolower );
      assert( mGroups.count( name ) == 0 );

      auto pGroup = std::make_shared<OptionGroup>( name, isExclusive );
      mGroups[name] = pGroup;
      return pGroup;
   }

   std::shared_ptr<OptionGroup> findGroup( std::string name ) const
   {
      std::transform( name.begin(), name.end(), name.begin(), tolower );
      auto igrp = mGroups.find( name );
      if ( igrp == mGroups.end() )
         return {};
      return igrp->second;
   }

   ArgumentHelpResult describeOption( const Option& option ) const
   {
      ArgumentHelpResult help;
      help.short_name = option.getShortName();
      help.long_name = option.getLongName();
      help.help = option.getRawHelp();
      help.required = option.isRequired();

      if ( option.acceptsAnyArguments() ) {
         const auto& metavar = option.getMetavar();
         auto [mmin, mmax] = option.getArgumentCounts();
         std::string res;
         if ( mmin > 0 ) {
            res = metavar;
            for ( int i = 1; i < mmin; ++i )
               res = res + " " + metavar;
         }
         if ( mmax < mmin ) {
            auto opt = ( res.empty() ? "[" : " [" ) + metavar + " ...]";
            res += opt;
         }
         else if ( mmax - mmin == 1 )
            res += "[" + metavar + "]";
         else if ( mmax > mmin ) {
            auto opt = ( res.empty() ? "[" : " [" ) + metavar + " {0.."
                  + std::to_string( mmax - mmin ) + "}]";
            res += opt;
         }

         help.arguments = std::move( res );
      }

      auto pGroup = option.getGroup();
      if ( pGroup ) {
         help.group.name = pGroup->getName();
         help.group.isExclusive = pGroup->isExclusive();
         help.group.isRequired = pGroup->isRequired();
      }

      return help;
   }

   void generate_help()
   {
      // TODO: The formatter should be configurable
      auto formatter = HelpFormatter();
      auto pStream = getConfig().pOutStream;
      if ( !pStream )
         pStream = &std::cout;

      formatter.format( *this, *pStream );
   }

   ParseResult exit_parser( const std::string& arg, EError errorCode )
   {
      switch ( getConfig().exitMode ) {
         default:
         case EXIT_TERMINATE:
            exit( 0 );
         case EXIT_THROW:
            throw ParserTerminated( arg, errorCode );
         case EXIT_RETURN: {
            ParseResult res;
            res.errors.emplace_back( arg, errorCode );
            return res;
         }
      }

      return {};
   }
};

}   // namespace argparse

#include "helpformatter.h"
