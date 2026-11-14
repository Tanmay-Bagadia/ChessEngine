const http = require('http');
const { spawn } = require('child_process');
const fs = require('fs');
const path = require('path');

const PORT = process.env.PORT || 3000;
const ENGINE_PATH = path.resolve(__dirname, '..', 'build', 'engine');

let engineProcess = null;
let sseClients = [];
let engineOutputLog = [];

let gameStartFen = 'startpos';
let gameMoves = [];
let gameState = {
    fen: 'rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1',
    side: 'w',
    check: false,
    mate: false,
    stalemate: false,
    halfmove: 0,
    fullmove: 1,
    legalMoves: []
};

let playSettings = {
    playAgainstEngine: true,
    playerColor: 'w',
    thinkTime: 500
};

let engineStatus = {
    connected: false,
    searching: false,
    lastBestMove: null,
    multipvLines: {},
    options: {
        hash: 16,
        useNNUE: false,
        multipv: 3
    }
};

let stateResolvers = [];

function waitForState(timeoutMs = 1500) {
    return new Promise(resolve => {
        const timer = setTimeout(() => {
            const idx = stateResolvers.indexOf(resolve);
            if (idx !== -1) stateResolvers.splice(idx, 1);
            resolve(gameState);
        }, timeoutMs);

        stateResolvers.push((st) => {
            clearTimeout(timer);
            resolve(st);
        });
    });
}

