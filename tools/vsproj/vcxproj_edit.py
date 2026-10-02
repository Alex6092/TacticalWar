"""Small helpers to edit existing (hand-made) .vcxproj files."""
import re
import sys


def load(path):
    raw = open(path, 'rb').read()
    bom = raw.startswith(b'\xef\xbb\xbf')
    text = raw.decode('utf-8-sig')
    crlf = '\r\n' in text
    return text.replace('\r\n', '\n'), bom, crlf


def save(path, text, bom, crlf):
    if crlf:
        text = text.replace('\n', '\r\n')
    open(path, 'wb').write((b'\xef\xbb\xbf' if bom else b'') + text.encode('utf-8'))


def remove_items(text, files):
    for f in files:
        text, _ = re.subn(r'\s*<Cl(?:Compile|Include) Include="' + re.escape(f) + r'" />', '', text)
    return text


def add_items(text, compiles=(), includes=()):
    if compiles:
        block = ''.join(f'    <ClCompile Include="{c}" />\n' for c in compiles)
        text = text.replace('  <ItemGroup>\n    <ClCompile Include=', '  <ItemGroup>\n' + block + '    <ClCompile Include=', 1)
    if includes:
        block = ''.join(f'    <ClInclude Include="{c}" />\n' for c in includes)
        text = text.replace('  <ItemGroup>\n    <ClInclude Include=', '  <ItemGroup>\n' + block + '    <ClInclude Include=', 1)
    return text


def add_reference(text, project_rel_path, guid):
    if project_rel_path in text:
        return text
    item = (f'    <ProjectReference Include="{project_rel_path}">\n'
            f'      <Project>{guid.lower()}</Project>\n    </ProjectReference>\n')
    return text.replace('  <ItemGroup>\n    <ProjectReference Include=', '  <ItemGroup>\n' + item + '    <ProjectReference Include=', 1)


def add_include_dirs(text, dirs):
    prefix = ';'.join(dirs) + ';'
    return re.sub(r'<AdditionalIncludeDirectories>(?!' + re.escape(prefix) + ')',
                  lambda m: '<AdditionalIncludeDirectories>' + prefix, text)


def add_compile_option(text, option):
    """Add an option to every ClCompile block of the configuration ItemDefinitionGroups."""
    def repl(match):
        block = match.group(0)
        if option in block:
            return block
        if '<AdditionalOptions>' in block:
            return block.replace('<AdditionalOptions>', f'<AdditionalOptions>{option} ', 1)
        return block.replace('</ClCompile>', f'  <AdditionalOptions>{option} %(AdditionalOptions)</AdditionalOptions>\n    </ClCompile>', 1)
    return re.sub(r'<ItemDefinitionGroup[^>]*>\s*<ClCompile>.*?</ClCompile>', repl, text, flags=re.S)
