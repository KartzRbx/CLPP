// One-shot, user-authorized test of the desktop Discord Rich Presence transport.
const net = require('node:net');
const crypto = require('node:crypto');
const vscodeIcon = process.argv.includes('--vscode-icon');
const clientId = vscodeIcon ? '383226320970055681' : '1263505205522337886'; // Public editor application IDs.
const frame = (opcode, data) => {
  const body = Buffer.from(JSON.stringify(data));
  const header = Buffer.alloc(8); header.writeUInt32LE(opcode); header.writeUInt32LE(body.length, 4);
  return Buffer.concat([header, body]);
};
async function connect() {
  for (let i = 0; i < 10; ++i) {
    try {
      return await new Promise((resolve, reject) => {
        const socket = net.createConnection(`\\\\.\\pipe\\discord-ipc-${i}`);
        socket.setTimeout(2000, () => socket.destroy(Error('Discord connection timeout')));
        socket.once('error', reject); socket.once('connect', () => { socket.removeListener('error', reject); resolve(socket); });
      });
    } catch { /* Discord can use another numbered pipe. */ }
  }
  throw Error('No desktop Discord IPC pipe is available');
}
async function main() {
  if (process.platform !== 'win32') throw Error('This local probe currently targets Windows');
  const socket = await connect();
  let buffer = Buffer.alloc(0);
  const pending = new Map();
  const rejectAll = error => { for (const p of pending.values()) p.reject(error); pending.clear(); };
  socket.on('error', rejectAll);
  socket.on('close', () => rejectAll(Error('Discord IPC closed')));
  socket.on('data', chunk => {
    buffer = Buffer.concat([buffer, chunk]);
    while (buffer.length >= 8) {
      const opcode = buffer.readUInt32LE(), length = buffer.readUInt32LE(4);
      if (length > 1024 * 1024) { socket.destroy(Error('Oversized Discord IPC frame')); return; }
      if (buffer.length < 8 + length) return;
      const payload = buffer.subarray(8, 8 + length); buffer = buffer.subarray(8 + length);
      try {
        const reply = JSON.parse(payload.toString());
        const key = reply.nonce || (reply.evt === 'READY' ? 'ready' : '');
        const p = pending.get(key);
        if (p) { pending.delete(key); reply.evt === 'ERROR' || opcode === 2 ? p.reject(Error('Discord rejected the activity')) : p.resolve(reply); }
      } catch (error) { socket.destroy(error); }
    }
  });
  const wait = key => new Promise((resolve, reject) => {
    const timer = setTimeout(() => { pending.delete(key); reject(Error('Discord reply timeout')); }, 5000);
    pending.set(key, { resolve: value => { clearTimeout(timer); resolve(value); }, reject: error => { clearTimeout(timer); reject(error); } });
  });
  const sendActivity = async activity => {
    const nonce = crypto.randomUUID(), reply = wait(nonce);
    socket.write(frame(1, { cmd: 'SET_ACTIVITY', args: { pid: process.pid, activity }, nonce }));
    return reply;
  };
  try {
    const ready = wait('ready'); socket.write(frame(0, { v: 1, client_id: clientId })); await ready;
    const activity = { details: 'Programando CL++', state: 'Projeto: CLPP_Rework | main.clp',
      timestamps: { start: Math.floor(Date.now() / 1000) } };
    if (vscodeIcon) activity.assets = {
      large_image: require('./vscode-icon.cjs').ICON, large_text: 'CL++',
      small_image: 'vscode', small_text: 'Visual Studio Code'
    };
    const acknowledgement = await sendActivity(activity);
    if (vscodeIcon && !acknowledgement.data?.assets?.large_image) throw Error('Discord did not return a large image asset');
    console.log('PASS Discord accepted the CL++ activity through local RPC');
    await sendActivity(null);
    console.log('PASS test activity cleared; no code or full paths were sent');
  } finally { socket.destroy(); }
}
main().catch(error => { console.error(error.message); process.exitCode = 1; });