function broadcastSSE(data) {
    const msg = `data: ${JSON.stringify(data)}\n\n`;
    sseClients.forEach(res => {
        try {
            res.write(msg);
        } catch (e) {
            // client disconnected
        }
    });
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

    let lineBuffer = '';
    engineProcess.stdout.on('data', (data) => {
        lineBuffer += data.toString();
        const lines = lineBuffer.split(/\r?\n/);
        lineBuffer = lines.pop(); // save incomplete line

        for (const line of lines) {
            if (!line.trim()) continue;
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
    sendUciCommand('ucinewgame');
    syncEnginePosition();
}

function sendUciCommand(cmd) {
    if (!engineProcess || !engineProcess.stdin.writable) {
        console.error(`[Engine] Cannot send '${cmd}': engine process not ready`);
        return;
    }
    engineProcess.stdin.write(cmd + '\n');
}

function syncEnginePosition() {
    let cmd = 'position ';
    if (gameStartFen === 'startpos') {
        cmd += 'startpos';
    } else {
        cmd += `fen ${gameStartFen}`;
    }

    if (gameMoves.length > 0) {
        cmd += ' moves ' + gameMoves.join(' ');
    }

    sendUciCommand(cmd);
    sendUciCommand('state');
}

function parseUciLine(line) {
    if (line.startsWith('id name ')) {
        engineStatus.name = line.substring(8);
        broadcastSSE({ type: 'engine_info', name: engineStatus.name });
    }
    else if (line === 'readyok') {
        broadcastSSE({ type: 'ready' });
    }
    else if (line.startsWith('state ')) {
        parseStateLine(line);
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

        // If playing against engine and it was engine's turn to play:
        if (playSettings.playAgainstEngine && bestMove && bestMove !== '(none)' && gameState.side !== playSettings.playerColor) {
            if (!gameState.mate && !gameState.stalemate) {
                gameMoves.push(bestMove);
                syncEnginePosition();
            }
        }
    }
}

function parseStateLine(line) {
    // Format: state fen <fen 6 tokens> side <w|b> check <0|1> mate <0|1> stalemate <0|1> halfmove <N> fullmove <N> legalmoves <m1> ...
    const tokens = line.split(' ');
    const fenTokens = tokens.slice(2, 8);
    const fen = fenTokens.join(' ');

    let side = 'w';
    let check = false;
    let mate = false;
    let stalemate = false;
    let halfmove = 0;
    let fullmove = 1;
    let legalMoves = [];

    let idx = 8;
    while (idx < tokens.length) {
        const tok = tokens[idx];
        if (tok === 'side' && idx + 1 < tokens.length) side = tokens[++idx];
        else if (tok === 'check' && idx + 1 < tokens.length) check = tokens[++idx] === '1';
        else if (tok === 'mate' && idx + 1 < tokens.length) mate = tokens[++idx] === '1';
        else if (tok === 'stalemate' && idx + 1 < tokens.length) stalemate = tokens[++idx] === '1';
        else if (tok === 'halfmove' && idx + 1 < tokens.length) halfmove = parseInt(tokens[++idx], 10);
        else if (tok === 'fullmove' && idx + 1 < tokens.length) fullmove = parseInt(tokens[++idx], 10);
        else if (tok === 'legalmoves') {
            legalMoves = tokens.slice(idx + 1).filter(m => m.length >= 4);
            break;
        }
        idx++;
    }

    gameState = {
        fen,
        side,
        check,
        mate,
        stalemate,
        halfmove,
        fullmove,
        legalMoves
    };

    const payload = {
        type: 'state',
        state: gameState,
        moves: gameMoves,
        playSettings
    };
    broadcastSSE(payload);

    const resolvers = stateResolvers.splice(0);
    resolvers.forEach(cb => cb(gameState));
}

function parseInfoLine(line) {
    const tokens = line.split(' ');
    const res = { raw: line };

    for (let i = 1; i < tokens.length; i++) {
        const t = tokens[i];
        if (t === 'depth' && i + 1 < tokens.length) res.depth = parseInt(tokens[++i], 10);
        else if (t === 'multipv' && i + 1 < tokens.length) res.multipv = parseInt(tokens[++i], 10);
        else if (t === 'nodes' && i + 1 < tokens.length) res.nodes = parseInt(tokens[++i], 10);
        else if (t === 'nps' && i + 1 < tokens.length) res.nps = parseInt(tokens[++i], 10);
        else if (t === 'time' && i + 1 < tokens.length) res.time = parseInt(tokens[++i], 10);
        else if (t === 'score' && i + 2 < tokens.length) {
            res.scoreType = tokens[++i]; // 'cp' or 'mate'
            res.score = parseInt(tokens[++i], 10);
        }
        else if (t === 'pv') {
            res.pv = tokens.slice(i + 1).join(' ');
            break;
        }
    }
    return res;
}

function triggerEngineThink() {
    if (gameState.mate || gameState.stalemate) return;
    sendUciCommand(`go movetime ${playSettings.thinkTime || 500}`);
}

const server = http.createServer(async (req, res) => {
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

        // Send current state on initial connect
        res.write(`data: ${JSON.stringify({
            type: 'state',
            state: gameState,
            moves: gameMoves,
            playSettings
        })}\n\n`);

        req.on('close', () => {
            sseClients = sseClients.filter(c => c !== res);
        });
        return;
    }

    if (url.pathname === '/api/state' && req.method === 'GET') {
        res.writeHead(200, { 'Content-Type': 'application/json' });
        res.end(JSON.stringify({
            state: gameState,
            moves: gameMoves,
            playSettings,
            engineStatus
        }));
        return;
    }

    if (url.pathname === '/api/move' && req.method === 'POST') {
        let body = '';
        req.on('data', chunk => body += chunk);
        req.on('end', async () => {
            try {
                const parsed = JSON.parse(body);
                const move = parsed.move; // e.g. "e2e4" or "e7e8q"

                if (!move || !gameState.legalMoves.includes(move)) {
                    res.writeHead(400, { 'Content-Type': 'application/json' });
                    res.end(JSON.stringify({
                        error: 'Illegal move rejected by engine',
                        move,
                        legalMoves: gameState.legalMoves
                    }));
                    return;
                }

                // Add move to game moves
                gameMoves.push(move);
                const waitPromise = waitForState();
                syncEnginePosition();
                const newState = await waitPromise;

                res.writeHead(200, { 'Content-Type': 'application/json' });
                res.end(JSON.stringify({ ok: true, state: newState, moves: gameMoves }));

                // If playing against engine and it's engine's turn:
                if (playSettings.playAgainstEngine && newState.side !== playSettings.playerColor && !newState.mate && !newState.stalemate) {
                    setTimeout(() => triggerEngineThink(), 50);
                }
            } catch (e) {
                res.writeHead(500, { 'Content-Type': 'application/json' });
                res.end(JSON.stringify({ error: e.message }));
            }
        });
        return;
    }

    if (url.pathname === '/api/newgame' && req.method === 'POST') {
        gameStartFen = 'startpos';
        gameMoves = [];
        engineStatus.multipvLines = {};
        sendUciCommand('stop');
        sendUciCommand('ucinewgame');
        const waitPromise = waitForState();
        syncEnginePosition();
        const newState = await waitPromise;

        res.writeHead(200, { 'Content-Type': 'application/json' });
        res.end(JSON.stringify({ ok: true, state: newState, moves: gameMoves }));

        if (playSettings.playAgainstEngine && playSettings.playerColor === 'b') {
            setTimeout(() => triggerEngineThink(), 50);
        }
        return;
    }

    if (url.pathname === '/api/undo' && req.method === 'POST') {
        sendUciCommand('stop');
        if (gameMoves.length > 0) {
            if (playSettings.playAgainstEngine && gameMoves.length >= 2) {
                gameMoves.pop();
                gameMoves.pop();
            } else {
                gameMoves.pop();
            }
        }
        const waitPromise = waitForState();
        syncEnginePosition();
        const newState = await waitPromise;

        res.writeHead(200, { 'Content-Type': 'application/json' });
        res.end(JSON.stringify({ ok: true, state: newState, moves: gameMoves }));
        return;
    }

    if (url.pathname === '/api/fen' && req.method === 'POST') {
        let body = '';
        req.on('data', chunk => body += chunk);
        req.on('end', async () => {
            try {
                const parsed = JSON.parse(body);
                const fen = parsed.fen ? parsed.fen.trim() : '';
                if (!fen) {
                    res.writeHead(400, { 'Content-Type': 'application/json' });
                    res.end(JSON.stringify({ error: 'Missing fen' }));
                    return;
                }

                gameStartFen = fen;
                gameMoves = [];
                sendUciCommand('stop');
                const waitPromise = waitForState();
                syncEnginePosition();
                const newState = await waitPromise;

                res.writeHead(200, { 'Content-Type': 'application/json' });
                res.end(JSON.stringify({ ok: true, state: newState, moves: gameMoves }));
            } catch (e) {
                res.writeHead(500, { 'Content-Type': 'application/json' });
                res.end(JSON.stringify({ error: e.message }));
            }
        });
        return;
    }

    if (url.pathname === '/api/settings' && req.method === 'POST') {
        let body = '';
        req.on('data', chunk => body += chunk);
        req.on('end', () => {
            try {
                const parsed = JSON.parse(body);
                if (parsed.playAgainstEngine !== undefined) playSettings.playAgainstEngine = parsed.playAgainstEngine;
                if (parsed.playerColor !== undefined) playSettings.playerColor = parsed.playerColor;
                if (parsed.thinkTime !== undefined) playSettings.thinkTime = parseInt(parsed.thinkTime, 10);

                broadcastSSE({ type: 'settings', playSettings });
                res.writeHead(200, { 'Content-Type': 'application/json' });
                res.end(JSON.stringify({ ok: true, playSettings }));

                if (playSettings.playAgainstEngine && gameState.side !== playSettings.playerColor && !gameState.mate && !gameState.stalemate) {
                    triggerEngineThink();
                }
            } catch (e) {
                res.writeHead(400, { 'Content-Type': 'application/json' });
                res.end(JSON.stringify({ error: e.message }));
            }
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

    res.writeHead(404, { 'Content-Type': 'text/plain' });
    res.end('Not Found');
});

startEngine();

server.listen(PORT, () => {
    console.log(`===============================================`);
    console.log(`♟️  TanmayChess Server Running! (100% C++ Engine)`);
    console.log(`🌐  Open in browser: http://localhost:${PORT}`);
    console.log(`===============================================`);
});
