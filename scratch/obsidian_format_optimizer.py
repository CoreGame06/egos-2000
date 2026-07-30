import os
import re

dir_path = '/home/shenmi/egos_book_bilingual'

metadata_map = {
    'overview.md': {'title': 'The EGOS Book Overview / 概述', 'tag': 'overview'},
    'outcome.md': {'title': 'Learning Outcomes / 学习成果', 'tag': 'outcomes'},
    'p0_hello.md': {'title': 'P0: Hello, World! / 你好，世界！', 'tag': 'p0-hello'},
    'p1_ct.md': {'title': 'P1: Cooperative Threads / 协作式多线程', 'tag': 'p1-threads'},
    'p2_mlfq.md': {'title': 'P2: Preemptive Scheduling & MLFQ / 抢占式调度', 'tag': 'p2-scheduling'},
    'p3_syscall.md': {'title': 'P3: System Call & Protection / 系统调用与保护', 'tag': 'p3-syscall'},
    'p4_vm.md': {'title': 'P4: Virtual Memory & Page Tables / 虚拟内存', 'tag': 'p4-vm'},
    'p5_io.md': {'title': 'P5: I/O Device Driver / 设备驱动', 'tag': 'p5-io'},
    'p6_file.md': {'title': 'P6: File System & Inode / 文件系统', 'tag': 'p6-filesystem'},
    'p7_net.md': {'title': 'P7: Ethernet & TCP/IP / 网卡与网络', 'tag': 'p7-network'},
    'p8_lock.md': {'title': 'P8: Multicore & Locks / 多核与锁', 'tag': 'p8-multicore'}
}

def optimize_for_obsidian(filepath):
    fn = os.path.basename(filepath)
    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()

    # 1. Ensure YAML frontmatter for Obsidian
    if not content.startswith('---'):
        info = metadata_map.get(fn, {'title': fn.replace('.md', ''), 'tag': 'chapter'})
        frontmatter = f"""---
title: "{info['title']}"
tags:
  - egos-2000
  - {info['tag']}
---

"""
        content = frontmatter + content

    # 2. Fix Obsidian math rendering: LaTeX formulas like \lceil 10144/512 \rceil -> $ \lceil 10144/512 \rceil $
    content = re.sub(r'(\\lceil[^$\n]+\\rceil)', r'$\1$', content)
    content = re.sub(r'(\d+\s*\\pmod\{\d+\})', r'$\1$', content)

    # 3. Ensure code fences specify Obsidian-supported syntax highlighters (e.g. ```asm -> ```assembly)
    content = re.sub(r'```asm\b', '```assembly', content)
    content = re.sub(r'```lst\b', '```text', content)
    content = re.sub(r'```lds\b', '```c', content)

    with open(filepath, 'w', encoding='utf-8') as f:
        f.write(content)
    print(f"Optimized for Obsidian: {fn}")

for filename in sorted(os.listdir(dir_path)):
    if filename.endswith('.md'):
        optimize_for_obsidian(os.path.join(dir_path, filename))
