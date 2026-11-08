// Takes a picture of the page a running WebViewer (--remote-debugging-port PORT) shows, through Chrome's DevTools
// protocol, after WAIT seconds: node web_capture.mjs PORT WAIT OUT.png [JS-EXPRESSION]
// With an expression, also prints its value as JSON (what the page itself holds, e.g. the audio values it was given).
import { writeFileSync } from 'node:fs';
const [port, wait, out, expression] = process.argv.slice(2);
const deadline = Date.now() + 30000;
let target;
while (!target) {
    try {
        const list = await (await fetch(`http://127.0.0.1:${port}/json`)).json();
        target = list.find((t) => t.type === 'page');
    } catch (e) { /* not up yet */ }
    if (!target) {
        if (Date.now() > deadline) throw new Error('no page on port ' + port);
        await new Promise((r) => setTimeout(r, 250));
    }
}
await new Promise((r) => setTimeout(r, Number(wait) * 1000));
const socket = new WebSocket(target.webSocketDebuggerUrl);
await new Promise((r) => socket.addEventListener('open', r));
let id = 0;
const pending = new Map();
socket.addEventListener('message', (m) => {
    const reply = JSON.parse(m.data);
    if (pending.has(reply.id)) { pending.get(reply.id)(reply); pending.delete(reply.id); }
});
const call = (method, params = {}) => new Promise((r) => { pending.set(++id, r); socket.send(JSON.stringify({ id, method, params })); });
if (expression) {
    const value = await call('Runtime.evaluate', { expression: `JSON.stringify(${expression})`, returnByValue: true });
    console.log(value.result?.result?.value ?? JSON.stringify(value));
}
const shot = await call('Page.captureScreenshot', { format: 'png' });
writeFileSync(out, Buffer.from(shot.result.data, 'base64'));
socket.close();
