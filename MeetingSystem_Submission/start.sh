#!/bin/bash
# Startup script for MeetingSystem with WebRTC

# Colors for output
GREEN='\033[0;32m'
BLUE='\033[0;34m'
RED='\033[0;31m'
NC='\033[0m' # No Color

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}   MeetingSystem Hybrid Server Startup${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# Check if signaling server dependencies are installed
if [ ! -d "signaling-server/node_modules" ]; then
    echo -e "${RED}⚠️  Signaling server dependencies not found${NC}"
    echo -e "Installing Node.js dependencies..."
    cd signaling-server
    npm install
    cd ..
    echo ""
fi

# Check if frontend dependencies are installed
if [ ! -d "frontend/node_modules" ]; then
    echo -e "${RED}⚠️  Frontend dependencies not found${NC}"
    echo -e "Installing frontend dependencies..."
    cd frontend
    npm install
    cd ..
    echo ""
fi

# Check if C++ backend is built
if [ ! -f "build/meeting_server" ]; then
    echo -e "${RED}⚠️  C++ backend not built${NC}"
    echo -e "Building backend..."
    cd build
    cmake .. && make
    cd ..
    echo ""
fi

echo -e "${GREEN}Starting all services...${NC}"
echo ""

# Function to handle cleanup on exit
cleanup() {
    echo -e "\n${RED}Shutting down all services...${NC}"
    kill 0
    exit 0
}

trap cleanup SIGINT SIGTERM

# Start C++ Backend (port 8080)
echo -e "${BLUE}[1/3] Starting C++ Backend on port 8080...${NC}"
cd build
./meeting_server &
BACKEND_PID=$!
cd ..
sleep 2

# Start Node.js Signaling Server (port 8181)
echo -e "${BLUE}[2/3] Starting WebRTC Signaling Server on port 8181...${NC}"
cd signaling-server
node server.js &
SIGNALING_PID=$!
cd ..
sleep 2

# Start React Frontend (port 5173)
echo -e "${BLUE}[3/3] Starting React Frontend on port 5173...${NC}"
cd frontend
npm run dev &
FRONTEND_PID=$!
cd ..

echo ""
echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}   All services started successfully!${NC}"
echo -e "${GREEN}========================================${NC}"
echo ""
echo -e "📍 Backend API:           http://localhost:8080"
echo -e "📍 WebRTC Signaling:      http://localhost:8181"
echo -e "📍 Frontend:              http://localhost:5173"
echo ""
echo -e "${BLUE}Press Ctrl+C to stop all services${NC}"
echo ""

# Wait for all background processes
wait
