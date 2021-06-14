#
# Build Version Generator
# Author:  Jared Sanson <jared@jared.geek.nz>
# License: MIT
#
# Computes the current build version based either on the latest git tag + commit, 
# or a pre-defined version specified in JSON.
#
# Version is built into a standalone library as a string that can be 
# referenced by the application.
#
# By compiling & linking instead of passing in as a #define, we avoid
# breaking the incremental compiler each time the version changes.
# (Since modifying a define would force recompilation of all source files)
#
# Example Usage:
# extern const char* GEN_BUILD_VERSION;
# printf("Version: %s\n", GEN_BUILD_VERSION);
#

Import("env")

## This source file defines the base version
## If this doesn't exist, use the latest git tag instead.
ver_source_conf = 'version.json'

## The variable name to export
ver_variable_type = 'const char*'
ver_variable_name = 'GEN_BUILD_VERSION'


import json
import re
from SCons.Script import Import, SConscript, Builder, AlwaysBuild, Action

# Do not run extra script when IDE fetches C/C++ project metadata
from SCons.Script import COMMAND_LINE_TARGETS
if "idedata" in COMMAND_LINE_TARGETS:
    env.Exit(0)

def get_git_commit():
    """
    Get the current commit hash (short)
    Eg. '91ec25f'
    """
    try:
        result = env.backtick('git rev-parse --short HEAD')
        if result:
            return result.strip()
        return None
    except:
        return None

def get_git_desc():
    """
    Use git describe to form version string
    (tag)-(num commits)-g(commit hash)
    Returns SemVer formatted string
    Eg: '1.0.34+91ec25f'
    """
    try:
        result = env.backtick('git describe --tags')
        if result:
            # Eg. 'v1.0-34-g91ec25f'
            match = re.match(r'v?([\w\d\.]+)(?:-(\d+)-g([a-z0-9]+))?', result)
            if match:
                tag    = match.group(1)
                build  = match.group(2)
                commit = match.group(3)

                if not build:
                    build = 0

                if not commit:
                    commit = get_git_commit()

                if commit:
                    # SemVer style versioning (Major.Minor.Patch+Commit)
                    return f'{tag}.{build}+{commit}'

            # Fall back to the raw string if we can't parse it
            return result.strip()
            
        # Fall back to using the commit if tag not available
        return get_git_commit()
    except:
        raise
        return None

def generate_version(source, target, env):
    """
    Calculate the current build version and save it to a C file for compiling
    """
    #src = source[0].get_path()
    src = env.File(ver_source_conf)
    dst = target[0].get_path()

    if src.exists():
        with open(src.get_path(), 'rb') as f:
            ver_conf = json.load(f)
            build = 0
            version = f"{ver_conf['major']}.{ver_conf['minor']}.{build}"

        commit = get_git_commit()
        if commit:
            version += '+' + commit

    else:
        version = get_git_desc()

    # Fallback value if git is not available, or not building out of a git repository
    if not version:
        version = '<dev>'

    print(f"Build Version: {version}")
    
    # Escape string
    version = version.replace('"', '\"')

    # This file will be picked up by the build system since it is specified by targets
    with open(dst, 'wb') as f:
        f.write(bytes(f'{ver_variable_type} {ver_variable_name} = "{version}";', 'utf-8'))

ver_target_c = '$BUILD_DIR/version.c'
ver_target_obj = ver_target_c + '.o'

# Set up targets for version.c
env.Object(target=ver_target_obj, source=ver_target_c)
env.Command(ver_target_c, [], env.VerboseAction(generate_version, "Generate Build Version"), source_scanner=None)
env.AlwaysBuild(ver_target_c)

# Create a library for linking
verlib = env.StaticLibrary(
    '$BUILD_DIR/Version',
    [ver_target_c]
)
env.Append(LIBS=[verlib])
