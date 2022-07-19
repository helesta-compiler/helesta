import os, subprocess, time
from parse import parse

TIMEOUT = 20
ROUND = 5


def judge(test_case, base):
    in_file = base + '.in'
    ans_file = base + '.out'
    os.system('touch %s' % in_file)
    os.system('gcc %s libsysy.a -static -o main' % test_case)
    avg = 0
    print(in_file)
    for _ in range(ROUND):
        p = subprocess.Popen('./main', stdin=open(in_file),
                             stdout=open('out.txt', 'w'), stderr=open('time.txt', 'w'))
        try:
            p.wait(TIMEOUT)
        except subprocess.TimeoutExpired:
            p.kill()
        ret = p.returncode
        with open('out.txt') as f:
            output = f.read().strip()
        with open('time.txt') as f:
            timing = list(filter(None, f.read().strip().split('\n')))
        if timing:
            h, m, s, us = map(int, parse(
                'TOTAL: {}H-{}M-{}S-{}us', timing[-1]))
            timing = ((h * 60 + m) * 60 + s) * 1000000 + us
            timing /= 1000000
        else:
            timing = 0.0
        avg += timing
    avg /= ROUND
    result = f'{output}\n{ret}'.strip()
    with open('out.txt', 'w', encoding='UTF-8') as f:
        f.write(f'{result}\n')
    ret = os.system(f'diff -wq out.txt {ans_file} 1>&2') >> 8
    print({'filename': file_name, 'timing': round(avg, 6), 'return': ret})
    assert ret == 0

if __name__ == "__main__":
    g = os.walk(r"output")
    for path, _, file_list in g:
        for file_name in file_list:
            judge(os.path.join(path, file_name), os.path.join("testcases/" + path.split('/')[1], file_name.split('.')[0]))
