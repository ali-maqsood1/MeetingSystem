const express = require('express');
const { createServer } = require('https');
const { readFileSync } = require('fs');
const { Server } = require('socket.io');
const path = require('path');

const app = express();

// Load SSL certificates
const httpsOptions = {
    key: readFileSync(path.join(__dirname, '..', 'certs', 'key.pem')),
    cert: readFileSync(path.join(__dirname, '..', 'certs', 'cert.pem'))
};

const httpsServer = createServer(httpsOptions, app);
const io = new Server(httpsServer, {
    cors: {
        origin: ["https://localhost:5173", "http://localhost:5173", "https://192.168.*:5173"], // Frontend ports
        methods: ["GET", "POST"],
        credentials: true
    }
});

const connectedUsers = new Map(); // socketId -> { username, userId, meetingId }
const userIdToSocket = new Map(); // userId -> socketId
const meetingCalls = new Map(); // meetingId -> Map of userId -> username

console.log('========================================');
console.log('  WebRTC Signaling Server Starting...  ');
console.log('========================================\n');

io.on('connection', (socket) => {
    console.log(`✓ Socket connected: ${socket.id}`);

    // Join specific meeting room
    socket.on('joinMeeting', ({ username, userId, meetingId }) => {
        connectedUsers.set(socket.id, { username, userId, meetingId });
        userIdToSocket.set(userId, socket.id);
        socket.join(`meeting-${meetingId}`);
        console.log(`→ ${username} (ID: ${userId}) joined meeting ${meetingId}`);
    });

    // Join video call within meeting
    socket.on('joinCall', ({ meetingId, userId, username }) => {
        if (!meetingCalls.has(meetingId)) {
            meetingCalls.set(meetingId, new Map());
        }
        meetingCalls.get(meetingId).set(userId, username);

        // Send list of users already in call to the new joiner
        const usersInCall = Array.from(meetingCalls.get(meetingId).entries())
            .filter(([id]) => id !== userId)
            .map(([id, name]) => ({ userId: id, username: name }));
        socket.emit('usersInCall', usersInCall);

        // Notify others in the meeting room that someone joined the call
        socket.to(`meeting-${meetingId}`).emit('userJoinedCall', { userId, username });

        console.log(`📹 ${username} (ID: ${userId}) joined video call in meeting ${meetingId} (${meetingCalls.get(meetingId).size} users in call)`);
    });

    // Leave video call
    socket.on('leaveCall', ({ meetingId, userId }) => {
        const username = meetingCalls.get(meetingId)?.get(userId);
        if (meetingCalls.has(meetingId)) {
            meetingCalls.get(meetingId).delete(userId);
            if (meetingCalls.get(meetingId).size === 0) {
                meetingCalls.delete(meetingId);
            }
        }
        socket.to(`meeting-${meetingId}`).emit('userLeftCall', { userId });
        console.log(`← ${username || userId} left video call in meeting ${meetingId}`);
    });

    // WebRTC Signaling - Offer
    socket.on('offer', (data) => {
        const { offer, toUserId } = data;
        const fromUser = connectedUsers.get(socket.id);
        const toSocketId = userIdToSocket.get(toUserId);

        if (toSocketId) {
            io.to(toSocketId).emit('offer', {
                offer,
                fromUserId: fromUser.userId
            });
            console.log(`🔄 Offer: ${fromUser.username} → User ${toUserId}`);
        } else {
            console.log(`⚠️  Failed to send offer - User ${toUserId} not found`);
        }
    });

    // WebRTC Signaling - Answer
    socket.on('answer', (data) => {
        const { answer, toUserId } = data;
        const fromUser = connectedUsers.get(socket.id);
        const toSocketId = userIdToSocket.get(toUserId);

        if (toSocketId) {
            io.to(toSocketId).emit('answer', {
                answer,
                fromUserId: fromUser.userId
            });
            console.log(`✓ Answer: ${fromUser.username} → User ${toUserId}`);
        } else {
            console.log(`⚠️  Failed to send answer - User ${toUserId} not found`);
        }
    });

    // WebRTC Signaling - ICE Candidate
    socket.on('iceCandidate', (data) => {
        const { candidate, toUserId } = data;
        const toSocketId = userIdToSocket.get(toUserId);
        const fromUser = connectedUsers.get(socket.id);

        if (toSocketId) {
            io.to(toSocketId).emit('iceCandidate', {
                candidate,
                fromUserId: fromUser.userId
            });
        }
    });

    // Disconnect
    socket.on('disconnect', () => {
        const userInfo = connectedUsers.get(socket.id);
        if (userInfo) {
            const { username, userId, meetingId } = userInfo;

            // Remove from call if they were in one
            if (meetingCalls.has(meetingId)) {
                meetingCalls.get(meetingId).delete(userId);
                if (meetingCalls.get(meetingId).size === 0) {
                    meetingCalls.delete(meetingId);
                }
            }

            socket.to(`meeting-${meetingId}`).emit('userDisconnected', { userId });
            userIdToSocket.delete(userId);
            console.log(`✗ ${username} (ID: ${userId}) disconnected from meeting ${meetingId}`);
        }
        connectedUsers.delete(socket.id);
    });
});

const PORT = process.env.PORT || 8181;
httpsServer.listen(PORT, '0.0.0.0', () => {
    console.log(`\n✅ WebRTC Signaling Server running on https://0.0.0.0:${PORT}`);
    console.log(`   CORS enabled for HTTPS connections`);
    console.log(`   Ready to handle WebRTC signaling!\n`);
});
