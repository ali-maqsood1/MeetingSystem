# AWS EC2 Deployment Guide - C++ Backend

## Part 1: Create EC2 Instance

### Step 1: Launch EC2 Instance

1. **Go to AWS Console** → EC2 Dashboard
2. **Click "Launch Instance"**
3. **Name your instance**: `meeting-system-backend`

### Step 2: Choose AMI (Operating System)

**Recommended:** Ubuntu Server 24.04 LTS (Free tier eligible)

- Or: Amazon Linux 2023
- Architecture: 64-bit (x86)

### Step 3: Choose Instance Type

**Recommended for testing:**

- `t2.micro` (Free tier: 1 vCPU, 1 GB RAM) - Good for testing
- `t2.small` (1 vCPU, 2 GB RAM) - Better performance
- `t3.small` (2 vCPU, 2 GB RAM) - Production ready

### Step 4: Key Pair (Login)

1. **Create new key pair** or select existing
2. Name: `meeting-system-key`
3. Type: **RSA**
4. Format: `.pem` (for Mac/Linux) or `.ppk` (for Windows/PuTTY)
5. **Download and save** - you'll need this to SSH!

### Step 5: Network Settings (Security Group)

**Create security group with these rules:**

| Type       | Protocol | Port | Source    | Description             |
| ---------- | -------- | ---- | --------- | ----------------------- |
| SSH        | TCP      | 22   | My IP     | SSH access              |
| Custom TCP | TCP      | 8080 | 0.0.0.0/0 | Backend API             |
| Custom TCP | TCP      | 80   | 0.0.0.0/0 | Optional: HTTP redirect |
| Custom TCP | TCP      | 443  | 0.0.0.0/0 | Optional: HTTPS         |

**Important:** For production, restrict SSH (port 22) to "My IP" only!

### Step 6: Configure Storage

- Default: 8 GB gp3 (General Purpose SSD)
- **Recommended:** 16-20 GB for comfort
- Keep defaults otherwise

### Step 7: Launch Instance

Click **"Launch Instance"** and wait ~30 seconds for it to start.

---

## Part 2: Connect to Your Instance

### Get Instance Details

1. Go to **EC2 Dashboard** → **Instances**
2. Select your instance
3. Note the **Public IPv4 address** (e.g., `3.84.123.45`)

### Connect via SSH

**On Mac/Linux:**

```bash
# Set permissions for your key file (REQUIRED FIRST TIME)
chmod 400 ~/Downloads/meeting-system-key.pem

# Connect to EC2
ssh -i ~/Downloads/meeting-system-key.pem ubuntu@YOUR_EC2_PUBLIC_IP
```

**Example:**

```bash
ssh -i ~/Downloads/meeting-system-key.pem ubuntu@3.84.123.45
```

**On Windows (using PowerShell):**

```powershell
ssh -i C:\Users\YourName\Downloads\meeting-system-key.pem ubuntu@YOUR_EC2_PUBLIC_IP
```

---

## Part 3: Install Dependencies on EC2

Once connected, run these commands:

### Update System

```bash
sudo apt update
sudo apt upgrade -y
```

### Install Build Tools

```bash
# Essential build tools
sudo apt install -y build-essential cmake git

# Install Boost libraries (required for your project)
sudo apt install -y libboost-all-dev

# Verify installations
g++ --version
cmake --version
```

Expected output:

- GCC: 11.x or higher ✅
- CMake: 3.22 or higher ✅

---

## Part 4: Deploy Your Code

### Option A: Git Clone (Recommended)

```bash
# Install git if not already
sudo apt install -y git

# Clone your repository
git clone https://github.com/YOUR_USERNAME/MeetingSystem.git
cd MeetingSystem
```

### Option B: Upload Files with SCP

**From your Mac (in a new terminal, NOT on EC2):**

