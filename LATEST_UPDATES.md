# Latest Updates - Single Session & Render Ping

## Feature 1: Single Session Enforcement ✅

**What it does:**

- When a user logs in from a new location, their previous session is automatically invalidated
- Only ONE active session per user at a time
- Previous device/browser is logged out automatically

**How it works:**

1. User logs in from Device A → Gets token A
2. Same user logs in from Device B → Gets token B
3. Token A is automatically invalidated
4. Device A will get "Unauthorized" on next API call

**Testing:**

```bash
# Terminal 1: Login as user1
curl -X POST http://localhost:8080/api/v1/auth/login \
  -H "Content-Type: application/json" \
  -d '{"email":"test@example.com","password":"password123"}'
# Save the token

# Terminal 2: Login again as same user
curl -X POST http://localhost:8080/api/v1/auth/login \
  -H "Content-Type: application/json" \
  -d '{"email":"test@example.com","password":"password123"}'
# Get new token

# Terminal 1: Try using old token - will fail
curl -H "Authorization: Bearer OLD_TOKEN" \
  http://localhost:8080/api/v1/users/me
# Response: {"success":false,"error":"Invalid or expired token"}
```

**Console output:**

```
User logged in: ali1
Previous session invalidated for user: ali1
User logged in: ali1
```

---

## Feature 2: Render Server Wake-Up Ping ✅

**What it does:**

- Automatically pings your Render signaling server when someone opens the app
- Wakes up sleeping Render free tier services (which sleep after 15 min inactivity)
- Happens in the background, user doesn't notice

**Setup:**

1. **Create `.env.local` in frontend:**

   ```bash
   cd frontend
   nano .env.local
   ```

2. **Add your Render URL:**

   ```env
   VITE_SIGNALING_SERVER_URL=https://your-signaling-server.onrender.com
   ```

3. **Restart frontend:**
   ```bash
   npm run dev
   ```

**What happens:**

- User opens `http://localhost:5173`
- App automatically sends ping to Render server
- Render server wakes up (takes ~30-60 seconds)
- By the time user joins a meeting, signaling server is ready!

**Console output in browser:**

```
Pinging signaling server to wake it up...
Signaling server pinged successfully
```

---

## Testing Both Features

### Test Single Session:

1. **Open browser 1 (Chrome):**

   - Go to `http://localhost:5173`
   - Login as `ali1@example.com`
   - Go to lobby (should work)

2. **Open browser 2 (Firefox/Safari):**

   - Go to `http://localhost:5173`
   - Login as same user `ali1@example.com`
   - Go to lobby (should work)

3. **Back to browser 1:**
   - Try to create a meeting or access any feature
   - Should be logged out automatically
   - Error: "Invalid or expired token"

### Test Render Ping:

1. **Check browser console:**

   ```javascript
   // Open DevTools (F12) → Console tab
   // You should see:
   Pinging signaling server to wake it up...
   API URL: http://localhost:8080/api/v1
   Signaling server pinged successfully
   ```

2. **Verify ping was sent:**
   - Check Network tab in DevTools
   - Look for request to your Render URL
   - Status: (failed) or 0 - This is OK! The server got the ping

---

## Production Deployment Checklist

When deploying to AWS EC2 or other production:

### Backend:

- [x] Single session implemented
- [x] CORS enabled for all origins
- [x] Server binds to 0.0.0.0
- [ ] Deploy to EC2 (follow AWS_EC2_DEPLOYMENT.md)

### Frontend:

- [ ] Create `.env.production`:
  ```env
  VITE_API_URL=http://YOUR_EC2_IP:8080/api/v1
  VITE_SIGNALING_SERVER_URL=https://your-signaling.onrender.com
  ```
- [ ] Build for production: `npm run build`
- [ ] Deploy to Vercel/Netlify (or serve from EC2)

### Signaling Server (Render):

- [ ] Make sure health endpoint exists: `/health`
- [ ] Should return 200 OK for ping to work

---

## Security Notes

**Single Session:**

- ✅ Prevents account sharing
- ✅ Forces logout on new login
- ✅ More secure than multiple concurrent sessions
- ⚠️ User might be surprised if they forgot they were logged in elsewhere

**Render Ping:**

- ✅ Uses `mode: 'no-cors'` to avoid CORS issues
- ✅ Fails silently if server unreachable
- ✅ Non-blocking, doesn't affect app performance
- ℹ️ Render free tier: Server wakes in ~30-60 seconds

---

## Rollback Instructions

If you need to disable either feature:

**Disable Single Session:**

```cpp
// In AuthManager.cpp, comment out lines 133-140:
// auto user_session_it = user_sessions.find(user.user_id);
// if (user_session_it != user_sessions.end()) {
//     std::string old_token = user_session_it->second;
//     sessions.erase(old_token);
//     std::cout << "Previous session invalidated..." << std::endl;
// }
```

**Disable Render Ping:**

```javascript
// In frontend/src/App.jsx, comment out the useEffect:
// useEffect(() => { ... }, []);
```

---

## Done! 🎉

Both features are ready for production. Test them locally, then deploy to AWS EC2!
