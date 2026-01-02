import { BrowserRouter, Routes, Route, Navigate } from 'react-router-dom';
import { useEffect } from 'react';
import Login from './pages/Login';
import Lobby from './pages/Lobby';
import Meeting from './pages/Meeting';

function App() {
  useEffect(() => {
    // Ping Render signaling server to wake it up
    const wakeUpSignalingServer = async () => {
      // Replace with your actual Render signaling server URL
      const SIGNALING_SERVER_URL =
        import.meta.env.VITE_SIGNALING_SERVER_URL ||
        'https://your-signaling-server.onrender.com';

      try {
        console.log('Pinging signaling server to wake it up...');
        // Just fetch to wake up the server - don't care about response
        await fetch(`${SIGNALING_SERVER_URL}/health`, {
          method: 'GET',
          mode: 'no-cors', // Avoid CORS issues for the ping
        }).catch(() => {
          // Ignore errors - server might be waking up
        });
        console.log('Signaling server pinged successfully');
      } catch (error) {
        // Silently fail - server will wake up eventually
        console.log('Signaling server ping sent (may still be waking up)');
      }
    };

    // Ping on app load
    wakeUpSignalingServer();
  }, []);

  return (
    <BrowserRouter>
      <Routes>
        <Route path='/' element={<Login />} />
        <Route path='/lobby' element={<Lobby />} />
        <Route path='/meeting/:id' element={<Meeting />} />
        <Route path='*' element={<Navigate to='/' replace />} />
      </Routes>
    </BrowserRouter>
  );
}

export default App;
