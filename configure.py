#!/usr/bin/python3
# File: configure.py
# Part of cards <github.com/rmkrupp/hash>
#
# Copyright (C) 2024 Noah Santer <n.ed.santer@gmail.com>
# Copyright (C) 2024 Rebecca Krupp <beka.krupp@gmail.com>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

import argparse
import os
import sys
from datetime import datetime
import misc.ninja_syntax as ninja

#
# ARGUMENT PARSING
#

parser = argparse.ArgumentParser(
        prog = 'configure.py',
        description = 'generate the build.ninja file',
        epilog = 'This program is part of hash <github.com/rmkrupp/hash>'
    )

parser.add_argument('--cflags', help='override compiler flags')
parser.add_argument('--cc', help='override cc')
parser.add_argument('--ar', help='override ar')
parser.add_argument('--ldflags', help='override compiler flags when linking')
#parser.add_argument('--prefix')
#parser.add_argument('--destdir')
parser.add_argument('--build',
                    choices=['release', 'debug', 'w64'], default='debug',
                    help='set the build type (default: debug)')
parser.add_argument('--build-native',
                    choices=['none', 'mtune', 'march', 'both'], default='none',
                    help='build with mtune=native or march=native')
parser.add_argument('--O3', '--o3', action='store_true',
                    help='build releases with -O3')
parser.add_argument('--disable-static-library', action='store_true',
                    help='don\'t build hash.a')
parser.add_argument('--disable-tool', action='append', default=[],
                    choices=[],
                    help='don\'t build a specific tool')
parser.add_argument('--enable-hash-statistics', action='store_true',
                    help='compile with -DHASH_STATISTICS')
parser.add_argument('--disable-hash-warnings', action='store_true',
                    help='compile with -DHASH_NO_WARNINGS')
#parser.add_argument('--disable-argp', action='store_true',
#                    help='fall back to getopt for argument parsing')
parser.add_argument('--disable-sanitize', action='store_true',
                    help='don\'t enable the sanitizer in debug mode')
parser.add_argument('--force-version', metavar='STRING',
                    help='override the version string')
parser.add_argument('--add-version-suffix', metavar='SUFFIX',
                    help='append the version string')

args = parser.parse_args()

#
# HELPER FUNCTIONS
#

def exesuffix(root, enabled):
    if enabled:
        return root + '.exe'
    else:
        return root

def enable_debug():
    w.variable(key = 'std', value = '-std=gnu23')
    w.variable(key = 'cflags', value = '$cflags $sanflags -g -Og')
    if not args.force_version:
        w.variable(key = 'version', value = '"$version"-debug')
    else:
        w.comment('not appending -debug because we were generated with --force-version=')

def enable_release():
    w.variable(key = 'std', value = '-std=gnu23')
    if (args.O3):
        w.comment('setting -O3 because we were generated with --O3')
        w.variable(key = 'cflags', value = '$cflags -O3')
    else:
        w.variable(key = 'cflags', value = '$cflags -O2')
    w.variable(key = 'defines', value = '$defines -DNDEBUG')

def enable_w64():
    args.disable_argp = True
    w.variable(key = 'std', value = '-std=gnu2x')
    w.variable(key = 'cflags', value = '$cflags -O2 -static -I/usr/x86_64-w64-mingw32/include')
    w.variable(key = 'ldflags', value = '$ldflags -L/usr/x86_64-w64-mingw32/lib')
    w.variable(key = 'defines', value = '$defines -DNDEBUG')

def enable_hash_statistics():
    w.variable(key = 'defines', value = '$defines -DHASH_STATISTICS')

def disable_hash_warnings():
    w.variable(key = 'defines', value = '$defines -DHASH_NO_WARNINGS')


#
# THE WRITER
#

w = ninja.Writer(open('build.ninja', 'w'))

#
# PREAMBLE
#

w.comment('we were generated by configure.py on ' + datetime.now().strftime('%d-%m-%y %H:%M:%S'))
w.comment('arguments: ' + str(sys.argv[1:]))
w.newline()

#
# BASE VERSION
#

