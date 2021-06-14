#
# Secrets Generator
# Author:  Jared Sanson <jared@jared.geek.nz>
# License: MIT
#
# Compiles secrets defined in a JSON file into the application.
# This prevents you from accidentally checking in secrets into git.
#

Import("env")

secrets_source_file = 'secrets.json'
secrets_header_dest = 'src/secrets.h'
gitignore_file      = '.gitignore'
secrets_schema = [
    # Type,         Name,               Default
    ('const char*', 'device_name',      ''),
    ('const char*', 'mqtt_username',    ''),
    ('const char*', 'mqtt_password',    ''),
    ('uint16_t',    'mqtt_port',        1883),
    ('const char*', 'wifi_ssid',        ''),
    ('const char*', 'wifi_pw',          ''),
]

import json
import re
from typing import Type
from SCons.Script import Import, SConscript, Builder, AlwaysBuild, Action

# Do not run extra script when IDE fetches C/C++ project metadata
from SCons.Script import COMMAND_LINE_TARGETS
if "idedata" in COMMAND_LINE_TARGETS:
    env.Exit(0)

def generate_template(source, target, env):
    src = target[0] # secrets.json
    secrets = {}

    # Create blank JSON if not exists
    if not src.exists():
        with open(src.get_path(), 'wb') as f:
            for type,name,default in secrets_schema:
                secrets[name] = default
            f.write(bytes(json.dumps(secrets, indent=4), 'utf-8'))

def load_secrets():
    # Load & validate JSON if exists
    with open(secrets_source_file, 'rb') as f:
        secrets = json.load(f)

        if not isinstance(secrets, dict):
            raise TypeError(f'{secrets_source_file} must be a dict')

        # Validate schema
        for type,name,default in secrets_schema:
            if name not in secrets:
                raise ValueError(f'{secrets_source_file} expects key {name}')

    secrets_types = {}
    for type,name,default in secrets_schema:
        secrets_types[name] = type

    return (secrets, secrets_types)

def generate_secrets(source, target, env):
    """
    Generate a secrets source file that is not part of the repository
    """
    secrets, secrets_types = load_secrets()

    c_lines = []
    c_lines.append(f'// Automatically generated from {secrets_source_file}')
    c_lines.append('#include <stdint.h>')
    c_lines.append('')
    c_lines.append('namespace secrets {')

    h_lines = []
    h_lines.append(f'// Automatically generated from {secrets_source_file}')
    h_lines.append('#pragma once')
    h_lines.append('#include <stdint.h>')
    h_lines.append('')
    h_lines.append('namespace secrets {')

    for name, value in secrets.items():
        #print(f'{name}={value}')

        type = secrets_types.get(name, None)
        if name and type:
            if isinstance(value, str):
                value = '"' + value.replace('"', '\"') + '"'

            c_lines.append(f'    {type} {name} = {value};')
            h_lines.append(f'    extern {type} {name};');

    c_lines.append('}')
    h_lines.append('}')

    # This file will be picked up by the build system since it is specified by targets
    with open(target[0].get_path(), 'wb') as f:
        for ln in c_lines:
            f.write(bytes(ln, 'utf-8'))
            f.write(b'\r\n')

    # Generate a header that can be included by the application
    with open(target[1].get_path(), 'wb') as f:
        for ln in h_lines:
            f.write(bytes(ln, 'utf-8'))
            f.write(b'\r\n')

    # TODO: Keeps duplicating...
    # # Make sure the source JSON is specified in .gitignore
    # with open(gitignore_file, 'r') as f:
    #     lines = f.readlines()
    # if secrets_source_file not in lines:
    #     with open(gitignore_file, 'a') as f:
    #         f.write(secrets_source_file)
    #         f.write('\n')

# Validate JSON against schema
print(f"Validate {secrets_source_file}")
load_secrets()


target_c = '$BUILD_DIR/secrets.cpp'
target_h = secrets_header_dest #'$BUILD_DIR/secrets.h'
target_obj = target_c + '.o'

# Generate secrets.cpp.o from secrets.cpp
env.Object(target=target_obj, source=target_c)

# Generate secrets.json from template if not present
env.Command(
    target=[secrets_source_file], 
    source=[], 
    action=env.VerboseAction(generate_template, "Generate Default Secrets")
)

# Generate secrets.cpp / secrets.h from secrets.json
env.Command(
    target=[target_c, target_h], 
    source=[secrets_source_file], 
    action=env.VerboseAction(generate_secrets, "Generate Secrets")
)

# Ensure secrets.h is generated before attempting to compile source
# by marking it as a dependency for any files that include the header.
src = env.FindSourceFiles()
src = [d for d in src if d.get_path().startswith('src')] # Limit paths to src/
src = [d for d in src if ('"','secrets.h','"') in d.includes]
for d in src:
    #print(f"{d.get_path()} includes secrets.h, adding dependency")
    env.Depends(
        target=f"$BUILD_DIR/{d.get_path()}.o",
        dependency=[target_h]
    )

# Create a library for linking
secretslib = env.StaticLibrary(
    '$BUILD_DIR/Secrets',
    [target_c]
)
env.Prepend(LIBS=[secretslib])