```bash
# Go to your project directory
cd /Users/alimaqsood/Desktop/MeetingSystem

# Upload entire project
scp -i ~/Downloads/meeting-system-key.pem -r \
  /Users/alimaqsood/Desktop/MeetingSystem \
  ubuntu@51.20.77.103:~/
```

**Then on EC2:**

```bash
cd ~/MeetingSystem
```

---

## Part 5: Build the Project

```bash
# Create build directory
mkdir -p build
cd build

# Configure with CMake
cmake ..

# Build the project (use -j4 for faster build)
make -j4

# Verify the executable was created
.
```

You should see: `-rwxr-xr-x meeting_server` (executable)

---

## Part 6: Run the Server

### Quick Test Run

```bash
# Run the server
./meeting_server
```

You should see:

```
========================================
  Meeting System Server Starting...
========================================

...
HTTP Server initializing on 0.0.0.0:8080
Server is running!
```
ID
51676e79-406c-4695-8381-517e2d2cd92c

**Test it:**

```bash
# In another terminal (or from your laptop)
curl http://51.20.77.103:8080/health
```

Expected: `{"status":"ok"}`

### Stop the Server

Press `Ctrl+C` to stop.

---

## Part 7: Run Server in Background (Persistent)

### Option A: Using screen (Simple)

```bash
# Install screen
sudo apt install -y screen

# Start a screen session
screen -S meeting-server

# Run the server
cd ~/MeetingSystem/build
./meeting_server

# Detach from screen: Press Ctrl+A, then D
# Server keeps running!

# Reattach later:
screen -r meeting-server

# List all screens:
screen -ls
```

### Option B: Using systemd (Production)

Create a service file:

```bash
sudo nano /etc/systemd/system/meeting-server.service
```

**Paste this content:**

```ini
[Unit]
Description=Meeting System Backend Server
After=network.target

[Service]
Type=simple
User=ubuntu
WorkingDirectory=/home/ubuntu/MeetingSystem/build
ExecStart=/home/ubuntu/MeetingSystem/build/meeting_server
Restart=always
RestartSec=10

# Standard output logging
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=multi-user.target
```

**Enable and start the service:**

```bash
# Reload systemd
sudo systemctl daemon-reload

# Enable service (start on boot)
sudo systemctl enable meeting-server

# Start the service
sudo systemctl start meeting-server

# Check status
sudo systemctl status meeting-server

# View logs
sudo journalctl -u meeting-server -f
```

**Service commands:**

```bash
sudo systemctl start meeting-server    # Start
sudo systemctl stop meeting-server     # Stop
sudo systemctl restart meeting-server  # Restart
sudo systemctl status meeting-server   # Check status
```

---

## Part 8: Update Frontend Configuration

Update your frontend to use the EC2 IP address:

**Create `.env.production` in frontend directory:**

```bash
VITE_API_URL=http://51.20.77.103:8080/api/v1
```

**Or update `frontend/src/utils/api.js` to use EC2 IP directly.**

---

## Part 9: Test Your Deployment

### From Your Laptop

```bash
# Test health endpoint
curl http://YOUR_EC2_IP:8080/health

# Test with frontend
# Update frontend .env.local:
echo "VITE_API_URL=http://YOUR_EC2_IP:8080/api/v1" > frontend/.env.local

# Start frontend
cd frontend
npm run dev
```

Access: `http://localhost:5173`

---

## Part 10: Optional Enhancements

### A. Set Up a Domain Name (Optional)

1. Get domain from Route 53, Namecheap, etc.
2. Create A record pointing to EC2 IP
3. Update API URL to use domain

### B. Enable HTTPS with Let's Encrypt (Optional)

```bash
# Install Nginx as reverse proxy
sudo apt install -y nginx certbot python3-certbot-nginx

# Configure Nginx for your domain
sudo nano /etc/nginx/sites-available/meeting-system
```

**Nginx config:**

