const http = require('http');
const { spawn } = require('child_process');
const fs = require('fs');
const path = require('path');

const PORT = process.env.PORT || 3000;
const ENGINE_PATH = path.resolve(__dirname, '..', 'build', 'engine');

let engineProcess = null;
let sseClients = [];
let engineOutputLog = [];
let engineStatus = {
    connected: false,
    searching: false,
    lastBestMove: null,
    multipvLines: {},
    options: {
        hash: 16,
        useNNUE: true,
        multipv: 3
    }
};

function broadcastSSE(data) {
    const msg = `data: ${JSON.stringify(data)}\n\n`;
    sseClients.forEach(res => res.write(msg));
}

function startEngine() {
    if (!fs.existsSync(ENGINE_PATH)) {
        console.error(`[Engine] Executable not found at ${ENGINE_PATH}. Run 'cmake --build build' first.`);
        return;
    }

    console.log(`[Engine] Spawning engine at ${ENGINE_PATH}...`);
    engineProcess = spawn(ENGINE_PATH, [], {
        cwd: path.resolve(__dirname, '..')
    });

    engineStatus.connected = true;

    engineProcess.stdout.on('data', (data) => {
        const text = data.toString();
        const lines = text.split(/\r?\n/);

        for (const line of lines) {
            if (!line.trim()) continue;
            // console.log(`[UCI Out] ${line}`);
            engineOutputLog.push(line);
            if (engineOutputLog.length > 500) engineOutputLog.shift();

            parseUciLine(line);
        }
    });

    engineProcess.stderr.on('data', (data) => {
        console.error(`[Engine Error] ${data.toString()}`);
    });

    engineProcess.on('close', (code) => {
        console.log(`[Engine] Process exited with code ${code}`);
        engineStatus.connected = false;
        engineStatus.searching = false;
        broadcastSSE({ type: 'engine_exit', code });
    });

    sendUciCommand('uci');
    sendUciCommand('setoption name UseNNUE value false');
    sendUciCommand('setoption name MultiPV value 3');
    sendUciCommand('isready');
}

function sendUciCommand(cmd) {
    if (!engineProcess || !engineProcess.stdin.writable) {
        console.error(`[Engine] Cannot send '${cmd}': engine process not ready`);
        return;
    }
    // console.log(`[UCI In] ${cmd}`);
    engineProcess.stdin.write(cmd + '\n');
}

function parseUciLine(line) {
    if (line.startsWith('id name ')) {
        engineStatus.name = line.substring(8);
        broadcastSSE({ type: 'engine_info', name: engineStatus.name });
    }
    else if (line === 'readyok') {
        broadcastSSE({ type: 'ready' });
    }
    else if (line.startsWith('info ')) {
        engineStatus.searching = true;
        const infoObj = parseInfoLine(line);
        if (infoObj.multipv !== undefined) {
            engineStatus.multipvLines[infoObj.multipv] = infoObj;
        }
        broadcastSSE({ type: 'info', info: infoObj });
    }
    else if (line.startsWith('bestmove ')) {
        engineStatus.searching = false;
        const parts = line.split(' ');
        const bestMove = parts[1];
        engineStatus.lastBestMove = bestMove;
        broadcastSSE({ type: 'bestmove', move: bestMove, raw: line });
    }
}

function parseInfoLine(line) {
    const tokens = line.split(' ');
    const res = { raw: line };

    for (let i = 1; i < tokens.length; i++) {
        const t = tokens[i];
        if (t === 'depth' && i + 1 < tokens.length) res.depth = parseInt(tokens[++i]);
        else if (t === 'multipv' && i + 1 < tokens.length) res.multipv = parseInt(tokens[++i]);
        else if (t === 'nodes' && i + 1 < tokens.length) res.nodes = parseInt(tokens[++i]);
        else if (t === 'nps' && i + 1 < tokens.length) res.nps = parseInt(tokens[++i]);
        else if (t === 'time' && i + 1 < tokens.length) res.time = parseInt(tokens[++i]);
        else if (t === 'score' && i + 2 < tokens.length) {
            const scoreType = tokens[++i];
            const scoreVal = parseInt(tokens[++i]);
            res.scoreType = scoreType; // 'cp' or 'mate'
            res.score = scoreVal;
        }
        else if (t === 'pv') {
            res.pv = tokens.slice(i + 1).join(' ');
            break;
        }
    }
    return res;
}

