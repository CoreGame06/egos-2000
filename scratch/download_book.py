import os
import urllib.request
import re
from html.parser import HTMLParser

class HTMLToMarkdown(HTMLParser):
    def __init__(self):
        super().__init__()
        self.md = []
        self.ignore = False
        self.in_code = False

    def handle_starttag(self, tag, attrs):
        if tag in ['script', 'style', 'nav', 'header', 'footer']:
            self.ignore = True
        elif tag in ['h1', 'h2', 'h3', 'h4']:
            prefix = '\n\n### ' if tag=='h3' else ('\n\n## ' if tag=='h2' else '\n\n# ')
            self.md.append(prefix)
        elif tag == 'p':
            self.md.append('\n\n')
        elif tag == 'li':
            self.md.append('\n* ')
        elif tag in ['pre', 'code']:
            self.in_code = True
            if tag == 'pre':
                self.md.append('\n```\n')

    def handle_endtag(self, tag):
        if tag in ['script', 'style', 'nav', 'header', 'footer']:
            self.ignore = False
        elif tag == 'pre':
            self.in_code = False
            self.md.append('\n```\n')

    def handle_data(self, data):
        if not self.ignore:
            t = data
            if not self.in_code:
                t = re.sub(r'\s+', ' ', data)
            self.md.append(t)

pages = [
    'overview.html',
    'outcome.html',
    'p0_hello.html',
    'p1_ct.html',
    'p2_mlfq.html',
    'p3_syscall.html',
    'p4_vm.html',
    'p5_io.html',
    'p6_file.html',
    'p7_net.html',
    'p8_lock.html'
]

out_dir = '/home/shenmi/egos-2000/egos_book_en'
os.makedirs(out_dir, exist_ok=True)

for p in pages:
    url = f'https://egos.fun/book/{p}'
    req = urllib.request.Request(url, headers={'User-Agent': 'Mozilla/5.0'})
    try:
        html_content = urllib.request.urlopen(req).read().decode('utf-8')
        parser = HTMLToMarkdown()
        parser.feed(html_content)
        content = ''.join(parser.md).strip()
        out_path = os.path.join(out_dir, p.replace('.html', '.md'))
        with open(out_path, 'w', encoding='utf-8') as f:
            f.write(content)
        print(f'Successfully downloaded {p} -> {out_path} ({len(content)} bytes)')
    except Exception as e:
        print(f'Failed {p}: {e}')
