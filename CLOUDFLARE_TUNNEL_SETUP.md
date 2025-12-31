# Cloudflare Tunnel - Permanent Setup

## Current Status

✅ Quick tunnel working: `https://shipping-corporation-glory-allows.trycloudflare.com`
⚠️ Problem: URL changes every restart, not suitable for production

## Solution: Named Tunnel (Persistent)

### Step 1: Create Named Tunnel (on EC2)

```bash
# You already authenticated earlier, now create a named tunnel
cloudflared tunnel create meeting-backend

# You'll see output like:
# Tunnel credentials written to: /home/ubuntu/.cloudflared/<TUNNEL-ID>.json
# Created tunnel meeting-backend with id <TUNNEL-ID>
```

**Save the Tunnel ID** that's displayed!

### Step 2: Create Config File

```bash
# Create config directory if it doesn't exist
mkdir -p ~/.cloudflared

# Create config file
nano ~/.cloudflared/config.yml
```

**Paste this content** (replace `<TUNNEL-ID>` with your actual tunnel ID):

```yaml
tunnel: <TUNNEL-ID>
credentials-file: /home/ubuntu/.cloudflared/<TUNNEL-ID>.json

ingress:
  - hostname: your-subdomain.yourdomain.com
    service: http://localhost:8080
  - service: http_status:404
```

**Wait!** If you don't have a domain yet, use this simpler config:

```yaml
tunnel: <TUNNEL-ID>
credentials-file: /home/ubuntu/.cloudflared/<TUNNEL-ID>.json

ingress:
  - service: http://localhost:8080
```

### Step 3: Route Traffic to Tunnel

**Option A: Without Domain (Use trycloudflare.com)**

```bash
# This won't give you a custom domain, but it will be a persistent trycloudflare.com URL
cloudflared tunnel route dns meeting-backend meeting-backend.trycloudflare.com
```

**Option B: With Your Own Domain**

First, add your domain to Cloudflare:

1. Go to https://dash.cloudflare.com
2. Click "Add Site"
3. Enter your domain
4. Follow instructions to change nameservers

Then route the tunnel:

```bash
# Replace with your domain
cloudflared tunnel route dns meeting-backend api.yourdomain.com
```

### Step 4: Test the Tunnel

```bash
# Run the tunnel
cloudflared tunnel run meeting-backend

# Keep this running and test from your laptop:
curl https://api.yourdomain.com/health
# or
curl https://meeting-backend.trycloudflare.com/health
```

### Step 5: Run as Background Service (systemd)

Create service file:

```bash
sudo nano /etc/systemd/system/cloudflared.service
```

**Paste this content** (replace `<TUNNEL-ID>`):

```ini
[Unit]
Description=Cloudflare Tunnel
After=network.target

[Service]
Type=simple
User=ubuntu
ExecStart=/usr/local/bin/cloudflared tunnel run meeting-backend
Restart=always
RestartSec=5

[Install]
WantedBy=multi-user.target
```

**Enable and start the service:**

```bash
# Reload systemd
sudo systemctl daemon-reload

# Enable service (start on boot)
sudo systemctl enable cloudflared

# Start the service
sudo systemctl start cloudflared

# Check status
sudo systemctl status cloudflared

# View logs
sudo journalctl -u cloudflared -f
```

**Service commands:**

```bash
sudo systemctl start cloudflared    # Start
sudo systemctl stop cloudflared     # Stop
sudo systemctl restart cloudflared  # Restart
sudo systemctl status cloudflared   # Check status
```

---

## Alternative: Free Domain Option

If you don't have a domain, you have two options:

### Option 1: Get a Free Domain

1. **FreeDNS** (afraid.org): Free subdomains like `yourapp.mooo.com`
2. **Freenom**: Free domains (.tk, .ml, .ga)
3. **No-IP**: Free dynamic DNS

Then add to Cloudflare and follow "Option B" above.

### Option 2: Use Cloudflare's Quick Tunnel with Service

You can keep using quick tunnels but run them persistently:

```bash
sudo nano /etc/systemd/system/cloudflared-quick.service
```

```ini
[Unit]
Description=Cloudflare Quick Tunnel
After=network.target

[Service]
Type=simple
User=ubuntu
ExecStart=/usr/local/bin/cloudflared tunnel --url http://localhost:8080
Restart=always
RestartSec=5

# Log the output to capture the tunnel URL
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=multi-user.target
```

**Start it:**

```bash
sudo systemctl daemon-reload
sudo systemctl enable cloudflared-quick
sudo systemctl start cloudflared-quick

# Check logs to see the URL
sudo journalctl -u cloudflared-quick -f
```

**Problem with this approach:** The URL still changes on restart, so you'd need to:

1. Check logs for new URL after each restart
2. Update your frontend `.env.local` file
3. Redeploy frontend

**Not ideal for production!**

---

## Recommended: Get a Cheap Domain

**Best option for production:** Buy a domain ($1-12/year)

- **Namecheap**: $0.98/year for .xyz, $8.88/year for .com
- **Porkbun**: $1.14/year for .xyz, $9.13/year for .com
- **Cloudflare Registrar**: At-cost pricing ($9.15/year for .com)

Then use the named tunnel setup above with your domain.

---

## Quick Commands Reference

```bash
# List all tunnels
cloudflared tunnel list

# Delete a tunnel
cloudflared tunnel delete meeting-backend

# Check tunnel status
sudo systemctl status cloudflared

# View tunnel logs
sudo journalctl -u cloudflared -f

# Test your tunnel
curl https://your-tunnel-url/health
```

---

## What's Next?

1. **For now (testing):** Keep the quick tunnel running in `screen`:

   ```bash
   screen -S cloudflared
   cloudflared tunnel --url http://localhost:8080
   # Press Ctrl+A, then D to detach
   # Reattach: screen -r cloudflared
   ```

2. **For production:** Follow the named tunnel setup above

3. **Update frontend:** Once you have a permanent URL:

   ```bash
   # Update .env.local or deploy to Vercel with new URL
   VITE_API_URL=https://your-permanent-url/api/v1
   ```

4. **Close port 8080:** After tunnel is working, you can close port 8080 in EC2 security group (traffic goes through Cloudflare)
