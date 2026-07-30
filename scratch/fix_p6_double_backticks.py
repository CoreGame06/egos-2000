p6_path = '/home/shenmi/egos_book_bilingual/p6_file.md'
with open(p6_path, 'r', encoding='utf-8') as f:
    t = f.read()
t = t.replace('``mydisk_*``', '`mydisk_*`')
with open(p6_path, 'w', encoding='utf-8') as f:
    f.write(t)
print("Double backticks fixed.")
