# Copyright (c) 2024 dylanhook. All rights reserved.
# Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

import argparse
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SRC = os.path.join(ROOT, "src")
FILE_LIST = os.path.join(ROOT, "dh-gui.vcxitems")

SHADER_STAGES = {"_vs": "Vertex", "_ps": "Pixel"}

HEADER = """<?xml version="1.0" encoding="utf-8"?>
<Project xmlns="http://schemas.microsoft.com/developer/msbuild/2003">

  <PropertyGroup>
    <DhGuiSourceRoot>$(MSBuildThisFileDirectory)src</DhGuiSourceRoot>

    <DhGuiShaderOutput>$(IntDir)dh_gui_shaders\\</DhGuiShaderOutput>
  </PropertyGroup>
  <ItemDefinitionGroup>
    <ClCompile>
      <AdditionalIncludeDirectories>$(DhGuiSourceRoot);$(DhGuiShaderOutput);%(AdditionalIncludeDirectories)</AdditionalIncludeDirectories>
    </ClCompile>
  </ItemDefinitionGroup>
"""

SHADER_ITEM = """    <FxCompile Include="$(DhGuiSourceRoot)\\{path}">
      <ShaderType>{stage}</ShaderType>
      <ShaderModel>5.0</ShaderModel>
      <EntryPointName>main</EntryPointName>
      <HeaderFileOutput>$(DhGuiShaderOutput)%(Filename).h</HeaderFileOutput>
      <VariableName>g_%(Filename)_bytecode</VariableName>
      <ObjectFileOutput></ObjectFileOutput>
      <OptimizationLevel>Level3</OptimizationLevel>
      <TreatWarningAsError>true</TreatWarningAsError>
    </FxCompile>
"""

def source_files(suffix):
    found = []
    for directory, _, names in os.walk(SRC):
        for name in names:
            if name.endswith(suffix):
                found.append(os.path.relpath(os.path.join(directory, name), SRC).replace("/", "\\"))
    return sorted(found)

def shader_stage(path):
    stem = os.path.splitext(os.path.basename(path))[0]
    for suffix, stage in SHADER_STAGES.items():
        if stem.endswith(suffix):
            return stage
    return None

def build_file_list():
    lines = [HEADER, "  <ItemGroup>\n"]
    for path in source_files(".cpp"):
        lines.append('    <ClCompile Include="$(DhGuiSourceRoot)\\{}" />\n'.format(path))
    lines.append("  </ItemGroup>\n  <ItemGroup>\n")
    invalid_shaders = []
    for path in source_files(".hlsl"):
        stage = shader_stage(path)
        if stage is None:
            invalid_shaders.append(path)
            continue
        lines.append(SHADER_ITEM.format(path=path, stage=stage))
    lines.append("  </ItemGroup>\n</Project>\n")
    return "".join(lines), invalid_shaders

def main():
    parser = argparse.ArgumentParser(description="check the source file list")
    parser.add_argument("--write", action="store_true", help="update dh-gui.vcxitems")
    args = parser.parse_args()

    expected, invalid_shaders = build_file_list()
    if invalid_shaders:
        for path in invalid_shaders:
            print("rename {} to end in _vs.hlsl or _ps.hlsl".format(path))
        return 1

    if args.write:
        with open(FILE_LIST, "w", encoding="utf-8", newline="\r\n") as handle:
            handle.write(expected)
        print("updated dh-gui.vcxitems")
        return 0

    try:
        with open(FILE_LIST, encoding="utf-8") as handle:
            actual = handle.read()
    except FileNotFoundError:
        print("dh-gui.vcxitems is missing, run this script with --write")
        return 1
    if actual != expected:
        print("dh-gui.vcxitems needs updating, run this script with --write")
        return 1
    print("source file list is up to date")
    return 0

if __name__ == "__main__":
    sys.exit(main())
