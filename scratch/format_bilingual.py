import os
import re

def clean_and_format_file(filepath):
    with open(filepath, 'r', encoding='utf-8') as f:
        text = f.read()

    # 1. Remove navigation header artifacts at top up to the first '# '
    first_h1 = text.find('# ')
    if first_h1 != -1:
        text = text[first_h1:]

    # 2. Split into blocks separated by blank lines
    blocks = re.split(r'\n\s*\n', text)
    cleaned_blocks = []

    for block in blocks:
        block = block.strip()
        if not block:
            continue

        # Skip remaining nav artifact blocks if any
        if 'Skip to content' in block or 'Return to top' in block or '外观菜单' in block:
            continue

        # Code block handling: preserve exactly as is
        if block.startswith('```'):
            cleaned_blocks.append(block)
            continue

        # Clean trailing artifacts like '1234' or '.c' at line ends before code blocks
        lines = block.split('\n')
        
        # Check if block contains English line followed by Chinese line
        # Format English line, followed by blockquote '> ' for Chinese line
        if len(lines) == 2 and not lines[0].startswith('#') and not lines[1].startswith('>'):
            en, zh = lines[0].strip(), lines[1].strip()
            # Clean '.c', '.lds', '.asm', '1234' suffixes if any
            en = re.sub(r'(\.c|\.lds|\.asm|\.lst|12345?)$', '', en).strip()
            cleaned_blocks.append(f"{en}\n> {zh}")
            continue

        # Header with '/ 中文': split into Header and blockquote or clean format
        if block.startswith('#'):
            # e.g. '## Get started / 快速上手'
            header_match = re.match(r'^(#+)\s+(.+?)(?:\s*/\s*(.+))?$', lines[0])
            if header_match:
                level, en_title, zh_title = header_match.groups()
                if zh_title:
                    cleaned_blocks.append(f"{level} {en_title.strip()}\n> **{zh_title.strip()}**")
                else:
                    cleaned_blocks.append(lines[0])
                # Process remaining lines in header block if any
                if len(lines) > 1:
                    rest_lines = lines[1:]
                    if len(rest_lines) == 2:
                        cleaned_blocks.append(f"{rest_lines[0].strip()}\n> {rest_lines[1].strip()}")
                    else:
                        cleaned_blocks.append('\n'.join(rest_lines))
                continue

        # Handle general multi-line paragraphs (English + Chinese line pairs)
        if len(lines) % 2 == 0 and len(lines) >= 2:
            pair_blocks = []
            is_pairs = True
            for i in range(0, len(lines), 2):
                l_en = lines[i].strip()
                l_zh = lines[i+1].strip()
                # Check if second line contains Chinese characters
                if re.search(r'[\u4e00-\u9fa5]', l_zh) and not re.search(r'[\u4e00-\u9fa5]', l_en[:15]):
                    l_en = re.sub(r'(\.c|\.lds|\.asm|\.lst|12345?)$', '', l_en).strip()
                    pair_blocks.append(f"{l_en}\n> {l_zh}")
                else:
                    is_pairs = False
                    break
            if is_pairs:
                cleaned_blocks.append('\n\n'.join(pair_blocks))
                continue

        cleaned_blocks.append(block)

    formatted_md = '\n\n'.join(cleaned_blocks) + '\n'
    
    with open(filepath, 'w', encoding='utf-8') as f:
        f.write(formatted_md)
    print(f"Successfully formatted {filepath}")

# Format all files in /home/shenmi/egos_book_bilingual/
dir_path = '/home/shenmi/egos_book_bilingual'
for filename in os.listdir(dir_path):
    if filename.endswith('.md'):
        clean_and_format_file(os.path.join(dir_path, filename))
