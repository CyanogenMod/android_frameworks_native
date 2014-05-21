#!/usr/bin/env python
#
# Copyright 2014 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

from __future__ import print_function
from operator import itemgetter
import collections
import os.path
import re
import sys


# Avoid endlessly adding to the path if this module is imported multiple
# times, e.g. in an interactive session
regpath = os.path.join(sys.path[0], "registry")
if sys.path[1] != regpath:
    sys.path.insert(1, regpath)
import reg


def nonestr(s):
    return s if s else ""


def parseTypedName(elem):
    type = [nonestr(elem.text)]
    name = None
    for subelem in elem:
        text = nonestr(subelem.text)
        tail = nonestr(subelem.tail)
        if subelem.tag == 'name':
            name = text
            break
        else:
            type.extend([text, tail])
    return (''.join(type).strip(), name)


# Format a list of (type, name) tuples as a C-style parameter list
def fmtParams(params):
    if not params:
        return 'void'
    return ', '.join(['%s %s' % (p[0], p[1]) for p in params])

# Format a list of (type, name) tuples as a C-style argument list
def fmtArgs(params):
    return ', '.join(p[1] for p in params)

# Format a list of (type, name) tuples as comma-separated '"type", name'
def fmtTypeNameList(params):
    return ', '.join(['"%s", %s' % (p[0], p[1]) for p in params])


def overrideSymbolName(sym):
    # The wrapper intercepts glGetString and (sometimes) calls the generated
    # __glGetString thunk which dispatches to the driver's glGetString
    if sym == 'glGetString':
        return '__glGetString'
    else:
        return sym


# Generate API trampoline templates:
#   <rtype> API_ENTRY(<name>)(<params>) {
#       CALL_GL_API(<name>, <args>);
#       // or
#       CALL_GL_API_RETURN(<name>, <args>);
#   }
class TrampolineGen(reg.OutputGenerator):
    def __init__(self):
        reg.OutputGenerator.__init__(self, sys.stderr, sys.stderr, None)

    def genCmd(self, cmd, name):
        reg.OutputGenerator.genCmd(self, cmd, name)

        rtype, fname = parseTypedName(cmd.elem.find('proto'))
        params = [parseTypedName(p) for p in cmd.elem.findall('param')]

        call = 'CALL_GL_API' if rtype == 'void' else 'CALL_GL_API_RETURN'
        print('%s API_ENTRY(%s)(%s) {\n'
              '    %s(%s%s%s);\n'
              '}'
              % (rtype, overrideSymbolName(fname), fmtParams(params),
                 call, fname,
                 ', ' if len(params) > 0 else '',
                 fmtArgs(params)),
              file=self.outFile)



