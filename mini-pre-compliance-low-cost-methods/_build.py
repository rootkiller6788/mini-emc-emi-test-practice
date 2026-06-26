#!/usr/bin/env python3
"""Build all source files for mini-pre-compliance-low-cost-methods module."""
import os

def writef(path, content):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'w', encoding='utf-8') as f:
        f.write(content)
    lines = content.count('\n')
    print(f"  {path}: {lines} lines")

BASE = "."

# We construct files one by one. The already-written core.h has 250 lines.
# We write all other files below.
