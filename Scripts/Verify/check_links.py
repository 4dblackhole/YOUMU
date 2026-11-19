import re, os, sys, subprocess, urllib.parse
# Markdown의 상대 링크와 제목 앵커를 검사합니다. 인자가 없으면 저장소가 추적하는 md 전부(Dependencies 제외)를 봅니다.
# 사용: python Scripts/Verify/check_links.py [file.md ...]   깨진 링크가 있으면 종료 코드 1
def anchors(path):
    out = set()
    for line in open(path, encoding='utf-8-sig'):
        m = re.match(r'^(#{1,6})\s+(.*?)\s*$', line)
        if m:
            t = m.group(2)
            t = re.sub(r'`', '', t)
            a = ''.join(c for c in t.lower() if c.isalnum() or c in ' -_' or ord(c) > 127 and c not in '·—–→().,:/~×')
            a = a.replace(' ', '-')
            out.add(a)
    return out
files = sys.argv[1:]
if not files:
    os.chdir(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
    listed = subprocess.run(['git', 'ls-files', '-z', '*.md'], capture_output=True).stdout.decode('utf-8').split('\0')
    files = [f for f in listed if f and not f.startswith('Dependencies/')]
bad = 0
for md in files:
    text = open(md, encoding='utf-8-sig').read()
    for m in re.finditer(r'\[[^\]]*\]\(([^)\s]+)\)', text):
        link = m.group(1).strip('<>')
        if link.startswith(('http://', 'https://', 'mailto:')):
            continue
        target, _, frag = link.partition('#')
        target = urllib.parse.unquote(target)
        path = md if not target else os.path.normpath(os.path.join(os.path.dirname(md), target))
        if not os.path.exists(path):
            print('MISSING FILE', md, link); bad += 1; continue
        if frag and path.endswith('.md'):
            frag = urllib.parse.unquote(frag)
            if frag not in anchors(path):
                print('MISSING ANCHOR', md, link); bad += 1
print('bad links:', bad)
sys.exit(1 if bad else 0)
