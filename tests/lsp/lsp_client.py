"""Minimal LSP client that talks to `clpp --lsp` exactly like VS Code does (JSON-RPC over stdio)."""
import json, os, subprocess, sys, threading, queue, time

class Client:
    def __init__(self, exe, root):
        self.p = subprocess.Popen([exe, "--lsp"], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        self.id = 0; self.q = queue.Queue(); self.diags = {}
        threading.Thread(target=self._read, daemon=True).start()
        self.root = root
        r = self.req("initialize", {"processId": os.getpid(), "rootUri": "file://" + root, "capabilities": {}})
        self.caps = r["result"]["capabilities"] if r and "result" in r else None
        self.notify("initialized", {})
    def _read(self):
        f = self.p.stdout
        while True:
            headers = {}
            while True:
                line = f.readline()
                if not line: return
                line = line.decode().strip()
                if not line: break
                k, v = line.split(":", 1); headers[k.lower()] = v.strip()
            body = json.loads(f.read(int(headers["content-length"])))
            if body.get("method") == "textDocument/publishDiagnostics":
                self.diags[body["params"]["uri"]] = body["params"]["diagnostics"]
            elif "id" in body:
                self.q.put(body)
    def send(self, msg):
        b = json.dumps(msg).encode()
        self.p.stdin.write(b"Content-Length: %d\r\n\r\n" % len(b) + b); self.p.stdin.flush()
    def notify(self, m, params): self.send({"jsonrpc": "2.0", "method": m, "params": params})
    def req(self, m, params, timeout=5):
        self.id += 1; self.send({"jsonrpc": "2.0", "id": self.id, "method": m, "params": params})
        t0 = time.time()
        while True:
            try:
                r = self.q.get(timeout=timeout)
            except queue.Empty:
                return None
            if r.get("id") == self.id:
                r["_ms"] = (time.time() - t0) * 1000
                return r
    def open(self, path, text, version=1):
        uri = "file://" + path
        self.notify("textDocument/didOpen", {"textDocument": {"uri": uri, "languageId": "clpp", "version": version, "text": text}})
        return uri
    def change(self, uri, text, version):
        self.notify("textDocument/didChange", {"textDocument": {"uri": uri, "version": version}, "contentChanges": [{"text": text}]})
    def pos(self, uri, m, line, ch, **extra):
        p = {"textDocument": {"uri": uri}, "position": {"line": line, "character": ch}}; p.update(extra)
        return self.req(m, p)
    def close(self):
        try:
            self.req("shutdown", None, 2); self.notify("exit", None)
        except Exception: pass
        self.p.kill()

def labels(r):
    if not r or r.get("result") is None: return None
    res = r["result"]; items = res["items"] if isinstance(res, dict) else res
    return [i["label"] for i in items]
