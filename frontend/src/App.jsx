import { BrowserRouter, Routes, Route, Navigate } from 'react-router-dom';
import { useEffect } from 'react';
import Login from './pages/Login';
import Lobby from './pages/Lobby';
import Meeting from './pages/Meeting';

function App() {
  useEffect(() => {
    const wakeUpSignalingServer = async () => {
      const SIGNALING_SERVER_URL =
        import.meta.env.VITE_SIGNALING_SERVER_URL ||
        'https://meeting-signaling.onrender.com';

      try {
        console.log('Pinging signaling server to wake it up...');
        await fetch(`${SIGNALING_SERVER_URL}/health`, {
          method: 'GET',
          mode: 'no-cors', 
        }).catch(() => {
        });
        console.log('Signaling server pinged successfully');
      } catch (error) {
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
