"""if / while 조건 밖의 식에서 && · ||가 3개 이상인 곳을 셉니다(조건 안은 count_conditions.py).

`return a && b && c && d;`, 초기화식, 대입, 함수 인자(Check(...), Require(...)), `for` 조건, 삼항 조건 등을 봅니다.
사용(저장소 어디서든):
    python Scripts/Verify/count_expressions.py [--list] [--check] [--exclude-from FILE] [dir ...]
FILE에는 건너뛸 파일을 저장소 기준 경로로 한 줄에 하나씩 적습니다(예: 아직 열려 있는 브랜치의 파일).
"식"은 쉼표·세미콜론으로 나뉜 한 조각입니다. 중첩된 호출 인자, 중괄호, 람다 본문은 따로 셉니다.
주석, 문자열·문자 리터럴, rvalue 참조 `&&`(`T&& x`, `auto&& x`)는 세지 않습니다. --check는 하나라도 있으면 종료 코드 1.
"""
import os, re, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from count_conditions import strip, DEFAULT_DIRS, SKIP_PARTS

KEYWORD_BEFORE_GROUP = {'return', 'case', 'throw', 'co_return', 'co_yield', 'co_await', 'else', 'do', 'not',
                        'and', 'or', 'new', 'delete', 'typeid'}
OPEN_BRACE_STATEMENT_HEADS = {')', ';', '{', '}', '', 'else', 'do', 'try', 'const', 'override', 'noexcept',
                              'final', 'namespace', 'struct', 'class', 'enum', 'union'}


def blank_preprocessor(code):
    """Blank #include/#pragma/#if lines (keeps line numbers); #define bodies stay."""
    lines = code.split('\n')
    i = 0
    while i < len(lines):
        s = lines[i].lstrip()
        if re.match(r'#\s*(include|pragma|if|ifdef|ifndef|elif|else|endif|undef|error|line)\b', s):
            while lines[i].rstrip().endswith('\\') and i + 1 < len(lines):
                lines[i] = ''
                i += 1
            lines[i] = ''
        i += 1
    return '\n'.join(lines)


def previous_token(code, pos):
    j = pos - 1
    while j >= 0 and code[j].isspace():
        j -= 1
    if j < 0:
        return ''
    if code[j].isalnum() or code[j] == '_':
        k = j
        while k >= 0 and (code[k].isalnum() or code[k] == '_'):
            k -= 1
        return code[k + 1:j + 1]
    return code[j]


def is_logical_and(code, pos):
    """True for `a && b`, false for rvalue references (`T&& x`, `T &&x`, `T&&)`, `Args &&...a`).

    The code base spaces a logical && on both sides (or ends the line with it); a reference
    declarator hugs the type or the name.
    """
    before = code[pos - 1] if pos > 0 else ' '
    after = code[pos + 2] if pos + 2 < len(code) else ' '
    if previous_token(code, pos) == 'operator':
        return False
    if before.isspace() and after.isspace():
        return True
    if before.isspace() or after.isspace():
        return False
    return after not in '),>.'  # a&&b is logical; T&&) / vector<T&&> / T&&... are not.


class Item:
    __slots__ = ('count', 'line')

    def __init__(self):
        self.count = 0
        self.line = 0


def expressions(code):
    """Yield (line, operator_count) for every expression outside if/while conditions."""
    line = 1
    # stack entries: [kind, item, opener_head]; kind: 'root' 'brace' 'call' 'group' 'cond'
    stack = [['root', Item(), '']]
    pending = []

    def owner():
        for entry in reversed(stack):
            if entry[0] != 'group':
                return entry
        return stack[0]

    def finish(entry):
        item = entry[1]
        if entry[0] != 'cond' and item.count >= 3:
            pending.append((item.line, item.count))
        entry[1] = Item()

    i, n = 0, len(code)
    while i < n:
        c = code[i]
        if c == '\n':
            line += 1
        elif c in '({[':
            head = previous_token(code, i)
            if c == '(':
                if head in ('if', 'while') or (head == 'constexpr' and re.search(r'\bif\s+constexpr\s*$', code[max(0, i - 40):i])):
                    kind = 'cond' if head != 'constexpr' else 'call'
                elif head and (head[-1].isalnum() or head[-1] == '_') and head not in KEYWORD_BEFORE_GROUP:
                    kind = 'call'
                elif head in (')', ']', '>'):
                    kind = 'call'
                else:
                    kind = 'group'
            elif c == '{':
                finish(owner())
                kind = 'brace'
            else:
                kind = 'call'
            stack.append([kind, Item(), head])
        elif c in ')}]':
            if len(stack) > 1:
                entry = stack.pop()
                if entry[0] != 'group':
                    finish(entry)
                if entry[0] == 'brace' and entry[2] in OPEN_BRACE_STATEMENT_HEADS or (
                        entry[0] == 'brace' and entry[2][-1:].isalnum() and entry[2] not in ('return',)):
                    finish(owner())
        elif c in ',;':
            target = owner()
            if c == ';' or target[0] != 'root':
                finish(target)
        elif code.startswith('&&', i) or code.startswith('||', i):
            if code[i] == '|' or is_logical_and(code, i):
                target = owner()
                if target[1].count == 0:
                    target[1].line = line
                target[1].count += 1
            i += 2
            continue
        i += 1
    for entry in stack:
        if entry[0] != 'group':
            finish(entry)
    return pending


def main():
    argv = sys.argv[1:]
    listing = '--list' in argv
    excluded = set()
    if '--exclude-from' in argv:
        at = argv.index('--exclude-from')
        for text in open(argv[at + 1], encoding='utf-8'):
            if text.strip():
                excluded.add(text.strip().replace('\\', '/'))
        del argv[at:at + 2]
    args = [a for a in argv if not a.startswith('--')]
    if not args:
        # 폴더를 주지 않으면 저장소 루트 기준 기본 폴더를 봅니다(--exclude-from 경로도 루트 기준).
        os.chdir(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
    total, files, skipped = 0, 0, 0
    per_dir = {}
    for root_dir in (args or DEFAULT_DIRS):
        for folder, _, names in os.walk(root_dir):
            if any(part in folder + os.sep for part in SKIP_PARTS):
                continue
            for name in sorted(names):
                if not name.endswith(('.cpp', '.h', '.hpp', '.inl')):
                    continue
                path = os.path.join(folder, name)
                key = path.replace('\\', '/')
                hits = expressions(blank_preprocessor(strip(open(path, encoding='utf-8-sig', errors='replace').read())))
                if key in excluded:
                    skipped += len(hits)
                    continue
                if hits:
                    files += 1
                    total += len(hits)
                    per_dir[root_dir] = per_dir.get(root_dir, 0) + len(hits)
                    if listing:
                        for line, count in hits:
                            print('%s:%d: expression with %d operators' % (key, line, count))
    for key, value in sorted(per_dir.items()):
        print('%-14s %d' % (key, value))
    if skipped:
        print('(excluded files hold %d more)' % skipped)
    print('TOTAL %d expressions in %d files' % (total, files))
    if '--check' in sys.argv and total:
        sys.exit(1)


if __name__ == '__main__':
    main()
