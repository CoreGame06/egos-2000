import os
import re

def fix_backticks_in_text(text):
    # Regex to avoid replacing already backticked text `...`
    # We split text by backticks, so odd-index parts are inside backticks, even-index parts are outside
    parts = text.split('`')
    
    for i in range(0, len(parts), 2):
        chunk = parts[i]
        
        # 1. Filenames (e.g., hello.s, hello.lds, hello.c, hello.elf, hello.bin, hello.lst, USAGES.md, Makefile)
        chunk = re.sub(r'\b([a-zA-Z0-9_-]+\.(?:c|s|lds|elf|bin|lst|md|h|img))\b', r'`\1`', chunk)
        chunk = re.sub(r'\b(Makefile)\b', r'`\1`', chunk)

        # 2. Underscore symbols starting with _ or containing _ in code context (e.g. _start, _end, _sbrk, __heap_start, _exit, _entry)
        chunk = re.sub(r'(?<![a-zA-Z0-9`_])(_+[a-zA-Z0-9_]+)(?![a-zA-Z0-9`_])', r'`\1`', chunk)
        
        # 3. Functions with parentheses (e.g. main(), printf(), terminal_write(), yield(), malloc(), free())
        chunk = re.sub(r'(?<![`a-zA-Z0-9_])([a-zA-Z0-9_]+\(\))(?![`a-zA-Z0-9_])', r'`\1`', chunk)

        # 4. RISC-V registers & CSRs when referenced in text (e.g. sp, ra, pc, satp, mstatus, mepc, mtvec, mcause, mscratch, sscratch, pmpaddr0, pmpcfg0)
        csr_regs = r'\b(sp|ra|pc|satp|mstatus|mepc|mtvec|mcause|mscratch|sscratch|pmpaddr0|pmpcfg0|a0|a1|a2|a3|a4|a5|s0|s1|t0|t1|t2)\b'
        # Only replace if surrounded by spaces/punctuation, not part of a larger English word
        chunk = re.sub(r'(?<=\s|[\(\[\{,;\.:])' + csr_regs + r'(?=\s|[\)\]\},;\.:]|$)', r'`\1`', chunk)

        # 5. RISC-V Instructions in text (e.g. ecall, mret, jal, lui, addi, sw, lw, ret, li, blt, nop, amoswap.w.aq)
        instructions = r'\b(ecall|mret|jal|lui|addi|sw|lw|ret|li|blt|nop|amoswap\.w\.aq)\b'
        chunk = re.sub(r'(?<=\s|[\(\[\{,;\.:])' + instructions + r'(?=\s|[\)\]\},;\.:]|$)', r'`\1`', chunk)

        parts[i] = chunk

    return '`'.join(parts)

def process_file(filepath):
    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()

    # Split into code blocks (```...```) and normal text
    code_block_split = re.split(r'(```[\s\S]*?```)', content)
    
    new_parts = []
    for part in code_block_split:
        if part.startswith('```'):
            # Code block: leave completely untouched
            new_parts.append(part)
        else:
            # Process normal text
            new_parts.append(fix_backticks_in_text(part))

    final_text = ''.join(new_parts)
    
    with open(filepath, 'w', encoding='utf-8') as f:
        f.write(final_text)
    print(f"Fixed backticks in {filepath}")

dir_path = '/home/shenmi/egos_book_bilingual'
for filename in os.listdir(dir_path):
    if filename.endswith('.md'):
        process_file(os.path.join(dir_path, filename))