# Collect all API prototypes across all families, remove duplicates,
# emit to entries.in and trace.in files.
class ApiGenerator(reg.OutputGenerator):
    def __init__(self):
        reg.OutputGenerator.__init__(self, sys.stderr, sys.stderr, None)
        self.cmds = []
        self.enums = collections.OrderedDict()

    def genCmd(self, cmd, name):
        reg.OutputGenerator.genCmd(self, cmd, name)
        rtype, fname = parseTypedName(cmd.elem.find('proto'))
        params = [parseTypedName(p) for p in cmd.elem.findall('param')]
        self.cmds.append({'rtype': rtype, 'name': fname, 'params': params})

    def genEnum(self, enuminfo, name):
        reg.OutputGenerator.genEnum(self, enuminfo, name)
        value = enuminfo.elem.get('value')

        # Skip bitmask enums. Pattern matches:
        # - GL_DEPTH_BUFFER_BIT
        # - GL_MAP_INVALIDATE_BUFFER_BIT_EXT
        # - GL_COLOR_BUFFER_BIT1_QCOM
        # but not
        # - GL_DEPTH_BITS
        # - GL_QUERY_COUNTER_BITS_EXT
        #
        # TODO: Assuming a naming pattern and using a regex is what the
        # old glenumsgen script did. But the registry XML knows which enums are
        # parts of bitmask groups, so we should just use that. I'm not sure how
        # to get the information out though, and it's not critical right now,
        # so leaving for later.
        if re.search('_BIT($|\d*_)', name):
            return

        # Skip non-hex values (GL_TRUE, GL_FALSE, header guard junk)
        if not re.search('0x[0-9A-Fa-f]+', value):
            return

        # Append 'u' or 'ull' type suffix if present
        type = enuminfo.elem.get('type')
        if type and type != 'i':
            value += type

        if value not in self.enums:
            self.enums[value] = name

    def finish(self):
        # sort by function name, remove duplicates
        self.cmds.sort(key=itemgetter('name'))
        cmds = []
        for cmd in self.cmds:
            if len(cmds) == 0 or cmd != cmds[-1]:
                cmds.append(cmd)
        self.cmds = cmds

    # Write entries.in
    def writeEntries(self, outfile):
        for cmd in self.cmds:
            print('GL_ENTRY(%s, %s, %s)'
                  % (cmd['rtype'], cmd['name'], fmtParams(cmd['params'])),
                  file=outfile)

    # Write traces.in
    def writeTrace(self, outfile):
        for cmd in self.cmds:
            if cmd['rtype'] == 'void':
                ret = '_VOID('
            else:
                ret = '(%s, ' % cmd['rtype']

            params = cmd['params']
            if len(params) > 0:
                typeNameList = ', ' + fmtTypeNameList(params)
            else:
                typeNameList = ''

            print('TRACE_GL%s%s, (%s), (%s), %d%s)'
                  % (ret, cmd['name'],
                     fmtParams(params), fmtArgs(params),
                     len(params), typeNameList),
                  file=outfile)

    # Write enums.in
    def writeEnums(self, outfile):
        for enum in self.enums.iteritems():
            print('GL_ENUM(%s,%s)' % (enum[0], enum[1]), file=outfile)


if __name__ == '__main__':
    registry = reg.Registry()
    registry.loadFile('registry/gl.xml')

    registry.setGenerator(TrampolineGen())
    TRAMPOLINE_OPTIONS = [
        reg.GeneratorOptions(
            apiname             = 'gles1',
            profile             = 'common',
            filename            = '../../libs/GLES_CM/gl_api.in'),
        reg.GeneratorOptions(
            apiname             = 'gles1',
            profile             = 'common',
            emitversions        = None,
            defaultExtensions   = 'gles1',
            filename            = '../../libs/GLES_CM/glext_api.in'),
        reg.GeneratorOptions(
            apiname             = 'gles2',
            versions            = '(2|3)\.0',
            profile             = 'common',
            filename            = '../../libs/GLES2/gl2_api.in'),
        reg.GeneratorOptions(
            apiname             = 'gles2',
            versions            = '(2|3)\.0',
            profile             = 'common',
            emitversions        = None,
            defaultExtensions   = 'gles2',
            filename            = '../../libs/GLES2/gl2ext_api.in')]
    for opts in TRAMPOLINE_OPTIONS:
        registry.apiGen(opts)

    apigen = ApiGenerator()
    registry.setGenerator(apigen)
    API_OPTIONS = [
        # Generate non-extension versions of each API first, then extensions,
        # so that if an extension enum was later standardized, we see the non-
        # suffixed version first.
        reg.GeneratorOptions(
            apiname             = 'gles1',
            profile             = 'common'),
        reg.GeneratorOptions(
            apiname             = 'gles2',
            versions            = '2\.0|3\.0',
            profile             = 'common'),
        reg.GeneratorOptions(
            apiname             = 'gles1',
            profile             = 'common',
            emitversions        = None,
            defaultExtensions   = 'gles1'),
        reg.GeneratorOptions(
            apiname             = 'gles2',
            versions            = '2\.0|3\.0',
            profile             = 'common',
            emitversions        = None,
            defaultExtensions   = 'gles2')]
    for opts in API_OPTIONS:
        registry.apiGen(opts)
    apigen.finish()
    with open('../../libs/entries.in', 'w') as f:
        apigen.writeEntries(f)
    with open('../../libs/trace.in', 'w') as f:
        apigen.writeTrace(f)
    with open('../../libs/enums.in', 'w') as f:
        apigen.writeEnums(f)
