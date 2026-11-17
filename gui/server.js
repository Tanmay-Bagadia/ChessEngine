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

let pendingEngineMove = false;
let pendingStyleUpdates = null;
let styleDebounceTimer = null;

const defaultOptions = {
    useNNUE: false,
    multiPV: 3,
    hash: 16
};

let styleProfile = {
    calculation: 50,
    aggression: 50,
    structure: 50,
    creativity: 50,
    defense: 50,
    prophylaxis: 50,
    activity: 50,
    enhancement: 50,
    transmutation: 50,
    conjuration: 50,
    specialization: 50,
    manipulation: 50,
    emission: 50
};
let currentPreset = 'Balanced';

const PRESETS = {
    'Tal':       { calculation: 61, aggression: 56, structure: 43, creativity: 79, defense: 62, prophylaxis: 62, activity: 80 },
    'Petrosian': { calculation: 31, aggression: 25, structure: 95, creativity: 25, defense: 95, prophylaxis: 95, activity: 25 },
    'Karpov':    { calculation: 25, aggression: 31, structure: 85, creativity: 63, defense: 93, prophylaxis: 93, activity: 64 },
    'Morphy':    { calculation: 95, aggression: 95, structure: 25, creativity: 95, defense: 25, prophylaxis: 25, activity: 95 },
    'Kasparov':  { calculation: 49, aggression: 48, structure: 65, creativity: 56, defense: 92, prophylaxis: 92, activity: 69 },
    'Carlsen':   { calculation: 54, aggression: 43, structure: 27, creativity: 43, defense: 70, prophylaxis: 70, activity: 78 },
    'Balanced':  { calculation: 50, aggression: 50, structure: 50, creativity: 50, defense: 50, prophylaxis: 50, activity: 50 }
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
    sendUciCommand(`setoption name UseNNUE value ${defaultOptions.useNNUE}`);
    sendUciCommand(`setoption name MultiPV value ${defaultOptions.multiPV}`);
    sendUciCommand(`setoption name Hash value ${defaultOptions.hash}`);
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
    else if (line.startsWith('info string Style options active, NNUE disabled')) {
        defaultOptions.useNNUE = false;
        broadcastSSE({ type: 'options', defaultOptions });
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
        if (pendingEngineMove) {
            pendingEngineMove = false;
            if (playSettings.playAgainstEngine && bestMove && bestMove !== '(none)' && gameState.side !== playSettings.playerColor) {
                if (!gameState.mate && !gameState.stalemate) {
                    if (gameState.legalMoves && gameState.legalMoves.includes(bestMove)) {
                        gameMoves.push(bestMove);
                        const waitPromise = waitForState();
                        syncEnginePosition();
                        waitPromise.then((newState) => {
                            if (!newState.mate && !newState.stalemate) {
                                setTimeout(() => triggerLiveAnalysis(), 50);
                            }
                        });
                    } else {
                        console.warn(`[Engine] Illegal or stale move '${bestMove}' dropped; re-syncing position`);
                        syncEnginePosition();
                    }
                }
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
            let scoreVal = parseInt(tokens[++i], 10);
            res.rawScore = scoreVal;
            // Normalize to White's perspective if searching on Black's turn
            if (gameState.side === 'b') {
                scoreVal = -scoreVal;
            }
            res.score = scoreVal;
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
    pendingEngineMove = true;
    engineStatus.multipvLines = {};
    broadcastSSE({ type: 'lines_reset' });
    sendUciCommand(`go movetime ${playSettings.thinkTime || 500}`);
}

function triggerLiveAnalysis() {
    if (gameState.mate || gameState.stalemate) return;
    pendingEngineMove = false;
    engineStatus.multipvLines = {};
    broadcastSSE({ type: 'lines_reset' });
    sendUciCommand('go depth 10');
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
            playSettings,
            defaultOptions,
            style: { profile: styleProfile, preset: currentPreset },
            nen: { profile: styleProfile, preset: currentPreset }
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
            defaultOptions,
            style: { profile: styleProfile, preset: currentPreset },
            nen: { profile: styleProfile, preset: currentPreset },
            engineStatus
        }));
        return;
    }

    if ((url.pathname === '/api/style' || url.pathname === '/api/nen') && req.method === 'GET') {
        res.writeHead(200, { 'Content-Type': 'application/json' });
        res.end(JSON.stringify({
            profile: styleProfile,
            preset: currentPreset,
            defaultOptions
        }));
        return;
    }

    if ((url.pathname === '/api/style' || url.pathname === '/api/nen') && req.method === 'POST') {
        let body = '';
        req.on('data', chunk => body += chunk);
        req.on('end', () => {
            try {
                const parsed = JSON.parse(body);
                if (parsed.preset) currentPreset = parsed.preset;

                // Sync default trait values if preset specified without explicit traits
                if (parsed.preset && PRESETS[parsed.preset]) {
                    const p = PRESETS[parsed.preset];
                    for (const [k, v] of Object.entries(p)) {
                        if (parsed[k] === undefined) {
                            parsed[k] = v;
                        }
                    }
                }

                if (!pendingStyleUpdates) pendingStyleUpdates = {};
                if (parsed.preset) pendingStyleUpdates.preset = parsed.preset;
                
                const traitMap = {
                    'calculation': 'Calculation',
                    'enhancement': 'Calculation',
                    'aggression': 'Aggression',
                    'transmutation': 'Aggression',
                    'structure': 'Structure',
                    'conjuration': 'Structure',
                    'creativity': 'Creativity',
                    'specialization': 'Creativity',
                    'defense': 'Defense',
                    'prophylaxis': 'Defense',
                    'manipulation': 'Defense',
                    'activity': 'Activity',
                    'emission': 'Activity'
                };

                for (const [key, uciName] of Object.entries(traitMap)) {
                    if (parsed[key] !== undefined) {
                        const val = Math.max(0, Math.min(100, parseInt(parsed[key], 10)));
                        styleProfile[key] = val;
                        pendingStyleUpdates[key] = val;

                        // Also sync pair aliases
                        if (key === 'calculation') styleProfile.enhancement = val;
                        else if (key === 'enhancement') styleProfile.calculation = val;
                        else if (key === 'aggression') styleProfile.transmutation = val;
                        else if (key === 'transmutation') styleProfile.aggression = val;
                        else if (key === 'structure') styleProfile.conjuration = val;
                        else if (key === 'conjuration') styleProfile.structure = val;
                        else if (key === 'creativity') styleProfile.specialization = val;
                        else if (key === 'specialization') styleProfile.creativity = val;
                        else if (key === 'defense' || key === 'prophylaxis' || key === 'manipulation') {
                            styleProfile.defense = val;
                            styleProfile.prophylaxis = val;
                            styleProfile.manipulation = val;
                        }
                        else if (key === 'activity') styleProfile.emission = val;
                        else if (key === 'emission') styleProfile.activity = val;
                    }
                }

                // If engine is actively searching, stop it before queueing changes
                if (engineStatus.searching) {
                    sendUciCommand('stop');
                }

                // Coalesce in-flight requests and debounce by 250ms
                if (styleDebounceTimer) clearTimeout(styleDebounceTimer);
                styleDebounceTimer = setTimeout(() => {
                    if (!pendingStyleUpdates) return;
                    const updates = pendingStyleUpdates;
                    pendingStyleUpdates = null;

                    if (engineStatus.searching) {
                        sendUciCommand('stop');
                    }

                    for (const [key, uciName] of Object.entries(traitMap)) {
                        if (updates[key] !== undefined) {
                            sendUciCommand(`setoption name Style_${uciName} value ${updates[key]}`);
                        }
                    }

                    if (updates.preset && updates.preset !== 'Custom') {
                        sendUciCommand(`setoption name Style_Preset value ${updates.preset}`);
                    }

                    if (!gameState.mate && !gameState.stalemate) {
                        setTimeout(() => triggerLiveAnalysis(), 50);
                    }
                }, 250);

                broadcastSSE({
                    type: 'style',
                    profile: styleProfile,
                    preset: currentPreset
                });

                res.writeHead(200, { 'Content-Type': 'application/json' });
                res.end(JSON.stringify({ ok: true, profile: styleProfile, preset: currentPreset }));
            } catch (e) {
                res.writeHead(400, { 'Content-Type': 'application/json' });
                res.end(JSON.stringify({ error: e.message }));
            }
        });
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
                } else if (!newState.mate && !newState.stalemate) {
                    setTimeout(() => triggerLiveAnalysis(), 50);
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
        } else {
            setTimeout(() => triggerLiveAnalysis(), 50);
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

        if (playSettings.playAgainstEngine && newState.side !== playSettings.playerColor && !newState.mate && !newState.stalemate) {
            setTimeout(() => triggerEngineThink(), 50);
        } else if (!newState.mate && !newState.stalemate) {
            setTimeout(() => triggerLiveAnalysis(), 50);
        }
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
                if (parsed.useNNUE !== undefined) {
                    defaultOptions.useNNUE = !!parsed.useNNUE;
                    sendUciCommand(`setoption name UseNNUE value ${defaultOptions.useNNUE}`);
                }

                broadcastSSE({ type: 'settings', playSettings, defaultOptions });
                res.writeHead(200, { 'Content-Type': 'application/json' });
                res.end(JSON.stringify({ ok: true, playSettings, defaultOptions }));

                if (playSettings.playAgainstEngine && gameState.side !== playSettings.playerColor && !gameState.mate && !gameState.stalemate) {
                    triggerEngineThink();
                } else if (parsed.useNNUE !== undefined && !gameState.mate && !gameState.stalemate) {
                    setTimeout(() => triggerLiveAnalysis(), 50);
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

server.listen(PORT, '0.0.0.0', () => {
    console.log(`===============================================`);
    console.log(`♟️  Chess Engine Server Running! (100% C++ Engine)`);
    console.log(`🌐  Open in browser: http://localhost:${PORT}`);
    console.log(`===============================================`);
});