if args.force_version:
    # this option also disables the -debug suffix in enable_debug()
    # but it does not disable --add_version_suffix
    w.comment('the following version was set at generation by --force-version=' + args.force_version)
    w.variable(key = 'version', value = args.force_version)
else:
    w.variable(key = 'version', value = '$$(git describe --always --dirty)')

w.variable(key = 'builddir', value = 'out')

#
# TOOLS TO INVOKE
#

if 'CC' in os.environ:
    print('WARNING: CC environment variable is set but will be ignored (did you mean --cc=?)',
          file=sys.stderr)

if args.cc:
    if (args.build == 'w64' and args.cc != 'x86_64-w64-mingw32-gcc') or (args.build != 'w64' and args.cc != 'gcc'):
        w.comment('using this cc because we were generated with --cc=' + args.cc)
    w.variable(key = 'cc', value = args.cc)
elif args.build == 'w64':
    w.variable(key = 'cc', value = 'x86_64-w64-mingw32-gcc')
else:
    w.variable(key = 'cc', value = 'gcc')

if args.ar:
    if (args.build == 'w64' and args.ar != 'x86_64-w64-mingw32-gcc-ar') or (args.build != 'w64' and args.cc != 'gcc-ar'):
        w.comment('using this ar because we were generated with --ar=' + args.ar)
    w.variable(key = 'ar', value = args.ar)
elif args.build == 'w64':
    w.variable(key = 'ar', value = 'x86_64-w64-mingw32-gcc-ar')
else:
    w.variable(key = 'ar', value = 'gcc-ar')

#
# CFLAGS/LDFLAGS DEFAULTS
#

if args.cflags:
    w.comment('these are overriden below because we were generated with --cflags=' + args.cflags)
w.variable(key = 'cflags', value = '-Wall -Wextra -Werror -fdiagnostics-color -flto')

if args.ldflags:
    w.comment('these are overriden below because we were generated with --ldflags=' + args.ldflags)
w.variable(key = 'ldflags', value = '')

#
# MTUNE/MARCH SETTINGS
#

if args.build_native == 'none':
    pass
elif args.build_native == 'mtune':
    w.comment('# adding cflags for --build-native=mtune')
    w.variable(key = 'cflags', value = '$cflags -mtune=native')
elif args.build_native == 'march':
    w.comment('# adding cflags for --build-native=march')
    w.variable(key = 'cflags', value = '$cflags -march=native')
elif args.build_native == 'both':
    w.comment('# adding cflags for --build-native=both')
    w.variable(key = 'cflags', value = '$cflags -march=native -mtune=native')
else:
    print('WARNING: unhandled build-native mode "' + args.build_native + '"', file=sys.stderr)
    w.comment('WARNING: unhandled build-native mode "' + args.build_native +'"')

#
# SANITIZER
#

if args.build == 'w64':
    w.comment('-fsanitize disabled for w64 builds')
    w.variable(key = 'sanflags', value = '')
elif args.disable_sanitize:
    w.comment('-fsanitize disabled because we were generated with --disable-sanitize')
    w.variable(key = 'sanflags', value = '')
else:
    w.variable(key = 'sanflags', value = '-fsanitize=address,undefined')

#
# INCLUDES
#

w.variable(key = 'includes', value = '-Iinclude')
w.newline()

#
# BUILD MODE
#

if args.build == 'debug':
    if args.O3:
        print('WARNING: ignoring option --O3 for debug build', file=sys.stderr)
        w.comment('WARNING: ignoring option --O3 for debug build')
    w.comment('build mode: debug')
    enable_debug()
elif args.build == 'release':
    w.comment('build mode: release')
    enable_release()
elif args.build == 'w64':
    w.comment('build mode: w64')
    w.comment('(this implies --disable-argp)')
    enable_w64()
else:
    print('WARNING: unhandled build mode "' + args.build + '"', file=sys.stderr)
    w.comment('WARNING: unhandled build mode "' + args.build +'"')
w.newline()

#
# -DHASH_STATISTICS
#

if args.enable_hash_statistics:
    w.comment('we were generated with --enable-hash-statistics, so do so')
    enable_hash_statistics()
    w.newline()

