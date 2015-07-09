/*============================================================================
  CMake - Cross Platform Makefile Generator
  Copyright 2000-2015 Kitware, Inc., Insight Software Consortium

  Distributed under the OSI-approved BSD License (the "License");
  see accompanying file Copyright.txt for details.

  This software is distributed WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the License for more information.
============================================================================*/
#include "cmCommonTargetGenerator.h"

#include "cmComputeLinkInformation.h"
#include "cmGeneratorTarget.h"
#include "cmGlobalCommonGenerator.h"
#include "cmLocalCommonGenerator.h"
#include "cmMakefile.h"
#include "cmSourceFile.h"
#include "cmSystemTools.h"
#include "cmTarget.h"

cmCommonTargetGenerator::cmCommonTargetGenerator(cmGeneratorTarget* gt)
  : GeneratorTarget(gt)
  , Target(gt->Target)
  , Makefile(gt->Makefile)
  , LocalGenerator(static_cast<cmLocalCommonGenerator*>(gt->LocalGenerator))
  , GlobalGenerator(static_cast<cmGlobalCommonGenerator*>(
                      gt->LocalGenerator->GetGlobalGenerator()))
  , ConfigName(LocalGenerator->GetConfigName())
  , ModuleDefinitionFile(GeneratorTarget->GetModuleDefinitionFile(ConfigName))
  , FortranModuleDirectoryComputed(false)
{
}

cmCommonTargetGenerator::~cmCommonTargetGenerator()
{
}

std::string const& cmCommonTargetGenerator::GetConfigName() const
{
  return this->ConfigName;
}

std::string cmCommonTargetGenerator::Convert(
  std::string const& source,
  cmLocalGenerator::RelativeRoot relative,
  cmLocalGenerator::OutputFormat output)
{
  return this->LocalGenerator->Convert(source, relative, output);
}

//----------------------------------------------------------------------------
const char* cmCommonTargetGenerator::GetFeature(const std::string& feature)
{
  return this->GeneratorTarget->GetFeature(feature, this->ConfigName);
}

//----------------------------------------------------------------------------
bool cmCommonTargetGenerator::GetFeatureAsBool(const std::string& feature)
{
  return this->GeneratorTarget->GetFeatureAsBool(feature, this->ConfigName);
}

//----------------------------------------------------------------------------
void cmCommonTargetGenerator::AddFeatureFlags(
  std::string& flags, const std::string& lang
  )
{
  // Add language-specific flags.
  this->LocalGenerator->AddLanguageFlags(flags, lang, this->ConfigName);

  if(this->GetFeatureAsBool("INTERPROCEDURAL_OPTIMIZATION"))
    {
    this->LocalGenerator->AppendFeatureOptions(flags, lang, "IPO");
    }
}

//----------------------------------------------------------------------------
void cmCommonTargetGenerator::AddModuleDefinitionFlag(std::string& flags)
{
  if(this->ModuleDefinitionFile.empty())
    {
    return;
    }

  // TODO: Create a per-language flag variable.
  const char* defFileFlag =
    this->Makefile->GetDefinition("CMAKE_LINK_DEF_FILE_FLAG");
  if(!defFileFlag)
    {
    return;
    }

  // Append the flag and value.  Use ConvertToLinkReference to help
  // vs6's "cl -link" pass it to the linker.
  std::string flag = defFileFlag;
  flag += (this->LocalGenerator->ConvertToLinkReference(
             this->ModuleDefinitionFile));
  this->LocalGenerator->AppendFlags(flags, flag);
}

//----------------------------------------------------------------------------
const char* cmCommonTargetGenerator::GetFortranModuleDirectory()
{
  // Compute the module directory.
  if(!this->FortranModuleDirectoryComputed)
    {
    const char* target_mod_dir =
      this->Target->GetProperty("Fortran_MODULE_DIRECTORY");
    const char* moddir_flag =
      this->Makefile->GetDefinition("CMAKE_Fortran_MODDIR_FLAG");
    if(target_mod_dir && moddir_flag)
      {
      // Compute the full path to the module directory.
      if(cmSystemTools::FileIsFullPath(target_mod_dir))
        {
        // Already a full path.
        this->FortranModuleDirectory = target_mod_dir;
        }
      else
        {
        // Interpret relative to the current output directory.
        this->FortranModuleDirectory =
          this->Makefile->GetCurrentBinaryDirectory();
        this->FortranModuleDirectory += "/";
        this->FortranModuleDirectory += target_mod_dir;
        }

      // Make sure the module output directory exists.
      cmSystemTools::MakeDirectory(this->FortranModuleDirectory.c_str());
      }
    this->FortranModuleDirectoryComputed = true;
    }

  // Return the computed directory.
  if(this->FortranModuleDirectory.empty())
    {
    return 0;
    }
  else
    {
    return this->FortranModuleDirectory.c_str();
    }
}

