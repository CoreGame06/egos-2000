import os
import re

def clean_file(filepath):
    with open(filepath, 'r', encoding='utf-8') as f:
        text = f.read()

    # 1. Remove navigation header artifacts at top up to the first '# '
    first_h1 = text.find('# ')
    if first_h1 != -1:
        text = text[first_h1:]

    # Split text into code blocks vs non-code blocks
    parts = re.split(r'(```[\s\S]*?```)', text)

    new_parts = []
    for part in parts:
        if part.startswith('```'):
            # Code block: leave untouched
            new_parts.append(part)
        else:
            # Non-code text: cleanup junk and apply blockquotes
            # Clean trailing language tags like .c, .lds, .asm, .lst attached to paragraph ends
            part = re.sub(r'(\w+)\.(c|lds|asm|lst)\n', r'\1:\n', part)

            # Process blocks separated by blank lines
            blocks = re.split(r'\n\s*\n', part)
            cleaned_blocks = []

            for block in blocks:
                block = block.strip()
                if not block:
                    continue
                if 'Skip to content' in block or 'Return to top' in block or '外观菜单' in block:
                    continue

                lines = [l.strip() for l in block.split('\n') if l.strip()]

                # Case 1: Heading block
                if lines[0].startswith('#'):
                    m = re.match(r'^(#+)\s+(.+?)(?:\s*/\s*(.+))?$', lines[0])
                    if m:
                        level, en_h, zh_h = m.groups()
                        if zh_h:
                            cleaned_blocks.append(f"{level} {en_h.strip()}\n> **{zh_h.strip()}**")
                        else:
                            cleaned_blocks.append(lines[0])
                    else:
                        cleaned_blocks.append(lines[0])
                    
                    if len(lines) > 1:
                        rest_en = lines[1]
                        rest_zh = lines[2] if len(lines) > 2 else ''
                        if rest_zh:
                            cleaned_blocks.append(f"{rest_en}\n> {rest_zh}")
                        else:
                            cleaned_blocks.append(rest_en)
                    continue

                # Case 2: English line followed by Chinese line
                if len(lines) == 2:
                    en_line, zh_line = lines[0], lines[1]
                    if zh_line.startswith('>'):
                        zh_line = zh_line[1:].strip()
                    cleaned_blocks.append(f"{en_line}\n> {zh_line}")
                    continue

                # Case 3: Multiple paired lines
                if len(lines) >= 4 and len(lines) % 2 == 0:
                    pairs = []
                    for i in range(0, len(lines), 2):
                        l1, l2 = lines[i], lines[i+1]
                        if l2.startswith('>'):
                            l2 = l2[1:].strip()
                        pairs.append(f"{l1}\n> {l2}")
                    cleaned_blocks.append('\n\n'.join(pairs))
                    continue

                cleaned_blocks.append('\n'.join(lines))

            new_parts.append('\n\n'.join(cleaned_blocks))

    result = '\n\n'.join(new_parts).strip() + '\n'
    # Ensure double newlines between blocks
    result = re.sub(r'\n{3,}', '\n\n', result)

    with open(filepath, 'w', encoding='utf-8') as f:
        f.write(result)
    print(f"Refreshed {filepath}")

# First re-copy original bilingual files to reset any bad replaces, then re-format
for fn in os.listdir('/home/shenmi/egos_book_bilingual'):
    if fn.endswith('.md'):
        clean_file(os.path.join('/home/shenmi/egos_book_bilingual', fn))
