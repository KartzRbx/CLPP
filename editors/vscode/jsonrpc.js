"use strict";

function createFramer(onMessage) {
  let buf = Buffer.alloc(0);
  return {
    push(chunk) {
      buf = Buffer.concat([buf, chunk]);
      while (true) {
        const headerEnd = buf.indexOf("\r\n\r\n");
        if (headerEnd < 0) {
          return;
        }
        const header = buf.slice(0, headerEnd).toString("utf8");
        const match = header.match(/Content-Length:\s*(\d+)/i);
        if (!match) {
          buf = buf.slice(headerEnd + 4);
          continue;
        }
        const len = Number(match[1]);
        const start = headerEnd + 4;
        if (buf.length < start + len) {
          return;
        }
        const body = buf.slice(start, start + len).toString("utf8");
        buf = buf.slice(start + len);
        try {
          onMessage(JSON.parse(body));
        } catch {
          // ignore malformed JSON-RPC bodies
        }
      }
    },
  };
}

function writeMessage(stream, msg) {
  const json = Buffer.from(JSON.stringify(msg), "utf8");
  stream.write(`Content-Length: ${json.length}\r\n\r\n`);
  stream.write(json);
}

module.exports = { createFramer, writeMessage };
