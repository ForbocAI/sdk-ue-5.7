import os
import re

import os

# Resolve path relative to this script
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
SOURCE_ROOT = os.path.abspath(os.path.join(SCRIPT_DIR, "../Source/ForbocAI_SDK"))

def process_file(filepath):
    with open(filepath, 'r') as f:
        content = f.read()

    orig_content = content

    # 1. ValidationPipeline
    # Change ValidationPipeline<...>() to validation::create<...>()
    content = re.sub(r'ValidationPipeline<([^>]+)>(\s*)\(\)', r'func::validation::create<\1>()', content)
    # Change .add(...) to pipeline | func::validation::add(...) if we use operator|
    # But C++ doesn't have operator| for arbitrary functions unless we define one.
    # Let's just use func::validation::add(..., pipeline)

    # Actually, a much easier approach for C++ is:
    pass

def process_all():
    for root, dirs, files in os.walk(SOURCE_ROOT):
        for file in files:
            if file.endswith('.cpp') or file.endswith('.h') or file.endswith('.hpp'):
                process_file(os.path.join(root, file))

process_all()
