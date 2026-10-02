import sys

sys.path.insert(0, r'C:\Users\alexg\source\repos\TacticalWar\tools\vsproj')
import vcxproj_edit as v

# Usage: add_server_file.py <project.vcxproj> <file.cpp|file.h> ...
proj = sys.argv[1]
files = sys.argv[2:]
t, b, c = v.load(proj)
compiles = [f for f in files if f.endswith('.cpp') and f'Include="{f}"' not in t]
includes = [f for f in files if f.endswith('.h') and f'Include="{f}"' not in t]
t = v.add_items(t, compiles=compiles, includes=includes)
v.save(proj, t, b, c)

fp = proj + '.filters'
t, b, c = v.load(fp)
items = ''.join(f'    <ClCompile Include="{f}" />\n' for f in compiles) + ''.join(f'    <ClInclude Include="{f}" />\n' for f in includes)
if items:
    t = t.replace('</Project>', '  <ItemGroup>\n' + items + '  </ItemGroup>\n</Project>')
v.save(fp, t, b, c)
print('added', compiles + includes)