const server = http.createServer((req, res) => {
    const url = new URL(req.url, `http://${req.headers.host}`);

    // CORS headers
    res.setHeader('Access-Control-Allow-Origin', '*');
    res.setHeader('Access-Control-Allow-Methods', 'GET, POST, OPTIONS');
    res.setHeader('Access-Control-Allow-Headers', 'Content-Type');

    if (req.method === 'OPTIONS') {
        res.writeHead(204);
        res.end();
        return;
    }

    if (url.pathname === '/' || url.pathname === '/index.html') {
        const filePath = path.join(__dirname, 'index.html');
        fs.readFile(filePath, (err, data) => {
            if (err) {
                res.writeHead(500, { 'Content-Type': 'text/plain' });
                res.end('Failed to load index.html');
                return;
            }
            res.writeHead(200, { 'Content-Type': 'text/html; charset=utf-8' });
            res.end(data);
        });
        return;
    }

    if (url.pathname === '/chess.min.js') {
        const filePath = path.join(__dirname, 'chess.min.js');
        fs.readFile(filePath, (err, data) => {
            if (err) {
                res.writeHead(404, { 'Content-Type': 'text/plain' });
                res.end('Not Found');
                return;
            }
            res.writeHead(200, { 'Content-Type': 'application/javascript; charset=utf-8' });
            res.end(data);
        });
        return;
    }

    if (url.pathname.startsWith('/pieces/')) {
        const pieceFile = path.basename(url.pathname);
        const filePath = path.join(__dirname, 'pieces', pieceFile);
        fs.readFile(filePath, (err, data) => {
            if (err) {
                res.writeHead(404, { 'Content-Type': 'text/plain' });
                res.end('Not Found');
                return;
            }
            res.writeHead(200, { 'Content-Type': 'image/svg+xml; charset=utf-8' });
            res.end(data);
        });
        return;
    }

    if (url.pathname === '/api/stream') {
        res.writeHead(200, {
            'Content-Type': 'text/event-stream',
            'Cache-Control': 'no-cache',
            'Connection': 'keep-alive'
        });
        res.write('data: {"type": "connected"}\n\n');
        sseClients.push(res);

        req.on('close', () => {
            sseClients = sseClients.filter(c => c !== res);
        });
        return;
    }

    if (url.pathname === '/api/cmd' && req.method === 'POST') {
        let body = '';
        req.on('data', chunk => body += chunk);
        req.on('end', () => {
            try {
                const parsed = JSON.parse(body);
                if (parsed.cmd) {
                    sendUciCommand(parsed.cmd);
                    res.writeHead(200, { 'Content-Type': 'application/json' });
                    res.end(JSON.stringify({ ok: true, sent: parsed.cmd }));
                } else {
                    res.writeHead(400, { 'Content-Type': 'application/json' });
                    res.end(JSON.stringify({ error: 'Missing cmd property' }));
                }
            } catch (e) {
                res.writeHead(400, { 'Content-Type': 'application/json' });
                res.end(JSON.stringify({ error: e.message }));
            }
        });
        return;
    }

    if (url.pathname === '/api/status' && req.method === 'GET') {
        res.writeHead(200, { 'Content-Type': 'application/json' });
        res.end(JSON.stringify({
            status: engineStatus,
            clientCount: sseClients.length
        }));
        return;
    }

    res.writeHead(404, { 'Content-Type': 'text/plain' });
    res.end('Not Found');
});

startEngine();

server.listen(PORT, () => {
    console.log(`===============================================`);
    console.log(`♟️  TanmayChess Analysis GUI & Server Running!`);
    console.log(`🌐  Open in browser: http://localhost:${PORT}`);
    console.log(`===============================================`);
});

