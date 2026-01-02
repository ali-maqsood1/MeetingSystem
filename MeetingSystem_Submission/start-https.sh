#!/bin/bash
# Startup script for MeetingSystem with HTTPS (for network access)

# Colors for output
GREEN='\033[0;32m'
BLUE='\033[0;34m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}   MeetingSystem HTTPS Startup${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# Check if SSL certificates exist
if [ ! -f "certs/key.pem" ] || [ ! -f "certs/cert.pem" ]; then
    echo -e "${RED}⚠️  SSL certificates not found${NC}"
    echo -e "Generating certificates..."
    ./generate-certs.sh
    echo ""
fi

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

# Get local IP address
LOCAL_IP=$(ifconfig | grep "inet " | grep -v 127.0.0.1 | awk '{print $2}' | head -n 1)

echo -e "${GREEN}Starting all services with HTTPS...${NC}"
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

# Start Node.js Signaling Server with HTTPS (port 8181)
echo -e "${BLUE}[2/3] Starting WebRTC Signaling Server (HTTPS) on port 8181...${NC}"
cd signaling-server
node server.js &
SIGNALING_PID=$!
cd ..
sleep 2

# Start React Frontend with HTTPS (port 5173)
echo -e "${BLUE}[3/3] Starting React Frontend (HTTPS) on port 5173...${NC}"
cd frontend
npm run dev &
FRONTEND_PID=$!
cd ..

echo ""
echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}   All services started successfully!${NC}"
echo -e "${GREEN}========================================${NC}"
echo ""
echo -e "📍 ${YELLOW}Local Access:${NC}"
echo -e "   Backend API:      http://localhost:8080"
echo -e "   Signaling:        https://localhost:8181"
echo -e "   Frontend:         https://localhost:5173"
echo ""
echo -e "📱 ${YELLOW}Network Access (same WiFi):${NC}"
echo -e "   Backend API:      http://${LOCAL_IP}:8080"
echo -e "   Signaling:        https://${LOCAL_IP}:8181"
echo -e "   Frontend:         https://${LOCAL_IP}:5173"
echo ""
echo -e "${YELLOW}⚠️  Browser Security Warning:${NC}"
echo -e "   Your browser will show a security warning for self-signed certificates."
echo -e "   Click 'Advanced' → 'Proceed to ${LOCAL_IP} (unsafe)' to continue."
echo -e "   Do this for BOTH the frontend and when it tries to connect to signaling server."
echo ""
echo -e "${BLUE}Press Ctrl+C to stop all services${NC}"
echo ""

# Wait for all background processes
wait
