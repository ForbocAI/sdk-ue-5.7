import os
import re

import os

# Resolve path relative to this script
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
SOURCE_ROOT = os.path.abspath(os.path.join(SCRIPT_DIR, "../Source/ForbocAI_SDK"))

for root, _, files in os.walk(SOURCE_ROOT):
    for f in files:
        if f.endswith(('.h', '.hpp', '.cpp')):
            path = os.path.join(root, f)
            with open(path, 'r') as fp:
                c = fp.read()
            if '.setMember(' in c or '.with(' in c or '.createCase(' in c or '.addExtraCase(' in c or 'combineReducers<' in c:
                print(f"Found usages in: {path}")
                # print lines
                lines = c.split('\n')
                for i, l in enumerate(lines):
                    if '.setMember(' in l or '.with(' in l or '.createCase(' in l or '.addExtraCase(' in l or 'combineReducers<' in l:
                        print(f"  {i}: {l.strip()}")

