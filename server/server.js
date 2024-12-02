//server.js
const express = require('express');
const { createServer } = require('http');
const { WebSocketServer } = require('ws');
const NodeMediaServer = require('node-media-server');

const app = express();
const httpServer = createServer(app);
const wsServer = new WebSocketServer({ server: httpServer });

// RTMP Server Configuration
const rtmpConfig = {
    rtmp: {
        port: 1935,
        chunk_size: 60000,
        gop_cache: true,
        ping: 30,
        ping_timeout: 60
    },
    http: {
        port: 8000,
        allow_origin: '*'
    }
};

const nms = new NodeMediaServer(rtmpConfig);

// Serve static files
app.use(express.static('public'));

// WebSocket Broadcast
const clients = new Set();

wsServer.on('connection', (ws) => {
    clients.add(ws);
    console.log('New WebSocket client connected');

    ws.on('close', () => {
        clients.remove(ws);
        console.log('WebSocket client disconnected');
    });
});

// Broadcast stream status
nms.on('prePublish', (id, StreamPath, args) => {
    console.log('Stream started:', StreamPath);
    clients.forEach(client => {
        client.send(JSON.stringify({
            type: 'stream_status',
            status: 'started',
            path: StreamPath
        }));
    });
});

nms.on('donePublish', (id, StreamPath, args) => {
    console.log('Stream ended:', StreamPath);
    clients.forEach(client => {
        client.send(JSON.stringify({
            type: 'stream_status',
            status: 'ended',
            path: StreamPath
        }));
    });
});

// Start servers
nms.run();
httpServer.listen(5000, () => {
    console.log('HTTP & WebSocket Server running on port 5000');
});