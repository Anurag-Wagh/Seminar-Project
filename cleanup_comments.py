from pathlib import Path

files = [
    Path('core/opal_core.h'), Path('core/opal_core.c'), Path('core/opal_tokens.h'), Path('core/opal_uids.h'),
    Path('ral/opal_ral.h'), Path('ral/opal_ral_freertos.c'), Path('ral/opal_ral_posix.c'),
    Path('transport/opal_transport.h'), Path('transport/opal_transport_mock.c'), Path('tests/test_opal_core.c')
]
remove_tokens = [
    'LINUX', 'Linux', 'Member', 'Equivalent', 'EQUIVALENT', 'TCG', 'OPAL SSC',
    'Core Spec', 'HOW TO USE', 'Maps to:', 'This is the REAL RTOS target file',
    'This file allows opal_core.c to be compiled', 'Member 1 owns', 'Member 2 owns', 'Member 3 owns', 'Member 4 owns',
    'OS-agnostic header', 'Source: TCG Storage Architecture', 'Storage Architecture Core Specification',
    'Replays pre-recorded', 'parse_session_open()', 'parse_status()', 'build test', 'desktop unit tests',
    'Usage: compile with', 'PORTING NOTE:', 'Create opal_ral_<os>.c', 'do not touch', 'real RTOS implementation'
]

for path in files:
    if not path.exists():
        continue
    text = path.read_text()
    lines = text.splitlines()
    out = []
    for line in lines:
        stripped = line.strip()
        if not stripped:
            out.append(line)
            continue
        if stripped.startswith('*') and any(tok in line for tok in remove_tokens):
            continue
        if stripped.startswith('/*') and any(tok in line for tok in remove_tokens):
            continue
        if stripped.startswith('//') and any(tok in line for tok in remove_tokens):
            continue
        out.append(line)
    new_text = '\n'.join(out) + ('\n' if text.endswith('\n') else '')
    if new_text != text:
        path.write_text(new_text)
        print(f'cleaned {path}')
