import os
import re

def fix_all_conflicts(text):
    # Split text into code blocks (```...```) and normal text
    code_parts = re.split(r'(```[\s\S]*?```)', text)
    
    new_parts = []
    for part in code_parts:
        if part.startswith('```'):
            # Code block: leave completely untouched
            new_parts.append(part)
        else:
            # Process normal text outside code blocks
            # We split by backticks `...` so odd parts are already backticked, even parts are plain text
            subparts = part.split('`')
            for i in range(0, len(subparts), 2):
                chunk = subparts[i]

                # 1. Fix angle brackets like <main>, <_start>, <stdio.h>, <terminal_write> which MD parses as HTML tags
                chunk = re.sub(r'<([a-zA-Z0-9_./-]+)>', r'`<\1>`', chunk)

                # 2. Fix C pointers like char*, int*, void*, struct thread*, char** which MD parses as italics
                chunk = re.sub(r'\b(char\s*\*+|int\s*\*+|void\s*\*+|uint[0-9]+_t\s*\*+|int[0-9]+_t\s*\*+|struct\s+[a-zA-Z0-9_]+\s*\*+)\b', r'`\1`', chunk)

                # 3. Fix memory address hex numbers like 0x80000000, 0x80400000, 0x10000000UL
                chunk = re.sub(r'\b(0x[0-9a-fA-F]+(?:UL|L)?)\b', r'`\1`', chunk)

                # 4. Fix memory range brackets like [0x80400000 - 32, 0x80400000) or [0x80000000, 0x82000000)
                chunk = re.sub(r'\[(0x[0-9a-fA-F]+[^\]]+)\]', r'`[\1]`', chunk)

                # 5. Fix lone # at start of line in text that isn't a heading (e.g. #0, #1, #include)
                chunk = re.sub(r'(?m)^(#\d+|#include)', r'`\1`', chunk)

                subparts[i] = chunk
            
            new_parts.append('`'.join(subparts))

    return ''.join(new_parts)

dir_path = '/home/shenmi/egos_book_bilingual'
modified_count = 0

for filename in sorted(os.listdir(dir_path)):
    if filename.endswith('.md'):
        filepath = os.path.join(dir_path, filename)
        with open(filepath, 'r', encoding='utf-8') as f:
            content = f.read()
        
        fixed = fix_all_conflicts(content)
        
        if fixed != content:
            with open(filepath, 'w', encoding='utf-8') as f:
                f.write(fixed)
            print(f"Fixed conflicts in: {filename}")
            modified_count += 1
        else:
            print(f"No conflicts found in: {filename}")

print(f"Scan complete. Modified {modified_count} files.")
