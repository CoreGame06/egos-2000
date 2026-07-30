import os

# 1. Fix p7_net.md unclosed code block
p7_path = '/home/shenmi/egos_book_bilingual/p7_net.md'
with open(p7_path, 'r', encoding='utf-8') as f:
    t = f.read()
t = t.replace('> ```\n\nThe SUCCESS', '```\n\nThe SUCCESS')
t = t.replace('> ```\n\n', '```\n\n')
with open(p7_path, 'w', encoding='utf-8') as f:
    f.write(t)

# 2. Fix p1_ct.md thread_* and sys/queue.h angle brackets
p1_path = '/home/shenmi/egos_book_bilingual/p1_ct.md'
with open(p1_path, 'r', encoding='utf-8') as f:
    t = f.read()
t = t.replace('thread_*', '`thread_*`')
t = t.replace('<sys/queue.h>', '`<sys/queue.h>`')
t = t.replace('<<sys/`queue.h`>>', '`<sys/queue.h>`')
t = t.replace('<sys/`queue.h`>', '`<sys/queue.h>`')
with open(p1_path, 'w', encoding='utf-8') as f:
    f.write(t)

# 3. Fix p6_file.md mydisk_*
p6_path = '/home/shenmi/egos_book_bilingual/p6_file.md'
with open(p6_path, 'r', encoding='utf-8') as f:
    t = f.read()
t = t.replace('mydisk_*', '`mydisk_*`')
with open(p6_path, 'w', encoding='utf-8') as f:
    f.write(t)

print("Exact fixes applied.")