```nginx
server {
    listen 80;
    server_name your-domain.com;

    location / {
        proxy_pass http://localhost:8080;
        proxy_http_version 1.1;
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection 'upgrade';
        proxy_set_header Host $host;
        proxy_cache_bypass $http_upgrade;
    }
}
```

```bash
# Enable site
sudo ln -s /etc/nginx/sites-available/meeting-system /etc/nginx/sites-enabled/
sudo nginx -t
sudo systemctl restart nginx

# Get SSL certificate
sudo certbot --nginx -d your-domain.com
```

### C. Set Up Monitoring

```bash
# Install monitoring tools
sudo apt install -y htop

# Watch server resources
htop

# Monitor logs in real-time
sudo journalctl -u meeting-server -f
```

### D. Set Up Auto-Backups for Database

```bash
# Create backup script
nano ~/backup-db.sh
```

**Script content:**

```bash
#!/bin/bash
BACKUP_DIR="/home/ubuntu/backups"
DATE=$(date +%Y%m%d_%H%M%S)
mkdir -p $BACKUP_DIR
cp ~/MeetingSystem/build/meeting_system.db $BACKUP_DIR/meeting_system_$DATE.db
# Keep only last 7 days
find $BACKUP_DIR -name "meeting_system_*.db" -mtime +7 -delete
```

```bash
chmod +x ~/backup-db.sh

# Add to crontab (daily at 2 AM)
crontab -e
# Add this line:
0 2 * * * /home/ubuntu/backup-db.sh
```

---

## Troubleshooting

### Can't connect to port 8080?

- Check Security Group has port 8080 open
- Verify server is running: `sudo systemctl status meeting-server`
- Check if port is listening: `sudo netstat -tlnp | grep 8080`

### Out of memory?

- Use a larger instance type (t2.small or t2.medium)
- Or enable swap space:

```bash
sudo fallocate -l 2G /swapfile
sudo chmod 600 /swapfile
sudo mkswap /swapfile
sudo swapon /swapfile
```

### Server crashes?

- Check logs: `sudo journalctl -u meeting-server -n 100`
- Check available space: `df -h`
- Monitor memory: `free -h`

### Need to redeploy after code changes?

```bash
# Pull latest changes
cd ~/MeetingSystem
git pull

# Rebuild
cd build
make -j4

# Restart service
sudo systemctl restart meeting-server
```

---

## Costs Estimate (as of 2025)

**t2.micro (Free tier eligible):**

- First year: **FREE** (750 hours/month)
- After free tier: ~$8-10/month

**t2.small:**

- ~$17/month

**t3.small:**

- ~$15/month

**Data transfer:**

- First 1 GB out: Free
- 1-10 TB: $0.09/GB

**Storage:**

- 8 GB gp3: ~$0.80/month
- 16 GB gp3: ~$1.60/month

**Tip:** Stop your instance when not in use to save money!

---

## Quick Reference Commands

```bash
# Connect to EC2
ssh -i ~/Downloads/meeting-system-key.pem ubuntu@YOUR_EC2_IP

# Start server manually
cd ~/MeetingSystem/build && ./meeting_server

# Start server with systemd
sudo systemctl start meeting-server

# Check server status
sudo systemctl status meeting-server

# View logs
sudo journalctl -u meeting-server -f

# Restart after code changes
cd ~/MeetingSystem/build && make -j4 && sudo systemctl restart meeting-server

# Stop instance (to save money)
# Go to EC2 Console → Select instance → Instance State → Stop
```

---

## Next Steps

1. ✅ Launch EC2 instance
2. ✅ Connect via SSH
3. ✅ Install dependencies
4. ✅ Deploy and build code
5. ✅ Run server
6. ✅ Set up systemd service
7. ✅ Update frontend configuration
8. ✅ Test deployment
9. 🎉 Your backend is live!

**Your backend will be accessible at:**
`http://YOUR_EC2_IP:8080/api/v1`
