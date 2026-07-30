import os
import re

dir_path = '/home/shenmi/egos_book_bilingual'

def audit_file(filepath):
    filename = os.path.basename(filepath)
    with open(filepath, 'r', encoding='utf-8') as f:
        lines = f.readlines()

    in_code_block = False
    issues = []

    for idx, line in enumerate(lines, 1):
        stripped = line.strip()
        
        # Check code block fencing
        if stripped.startswith('```'):
            in_code_block = not in_code_block
            continue

        if in_code_block:
            continue

        # Skip blockquote indicator '> ' for checking
        text_to_check = stripped
        if text_to_check.startswith('>'):
            text_to_check = text_to_check[1:].strip()

        # 1. Check for odd number of backticks in a single line
        backtick_count = text_to_check.count('`')
        if backtick_count % 2 != 0:
            issues.append(f"Line {idx}: Odd backtick count ({backtick_count}): {stripped}")

        # Remove all text inside `...` for checking plain text
        plain = re.sub(r'`[^`]*`', '', text_to_check)

        # 2. Check for underscores in plain text outside backticks
        if '_' in plain:
            # Check if it's an unescaped underscore in plain text (excluding horizontal rule '---' or markdown lists)
            if not plain.startswith('---') and not re.match(r'^\s*[-*_]{3,}\s*$', plain):
                # find words with underscore
                words_with_sub = re.findall(r'\b[a-zA-Z0-9_]*_[a-zA-Z0-9_]*\b', plain)
                if words_with_sub:
                    issues.append(f"Line {idx}: Unquoted underscore symbols {words_with_sub}: {stripped}")

        # 3. Check for unquoted angle brackets <word> outside backticks
        if re.search(r'<[a-zA-Z0-9_./-]+>', plain):
            issues.append(f"Line {idx}: Unquoted angle brackets: {stripped}")

        # 4. Check for unquoted square brackets [text] outside backticks (excluding markdown links if any)
        # Check for unquoted [0x... or memory range
        if re.search(r'\[\s*0x[0-9a-fA-F]+', plain):
            issues.append(f"Line {idx}: Unquoted bracketed hex range: {stripped}")

    if in_code_block:
        issues.append("ERROR: File ends with unclosed ``` code block!")

    return issues

print("=== STARTING STRICT MARKDOWN AUDIT ===")
total_issues = 0
for fn in sorted(os.listdir(dir_path)):
    if fn.endswith('.md'):
        fp = os.path.join(dir_path, fn)
        errs = audit_file(fp)
        if errs:
            print(f"\n❌ [{fn}] Found {len(errs)} potential issues:")
            for e in errs:
                print(f"   - {e}")
            total_issues += len(errs)
        else:
            print(f"✅ [{fn}] 100% CLEAN - No markdown conflict risks found.")

print(f"\n=== AUDIT COMPLETE. Total issues found: {total_issues} ===")
