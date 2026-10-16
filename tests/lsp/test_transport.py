"""Bounded hostile JSON-RPC input: diagnose bad JSON, reject oversized frames.

Each subprocess receives finite input and has a timeout; no huge bodies are
allocated by the test. Normal valid requests follow malformed JSON to prove
the server remains usable.
"""
import json
import subprocess
import sys


def frame(value):
    body = value if isinstance(value, bytes) else json.dumps(value).encode('utf8')
    return f'Content-Length: {len(body)}\r\n\r\n'.encode() + body


def messages(data):
    result = []
    while data:
        header, data = data.split(b'\r\n\r\n', 1)
        length = int(header.split(b':', 1)[1].strip())
        result.append(json.loads(data[:length]))
        data = data[length:]
    return result


def run(data):
    return subprocess.run([sys.argv[1], '--lsp'], input=data,
                          capture_output=True, timeout=10)


invalid = [b'{', [], {'jsonrpc':'2.0','id':1,'method':42},
           {'jsonrpc':'2.0','id':{},'method':'initialize'},
           {'jsonrpc':'2.0','id':2,'method':'initialize','params':42},
           {'jsonrpc':'1.0','id':3,'method':'initialize'}]
requests = [
    {'jsonrpc':'2.0','id':10,'method':'initialize','params':{'capabilities':{}}},
    {'jsonrpc':'2.0','id':11,'method':'shutdown','params':None},
    {'jsonrpc':'2.0','method':'exit','params':None}]
result = run(b''.join(frame(value) for value in invalid + requests))
assert result.returncode == 0, (result.returncode, result.stderr)
responses = messages(result.stdout)
assert len(responses) == len(invalid) + 2, responses
assert responses[0]['error']['code'] == -32700
assert all(item['error']['code'] == -32600 for item in responses[1:len(invalid)])
assert responses[-2]['id'] == 10 and 'capabilities' in responses[-2]['result']
assert responses[-1] == {'jsonrpc':'2.0','id':11,'result':None}
print('PASS invalid JSON-RPC input is diagnosed and the next valid request works')

headers = [b'Content-Length: -1\r\n\r\n', b'Content-Length: +2\r\n\r\n{}',
           b'Content-Length: 2junk\r\n\r\n{}', b'Content-Length: 67108865\r\n\r\n',
           b'Content-Length: 999999999999999999999999999\r\n\r\n',
           b'Content-Length: 2\r\nContent-Length: 2\r\n\r\n{}',
           b'X-Long: ' + b'x' * 17000 + b'\r\nContent-Length: 2\r\n\r\n{}',
           b'Content-Length: 10\r\n\r\n{}']
for data in headers:
    rejected = run(data)
    assert rejected.returncode == 1 and not rejected.stderr, (rejected.returncode, rejected.stderr)
print(f'PASS {len(headers)} invalid, truncated or oversized frame headers are bounded')