//----------------------------------------------------------------------------
void cmCommonTargetGenerator::AddFortranFlags(std::string& flags)
{
  // Enable module output if necessary.
  if(const char* modout_flag =
     this->Makefile->GetDefinition("CMAKE_Fortran_MODOUT_FLAG"))
    {
    this->LocalGenerator->AppendFlags(flags, modout_flag);
    }

  // Add a module output directory flag if necessary.
  const char* mod_dir = this->GetFortranModuleDirectory();
  if(!mod_dir)
    {
    mod_dir = this->Makefile->GetDefinition("CMAKE_Fortran_MODDIR_DEFAULT");
    }
  if(mod_dir)
    {
    const char* moddir_flag =
      this->Makefile->GetRequiredDefinition("CMAKE_Fortran_MODDIR_FLAG");
    std::string modflag = moddir_flag;
    modflag += this->Convert(mod_dir,
                             cmLocalGenerator::START_OUTPUT,
                             cmLocalGenerator::SHELL);
    this->LocalGenerator->AppendFlags(flags, modflag);
    }

  // If there is a separate module path flag then duplicate the
  // include path with it.  This compiler does not search the include
  // path for modules.
  if(const char* modpath_flag =
     this->Makefile->GetDefinition("CMAKE_Fortran_MODPATH_FLAG"))
    {
    std::vector<std::string> includes;
    const std::string& config =
      this->Makefile->GetSafeDefinition("CMAKE_BUILD_TYPE");
    this->LocalGenerator->GetIncludeDirectories(includes,
                                                this->GeneratorTarget,
                                                "C", config);
    for(std::vector<std::string>::const_iterator idi = includes.begin();
        idi != includes.end(); ++idi)
      {
      std::string flg = modpath_flag;
      flg += this->Convert(*idi,
                           cmLocalGenerator::NONE,
                           cmLocalGenerator::SHELL);
      this->LocalGenerator->AppendFlags(flags, flg);
      }
    }
}

//----------------------------------------------------------------------------
void
cmCommonTargetGenerator
::AppendFortranFormatFlags(std::string& flags, cmSourceFile const& source)
{
  const char* srcfmt = source.GetProperty("Fortran_FORMAT");
  cmLocalGenerator::FortranFormat format =
    this->LocalGenerator->GetFortranFormat(srcfmt);
  if(format == cmLocalGenerator::FortranFormatNone)
    {
    const char* tgtfmt = this->Target->GetProperty("Fortran_FORMAT");
    format = this->LocalGenerator->GetFortranFormat(tgtfmt);
    }
  const char* var = 0;
  switch (format)
    {
    case cmLocalGenerator::FortranFormatFixed:
      var = "CMAKE_Fortran_FORMAT_FIXED_FLAG"; break;
    case cmLocalGenerator::FortranFormatFree:
      var = "CMAKE_Fortran_FORMAT_FREE_FLAG"; break;
    default: break;
    }
  if(var)
    {
    this->LocalGenerator->AppendFlags(
      flags, this->Makefile->GetDefinition(var));
    }
}

//----------------------------------------------------------------------------
std::string cmCommonTargetGenerator::GetFrameworkFlags(std::string const& l)
{
 if(!this->Makefile->IsOn("APPLE"))
   {
   return std::string();
   }

  std::string fwSearchFlagVar = "CMAKE_" + l + "_FRAMEWORK_SEARCH_FLAG";
  const char* fwSearchFlag =
    this->Makefile->GetDefinition(fwSearchFlagVar);
  if(!(fwSearchFlag && *fwSearchFlag))
    {
    return std::string();
    }

 std::set<std::string> emitted;
#ifdef __APPLE__  /* don't insert this when crosscompiling e.g. to iphone */
  emitted.insert("/System/Library/Frameworks");
#endif
  std::vector<std::string> includes;

  const std::string& config =
    this->Makefile->GetSafeDefinition("CMAKE_BUILD_TYPE");
  this->LocalGenerator->GetIncludeDirectories(includes,
                                              this->GeneratorTarget,
                                              "C", config);
  // check all include directories for frameworks as this
  // will already have added a -F for the framework
  for(std::vector<std::string>::iterator i = includes.begin();
      i != includes.end(); ++i)
    {
    if(this->Target->NameResolvesToFramework(*i))
      {
      std::string frameworkDir = *i;
      frameworkDir += "/../";
      frameworkDir = cmSystemTools::CollapseFullPath(frameworkDir);
      emitted.insert(frameworkDir);
      }
    }

  std::string flags;
  const char* cfg = this->LocalGenerator->GetConfigName().c_str();
  if(cmComputeLinkInformation* cli = this->Target->GetLinkInformation(cfg))
    {
    std::vector<std::string> const& frameworks = cli->GetFrameworkPaths();
    for(std::vector<std::string>::const_iterator i = frameworks.begin();
        i != frameworks.end(); ++i)
      {
      if(emitted.insert(*i).second)
        {
        flags += fwSearchFlag;
        flags += this->LocalGenerator
                     ->ConvertToOutputFormat(*i, cmLocalGenerator::SHELL);
        flags += " ";
        }
      }
    }
  return flags;
}

//----------------------------------------------------------------------------
std::string cmCommonTargetGenerator::GetFlags(const std::string &l)
{
  ByLanguageMap::iterator i = this->FlagsByLanguage.find(l);
  if (i == this->FlagsByLanguage.end())
    {
    std::string flags;
    const char *lang = l.c_str();

    // Add language feature flags.
    this->AddFeatureFlags(flags, lang);

    this->LocalGenerator->AddArchitectureFlags(flags, this->GeneratorTarget,
                                               lang, this->ConfigName);

    // Fortran-specific flags computed for this target.
    if(l == "Fortran")
      {
      this->AddFortranFlags(flags);
      }

    this->LocalGenerator->AddCMP0018Flags(flags, this->Target,
                                          lang, this->ConfigName);

    this->LocalGenerator->AddVisibilityPresetFlags(flags, this->Target,
                                                   lang);

    // Add include directory flags.
    this->AddIncludeFlags(flags, lang);

    // Append old-style preprocessor definition flags.
    this->LocalGenerator->
      AppendFlags(flags, this->Makefile->GetDefineFlags());

    // Add framework directory flags.
    this->LocalGenerator->
      AppendFlags(flags,this->GetFrameworkFlags(l));

    // Add target-specific flags.
    this->LocalGenerator->AddCompileOptions(flags, this->Target,
                                            lang, this->ConfigName);

    ByLanguageMap::value_type entry(l, flags);
    i = this->FlagsByLanguage.insert(entry).first;
    }
  return i->second;
}
