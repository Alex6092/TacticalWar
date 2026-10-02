"""Usage: add_ref.py <project dir name> <referenced project name> [...]"""
import re
import sys

sys.path.insert(0, r'C:\Users\alexg\source\repos\TacticalWar\tools\vsproj')
import vcxproj_edit as v

root = 'C:/Users/alexg/source/repos/TacticalWar/'
project = sys.argv[1]
p = root + project + '/' + project + '.vcxproj'
t, b, c = v.load(p)
for ref in sys.argv[2:]:
    g = re.search(r'<ProjectGuid>(\{[^}]+\})', open(root + ref + '/' + ref + '.vcxproj', encoding='utf-8-sig').read()).group(1)
    t = v.add_reference(t, '..\\' + ref + '\\' + ref + '.vcxproj', g)
    t = v.add_include_dirs(t, ['..\\' + ref])
v.save(p, t, b, c)
print('references added to', project)
