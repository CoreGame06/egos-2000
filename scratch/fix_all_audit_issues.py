import os
import re

dir_path = '/home/shenmi/egos_book_bilingual'

def fix_file_issues(filepath):
    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()

    # Split into code blocks and normal text
    code_blocks = re.split(r'(```[\s\S]*?```)', content)
    
    new_parts = []
    for part in code_blocks:
        if part.startswith('```'):
            new_parts.append(part)
        else:
            # We split by existing backticks `...`
            subparts = part.split('`')
            for i in range(0, len(subparts), 2):
                chunk = subparts[i]
                
                # Replace words containing underscores with `word` if not inside backticks
                # Match C/asm symbols like kernel_lock, trap_entry, proc_yield, thread_create, etc.
                # Must match word bounds and contain at least one underscore _
                def wrap_sub(m):
                    word = m.group(0)
                    # Don't match hr '---' or markdown tags
                    if set(word) == {'_'}:
                        return word
                    return f'`{word}`'

                chunk = re.sub(r'\b[a-zA-Z0-9]*_[a-zA-Z0-9_]+\b', wrap_sub, chunk)

                # Replace lone C struct / type names without backticks like struct thread, struct grass
                chunk = re.sub(r'\b(struct\s+[a-zA-Z0-9_]+)\b', r'`\1`', chunk)

                subparts[i] = chunk
            
            new_parts.append('`'.join(subparts))

    new_content = ''.join(new_parts)
    
    if new_content != content:
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(new_content)
        print(f"Fixed unquoted symbols in {os.path.basename(filepath)}")

for fn in sorted(os.listdir(dir_path)):
    if fn.endswith('.md'):
        fix_file_issues(os.path.join(dir_path, fn))
