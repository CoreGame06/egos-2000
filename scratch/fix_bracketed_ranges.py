import os
import re

def fix_ranges_in_file(filepath):
    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()

    # Pattern for range brackets like [`0x80400000` - 32, `0x80400000`) or [0x80000000, 0x80200000)
    # Match starting with '[' followed by text with hex and ending with ')' or ']'
    def range_replacer(match):
        full = match.group(0)
        # Remove inner backticks
        clean = full.replace('`', '')
        return f'`{clean}`'

    # Match [0x... ) or [0x... ] with possible inner backticks
    pattern = r'\[`?0x[0-9a-fA-F]+`?[^\]\)\n]*[`\d\w\s,\-+]*[\)\]]'
    
    # Process outside code blocks
    parts = re.split(r'(```[\s\S]*?```)', content)
    new_parts = []
    
    for part in parts:
        if part.startswith('```'):
            new_parts.append(part)
        else:
            # Replace matching bracketed ranges
            part = re.sub(pattern, range_replacer, part)
            new_parts.append(part)

    new_content = ''.join(new_parts)

    if new_content != content:
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(new_content)
        print(f"Fixed bracket ranges in {filepath}")
    else:
        print(f"No range conflicts in {filepath}")

dir_path = '/home/shenmi/egos_book_bilingual'
for fn in sorted(os.listdir(dir_path)):
    if fn.endswith('.md'):
        fix_ranges_in_file(os.path.join(dir_path, fn))