#
# -DHASH_NO_WARNINGS
#
if args.disable_hash_warnings:
    w.comment('we were generated with --disable-hash-warnings, so do so')
    disable_hash_warnings()
    w.newline()

#
# CFLAGS/LDFLAGS OVERRIDES
#

needs_newline = False

if args.cflags:
    w.variable(key = 'cflags', value = args.cflags)
    needs_newline = True

if args.ldflags:
    w.variable(key = 'ldflags', value = args.ldflags)
    needs_newline = True

#
# OPTIONAL VERSION SUFFIX
#

if args.add_version_suffix:
    w.variable(key = 'version', value = '"$version"-' + args.add_version_suffix)
    needs_newline = True

if needs_newline:
    w.newline()

#
# THE VERSION DEFINE
#

w.variable(key = 'defines', value = '$defines -DVERSION="\\"$version\\""')
w.newline()

#
# NINJA RULES
#

w.rule(
        name = 'cc',
        deps = 'gcc',
        depfile = '$out.d',
        command = '$cc $std $includes -MMD -MF $out.d $defines ' +
                  '$cflags $in -c -o $out'
    )
w.newline()

w.rule(
        name = 'bin',
        deps = 'gcc',
        depfile = '$out.d',
        command = '$cc $std $includes -MMD -MF $out.d $defines ' +
                  '$cflags $in -o $out $ldflags $libs'
    )
w.newline()

w.rule(
        name = 'static-library',
        command = '$ar rcs $out $in $arflags'
    )
w.newline()

#
# SOURCES
#

w.build('$builddir/hash.o', 'cc', 'src/hash.c')
w.build('$builddir/test/test.o', 'cc', 'src/test/test.c')
w.build('$builddir/test/reuse_test.o', 'cc', 'src/test/reuse_test.c')

w.newline()

#
# OUTPUTS
#

all_targets = []
tools_targets = []

def target(name, inputs, targets=[], variables=[], is_disabled=False, why_disabled='', rule='bin'):
    fullname = exesuffix(name, args.build == 'w64')

    if type(is_disabled) == bool:
        is_disabled = [is_disabled]
        why_disabled = [why_disabled]

    assert(len(is_disabled) == len(why_disabled))

    if True not in is_disabled:
        for group in targets:
            group += [fullname]
        w.build(fullname, rule, inputs, variables=variables)
    else:
        if sum([1 for disabled in is_disabled if disabled]) > 1:
            w.comment(fullname + ' is disabled because:')
            for disabled, why in zip(is_disabled, why_disabled):
                if disabled:
                    w.comment(' - ' + why)
        else:
            w.comment(fullname + ' is disabled because ' +
                      [why_disabled[x] for x in range(len(why_disabled))
                       if is_disabled[x]][0])
    w.newline()

target(
        name = 'test',
        inputs = [
            '$builddir/hash.o',
            '$builddir/test/test.o'
        ],
        variables = [('libs', '')],
        is_disabled = 'test' in args.disable_tool,
        why_disabled = 'we were generated with --disable-tool=test',
        targets = [all_targets, tools_targets]
    )

target(
        name = 'reuse_test',
        inputs = [
            '$builddir/hash.o',
            '$builddir/test/reuse_test.o'
        ],
        variables = [('libs', '')],
        is_disabled = 'reuse-test' in args.disable_tool,
        why_disabled = 'we were generated with --disable-tool=reuse_test',
        targets = [all_targets, tools_targets]
    )

target(
        rule = 'static-library',
        name = 'hash.a',
        inputs = [
            '$builddir/hash.o'
        ],
        variables = [('libs', '')],
        is_disabled = args.disable_static_library,
        why_disabled = 'we were generated with --disable-static-library',
        targets = [all_targets]
    )
#
# ALL, TOOLS, AND DEFAULT
#

if len(tools_targets) > 0:
    w.build('tools', 'phony', tools_targets)
else:
    w.comment('NOTE: no tools target because there are no enabled tools')
w.newline()

w.build('all', 'phony', all_targets)
w.newline()

w.default('all')

#
# DONE
#

w.close()
