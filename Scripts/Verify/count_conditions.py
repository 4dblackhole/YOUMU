"""if / while 조건 안의 && · || 개수가 3개 이상인 곳을 셉니다(AGENTS.md: 조건 하나에 최대 2개).

사용(저장소 어디서든):
    python Scripts/Verify/count_conditions.py [--list] [--check] [dir ...]
주석과 문자열·문자 리터럴을 먼저 지운 뒤 `if (`, `else if (`, `while (`의 괄호 안(여러 줄 포함)을 읽고
깊이와 관계없이 && · ||를 셉니다. --check는 하나라도 있으면 종료 코드 1을 냅니다.
"""
import os, re, sys

# 인자로 폴더를 주지 않으면 저장소 루트 기준 기본 폴더를 봅니다.
REPO_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

DEFAULT_DIRS = ['Client', 'YOUMU.Rhythm', 'YOUMU.Chart', 'YOUMU.Modes', 'YOUMU.Editor', 'YOUMU.Assets', 'Tests']
SKIP_PARTS = ('\\Assets\\', '/Assets/', '\\build\\', '/build/', '\\bin\\', '/bin/', 'Dependencies')

def strip(code):
    out, i, n = [], 0, len(code)
    while i < n:
        c = code[i]
        if code.startswith('//', i):
            j = code.find('\n', i)
            i = n if j < 0 else j
        elif code.startswith('/*', i):
            j = code.find('*/', i + 2)
            seg = code[i:(n if j < 0 else j + 2)]
            out.append('\n' * seg.count('\n'))
            i = n if j < 0 else j + 2
        elif c == 'R' and code.startswith('R"', i):
            m = re.match(r'R"([^()\\ ]{0,16})\(', code[i:])
            if not m:
                out.append(c); i += 1; continue
            end = code.find(')' + m.group(1) + '"', i)
            seg = code[i:(n if end < 0 else end + len(m.group(1)) + 2)]
            out.append('""' + '\n' * seg.count('\n'))
            i += len(seg)
        elif c == '\'' and i > 0 and code[i - 1].isalnum() and i + 1 < n and code[i + 1].isalnum() \
                and not re.search(r'(\bu8|\b[uUL])$', code[max(0, i - 2):i]):
            # C++14 digit separator such as 240'000'000, not a character literal.
            out.append(c); i += 1
        elif c in '"\'':
            j = i + 1
            while j < n and code[j] != c:
                j += 2 if code[j] == '\\' else 1
            out.append(c + c)
            i = j + 1
        else:
            out.append(c); i += 1
    return ''.join(out)

def conditions(code):
    for m in re.finditer(r'\b(if|while)\s*\(', code):
        depth, j = 1, m.end()
        while j < len(code) and depth:
            depth += {'(': 1, ')': -1}.get(code[j], 0)
            j += 1
        yield m.group(1), code.count('\n', 0, m.start()) + 1, code[m.end():j - 1]

def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    if not args:
        os.chdir(REPO_ROOT)
    listing = '--list' in sys.argv
    total, files = 0, 0
    per_dir = {}
    for root_dir in (args or DEFAULT_DIRS):
        for folder, _, names in os.walk(root_dir):
            if any(part in folder + os.sep for part in SKIP_PARTS):
                continue
            for name in names:
                if not name.endswith(('.cpp', '.h', '.hpp', '.inl')):
                    continue
                path = os.path.join(folder, name)
                code = strip(open(path, encoding='utf-8-sig', errors='replace').read())
                hits = [(kind, line, len(re.findall(r'&&|\|\|', body))) for kind, line, body in conditions(code)]
                hits = [h for h in hits if h[2] >= 3]
                if hits:
                    files += 1
                    total += len(hits)
                    per_dir[root_dir] = per_dir.get(root_dir, 0) + len(hits)
                    if listing:
                        for kind, line, count in hits:
                            print('%s:%d: %s with %d operators' % (path.replace('\\', '/'), line, kind, count))
    for key, value in sorted(per_dir.items()):
        print('%-14s %d' % (key, value))
    print('TOTAL %d conditions in %d files' % (total, files))
    if '--check' in sys.argv and total:
        sys.exit(1)

if __name__ == '__main__':
    main()
