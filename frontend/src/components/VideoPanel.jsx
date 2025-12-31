import { useState, useEffect, useRef } from 'react';
import { io } from 'socket.io-client';
import {
  Video,
  VideoOff,
  Mic,
  MicOff,
  Monitor,
  PhoneOff,
  User,
  MoreVertical,
} from 'lucide-react';

const VideoPanel = ({ meetingId, userId, username }) => {
  const [socket, setSocket] = useState(null);
  const [inCall, setInCall] = useState(false);
  const [cameraEnabled, setCameraEnabled] = useState(false);
  const [micEnabled, setMicEnabled] = useState(true);
  const [screenEnabled, setScreenEnabled] = useState(false);
  const [remoteStreams, setRemoteStreams] = useState(new Map());
  const [participants, setParticipants] = useState(new Map()); // userId -> username

  const localVideoRef = useRef(null);
  const localStreamRef = useRef(null);
  const screenStreamRef = useRef(null);
  const peerConnectionsRef = useRef(new Map());
  const socketRef = useRef(null);
  const inCallRef = useRef(false);
  const negotiationStateRef = useRef(new Map()); // remoteUserId -> { makingOffer: bool, ignoreOffer: bool }

  const isPolite = (remoteId) => String(userId) < String(remoteId);

  // Use environment variable for signaling server, or construct from hostname
  const getSignalingUrl = () => {
    if (import.meta.env.VITE_SIGNALING_URL) {
      return import.meta.env.VITE_SIGNALING_URL;
    }
    // For local development
    const protocol = window.location.protocol;
    const hostname = window.location.hostname;
    return `${protocol}//${hostname}:8181`;
  };

  const SIGNALING_SERVER = getSignalingUrl();

  // ICE Server configuration - Use your own TURN server for reliable connectivity
  const ICE_SERVERS = {
    iceServers: [
      // Google STUN servers (Keep these, they are free and very reliable for basic NAT)
      { urls: 'stun:stun.l.google.com:19302' },
      { urls: 'stun:stun1.l.google.com:19302' },

      // Your self-hosted CoTURN server on EC2
      // This is the CRITICAL part for cross-network calls
      {
        urls: import.meta.env.VITE_TURN_URL || 'turn:51.20.77.103:3478',
        username: import.meta.env.VITE_TURN_USERNAME || 'meetinguser',
        credential: import.meta.env.VITE_TURN_CREDENTIAL || 'SecurePassword123!',
      },
    ],
    iceCandidatePoolSize: 10,
  };

  // Initialize Socket.IO connection
  useEffect(() => {
    const newSocket = io(SIGNALING_SERVER, {
      transports: ['websocket'],
      reconnection: true,
    });

    newSocket.on('connect', () => {
      console.log('✅ Connected to signaling server');
      newSocket.emit('joinMeeting', { meetingId, userId, username });
    });

    newSocket.on('usersInCall', (users) => {
      console.log('👥 Users already in call:', users);
      const newParticipants = new Map();
      users.forEach(async (user) => {
        newParticipants.set(user.userId, user.username);
        await createPeerConnection(user.userId, true);
      });
      setParticipants(newParticipants);
    });

    newSocket.on(
      'userJoinedCall',
      async ({ userId: remoteUserId, username: remoteUsername }) => {
        console.log(`👤 ${remoteUsername} joined the call`);
        setParticipants((prev) => {
          const newMap = new Map(prev);
          newMap.set(remoteUserId, remoteUsername);
          return newMap;
        });
        // We wait for the new user to initiate the connection
      }
    );

    newSocket.on('userLeftCall', ({ userId: remoteUserId }) => {
      console.log(`👋 User ${remoteUserId} left the call`);
      setParticipants((prev) => {
        const newMap = new Map(prev);
        newMap.delete(remoteUserId);
        return newMap;
      });
      closePeerConnection(remoteUserId);
    });

    newSocket.on('offer', async ({ fromUserId, offer }) => {
      console.log(`📨 Received offer from ${fromUserId}`);
      await handleOffer(fromUserId, offer);
    });

    newSocket.on('answer', async ({ fromUserId, answer }) => {
      console.log(`📨 Received answer from ${fromUserId}`);
      const pc = peerConnectionsRef.current.get(fromUserId);
      if (pc) {
        try {
          await pc.setRemoteDescription(new RTCSessionDescription(answer));
        } catch (err) {
          console.error(
            `❌ Error setting remote answer for ${fromUserId}:`,
            err
          );
        }
      }
    });

    newSocket.on('iceCandidate', async ({ fromUserId, candidate }) => {
      const pc = peerConnectionsRef.current.get(fromUserId);
      if (pc && candidate) {
        const candStr = candidate.candidate || '';
        let type = 'unknown';
        if (candStr.includes('typ relay')) type = 'RELAY';
        else if (candStr.includes('typ srflx')) type = 'STUN';
        else if (candStr.includes('typ host')) type = 'LOCAL';

        console.log(
          `🧊 [ICE] From ${fromUserId} (${type}):`,
          candStr.substring(0, 70) + '...'
        );
        try {
          await pc.addIceCandidate(new RTCIceCandidate(candidate));
        } catch (err) {
          console.error(`❌ Error adding ICE candidate from ${fromUserId}:`, err);
        }
      }
    });

    newSocket.on('disconnect', () => {
      console.log('❌ Disconnected from signaling server');
    });

    socketRef.current = newSocket;
    setSocket(newSocket);

    return () => {
      if (localStreamRef.current) {
        localStreamRef.current.getTracks().forEach((track) => track.stop());
      }
      if (screenStreamRef.current) {
        screenStreamRef.current.getTracks().forEach((track) => track.stop());
      }
      peerConnectionsRef.current.forEach((pc) => pc.close());
      newSocket.disconnect();
    };
  }, [meetingId, userId, username]);

  // Attach local stream to video element when camera/screen state changes
  useEffect(() => {
    if (localVideoRef.current) {
      const stream = screenStreamRef.current || localStreamRef.current;
      if (stream) {
        localVideoRef.current.srcObject = stream;
        console.log('✅ Stream attached to video element');
        localVideoRef.current
          .play()
          .catch((e) => console.error('Video play error:', e));
      } else {
        localVideoRef.current.srcObject = null;
      }
    }
  }, [cameraEnabled, screenEnabled]);

  const createPeerConnection = async (remoteUserId, isInitiator) => {
    let pc = peerConnectionsRef.current.get(remoteUserId);

    if (!pc) {
      console.log(`🏗️ Creating new PeerConnection for ${remoteUserId}`);
      pc = new RTCPeerConnection(ICE_SERVERS);
      peerConnectionsRef.current.set(remoteUserId, pc);
      negotiationStateRef.current.set(remoteUserId, {
        makingOffer: false,
        ignoreOffer: false,
      });

      // Handle incoming remote tracks
      pc.ontrack = (event) => {
        console.log(
          `📹 Received remote track from ${remoteUserId}`,
          event.streams[0]
        );
        setRemoteStreams((prev) => {
          const newMap = new Map(prev);
          newMap.set(remoteUserId, event.streams[0]);
          return newMap;
        });
      };

      // Handle ICE candidates
      pc.onicecandidate = (event) => {
        if (event.candidate) {
          console.log(
            `🧊 ICE candidate for ${remoteUserId}:`,
            event.candidate.type,
            event.candidate.candidate?.substring(0, 50)
          );
          if (socketRef.current) {
            socketRef.current.emit('iceCandidate', {
              toUserId: remoteUserId,
              candidate: event.candidate,
            });
          }
        } else {
          console.log(`✅ ICE gathering complete for ${remoteUserId}`);
        }
      };

      // Handle ICE connection state changes
      pc.oniceconnectionstatechange = () => {
        console.log(
          `❄️ ICE connection state with ${remoteUserId}: ${pc.iceConnectionState}`
        );
        if (pc.iceConnectionState === 'connected') {
          console.log(`✅ P2P connection established with ${remoteUserId}`);
        } else if (pc.iceConnectionState === 'failed') {
          console.error(`❌ ICE connection failed with ${remoteUserId}`);
        }
      };

      // Handle connection state changes
      pc.onconnectionstatechange = () => {
        console.log(
          `🔗 Connection state with ${remoteUserId}: ${pc.connectionState}`
        );
        if (
          pc.connectionState === 'disconnected' ||
          pc.connectionState === 'failed'
        ) {
          console.error(
            `❌ Connection failed/disconnected with ${remoteUserId}`
          );
          closePeerConnection(remoteUserId);
        }
      };
    }

    // Add local tracks to peer connection if not already present
    const stream = screenStreamRef.current || localStreamRef.current;
    if (stream) {
      const existingSenders = pc.getSenders();
      stream.getTracks().forEach((track) => {
        // Check if this exact track is already added
        const trackAlreadyAdded = existingSenders.some(
          (sender) => sender.track && sender.track.id === track.id
        );
        if (!trackAlreadyAdded) {
          console.log(`➕ Adding ${track.kind} track to peer ${remoteUserId}`);
          pc.addTrack(track, stream);
        } else {
          console.log(
            `⏭️ Track ${track.id} already added to peer ${remoteUserId}`
          );
        }
      });
    }

    // If we're the initiator, create and send offer
    if (isInitiator) {
      try {
        const state = negotiationStateRef.current.get(remoteUserId);
        state.makingOffer = true;
        const offer = await pc.createOffer();
        await pc.setLocalDescription(offer);
        socketRef.current.emit('offer', {
          toUserId: remoteUserId,
          offer: pc.localDescription,
        });
      } catch (err) {
        console.error(`❌ Error creating offer for ${remoteUserId}:`, err);
      } finally {
        const state = negotiationStateRef.current.get(remoteUserId);
        if (state) state.makingOffer = false;
      }
    }

    return pc;
  };

  const handleOffer = async (remoteUserId, offer) => {
    try {
      const pc = await createPeerConnection(remoteUserId, false);
      const state = negotiationStateRef.current.get(remoteUserId);
      const polite = isPolite(remoteUserId);

      const offerCollision =
        state.makingOffer || pc.signalingState !== 'stable';
      state.ignoreOffer = !polite && offerCollision;

      if (state.ignoreOffer) {
        console.log(`⚠️ Glare detected: Ignoring offer from ${remoteUserId}`);
        return;
      }

      await pc.setRemoteDescription(new RTCSessionDescription(offer));
      const answer = await pc.createAnswer();
      await pc.setLocalDescription(answer);
      socketRef.current.emit('answer', {
        toUserId: remoteUserId,
        answer: pc.localDescription,
      });
    } catch (err) {
      console.error(`❌ Error handling offer from ${remoteUserId}:`, err);
    }
  };

  const toggleMic = () => {
    if (!inCall) {
      alert('Join the call first!');
      return;
    }

    const newMicState = !micEnabled;
    setMicEnabled(newMicState);

    if (localStreamRef.current) {
      localStreamRef.current.getAudioTracks().forEach((track) => {
        track.enabled = newMicState;
      });
    }
  };

  const closePeerConnection = (remoteUserId) => {
    const pc = peerConnectionsRef.current.get(remoteUserId);
    if (pc) {
      pc.close();
      peerConnectionsRef.current.delete(remoteUserId);
      negotiationStateRef.current.delete(remoteUserId);
    }
    setRemoteStreams((prev) => {
      const newMap = new Map(prev);
      newMap.delete(remoteUserId);
      return newMap;
    });
  };

  const joinCall = async () => {
    if (!socketRef.current) return;

    socketRef.current.emit('joinCall', { meetingId, userId, username });
    inCallRef.current = true;
    setInCall(true);
  };

  const leaveCall = () => {
    if (!socketRef.current) return;

    // Stop all tracks
    if (localStreamRef.current) {
      localStreamRef.current.getTracks().forEach((track) => track.stop());
      localStreamRef.current = null;
    }
    if (screenStreamRef.current) {
      screenStreamRef.current.getTracks().forEach((track) => track.stop());
      screenStreamRef.current = null;
    }

    // Close all peer connections
    peerConnectionsRef.current.forEach((pc) => pc.close());
    peerConnectionsRef.current.clear();

    setRemoteStreams(new Map());
    setCameraEnabled(false);
    setScreenEnabled(false);
    inCallRef.current = false;
    setInCall(false);

    socketRef.current.emit('leaveCall', { meetingId, userId });
  };

  const toggleCamera = async () => {
    if (!inCall) {
      alert('Join the call first!');
      return;
    }

    // Check if mediaDevices API is available
    if (!navigator.mediaDevices || !navigator.mediaDevices.getUserMedia) {
      alert('Camera access not available. Please use HTTPS or localhost.');
      return;
    }

    if (cameraEnabled) {
      // Stop camera
      if (localStreamRef.current) {
        localStreamRef.current.getTracks().forEach((track) => track.stop());
        localStreamRef.current = null;
        if (localVideoRef.current) {
          localVideoRef.current.srcObject = null;
        }
      }
      setCameraEnabled(false);

      // Remove camera tracks from all peer connections
      peerConnectionsRef.current.forEach((pc) => {
        pc.getSenders().forEach((sender) => {
          if (sender.track && sender.track.kind === 'video') {
            pc.removeTrack(sender);
          }
        });
      });
    } else {
      // Start camera
      try {
        console.log('🎥 Requesting camera access...');
        const stream = await navigator.mediaDevices.getUserMedia({
          video: {
            width: { ideal: 1280 },
            height: { ideal: 720 },
          },
          audio: true,
        });

        console.log('✅ Camera access granted:', stream.getTracks());
        localStreamRef.current = stream;
        setCameraEnabled(true);

        // Add camera tracks to all existing peer connections
        peerConnectionsRef.current.forEach(async (pc, remoteUserId) => {
          stream.getTracks().forEach((track) => {
            pc.addTrack(track, stream);
          });

          // Renegotiate
          const offer = await pc.createOffer();
          await pc.setLocalDescription(offer);
          socketRef.current.emit('offer', {
            toUserId: remoteUserId,
            offer: pc.localDescription,
          });
        });
      } catch (error) {
        console.error('❌ Failed to access camera:', error);
        alert('Failed to access camera: ' + error.message);
      }
    }
  };

  const toggleScreen = async () => {
    if (!inCall) {
      alert('Join the call first!');
      return;
    }

    // Check if mediaDevices API is available
    if (!navigator.mediaDevices || !navigator.mediaDevices.getDisplayMedia) {
      alert('Screen sharing not available. Please use HTTPS or localhost.');
      return;
    }

    if (screenEnabled) {
      // Stop screen share
      if (screenStreamRef.current) {
        screenStreamRef.current.getTracks().forEach((track) => track.stop());
        screenStreamRef.current = null;
        if (localVideoRef.current && !localStreamRef.current) {
          localVideoRef.current.srcObject = null;
        } else if (localVideoRef.current && localStreamRef.current) {
          localVideoRef.current.srcObject = localStreamRef.current;
        }
      }
      setScreenEnabled(false);

      // Switch back to camera or remove tracks
      peerConnectionsRef.current.forEach(async (pc, remoteUserId) => {
        // Remove screen tracks
        pc.getSenders().forEach((sender) => {
          if (sender.track && sender.track.kind === 'video') {
            pc.removeTrack(sender);
          }
        });

        // Add camera tracks if camera is enabled
        if (localStreamRef.current) {
          localStreamRef.current.getTracks().forEach((track) => {
            pc.addTrack(track, localStreamRef.current);
          });
        }

        // Renegotiate
        const offer = await pc.createOffer();
        await pc.setLocalDescription(offer);
        socketRef.current.emit('offer', {
          toUserId: remoteUserId,
          offer: pc.localDescription,
        });
      });
    } else {
      // Start screen share
      try {
        const stream = await navigator.mediaDevices.getDisplayMedia({
          video: {
            cursor: 'always',
          },
          audio: true, // Try to capture audio if possible
        });

        screenStreamRef.current = stream;
        if (localVideoRef.current) {
          localVideoRef.current.srcObject = stream;
        }
        setScreenEnabled(true);

        // Handle screen share stop (user clicks browser's stop button)
        stream.getVideoTracks()[0].onended = () => {
          toggleScreen();
        };

        // Replace video tracks in all peer connections
        peerConnectionsRef.current.forEach(async (pc, remoteUserId) => {
          // Remove old video tracks
          const senders = pc.getSenders();
          const videoSender = senders.find(
            (sender) => sender.track && sender.track.kind === 'video'
          );

          if (videoSender) {
            await videoSender.replaceTrack(stream.getVideoTracks()[0]);
          } else {
            pc.addTrack(stream.getVideoTracks()[0], stream);
          }

          // Renegotiate
          const offer = await pc.createOffer();
          await pc.setLocalDescription(offer);
          socketRef.current.emit('offer', {
            toUserId: remoteUserId,
            offer: pc.localDescription,
          });
        });
      } catch (error) {
        console.error('❌ Failed to share screen:', error);
        alert('Failed to share screen: ' + error.message);
      }
    }
  };

  return (
    <div className='h-full flex flex-col bg-dark-950 text-white relative overflow-hidden font-sans uppercase-none'>
      {/* Participant Grid */}
      <div className='flex-1 p-6 overflow-y-auto pb-24'>
        {!inCall ? (
          <div className='h-full flex flex-col items-center justify-center text-center space-y-4'>
            <div className='w-24 h-24 bg-dark-800 rounded-full flex items-center justify-center border-2 border-primary-500/30'>
              <Video className='w-12 h-12 text-primary-500' />
            </div>
            <div>
              <h2 className='text-3xl font-extrabold tracking-tight'>
                Ready to join?
              </h2>
              <p className='text-gray-400 mt-1'>
                No one will see you until you join the call.
              </p>
            </div>
            <button
              onClick={joinCall}
              className='px-10 py-4 bg-primary-600 hover:bg-primary-500 rounded-2xl font-bold text-lg shadow-xl shadow-primary-900/20 transform active:scale-95 transition-all'
            >
              Join Secure Call
            </button>
          </div>
        ) : (
          <div
            className={`grid gap-4 h-full content-center ${participants.size + 1 === 1
              ? 'grid-cols-1'
              : participants.size + 1 === 2
                ? 'grid-cols-1 md:grid-cols-2'
                : participants.size + 1 <= 4
                  ? 'grid-cols-1 md:grid-cols-2 lg:grid-cols-2'
                  : 'grid-cols-1 md:grid-cols-2 lg:grid-cols-3'
              }`}
          >
            {/* Local Video Tile */}
            <div className='relative aspect-video bg-dark-800 rounded-3xl overflow-hidden border border-white/5 shadow-2xl group'>
              {cameraEnabled || screenEnabled ? (
                <video
                  ref={localVideoRef}
                  autoPlay
                  playsInline
                  muted
                  className='w-full h-full object-cover scale-x-[-1]'
                />
              ) : (
                <div className='w-full h-full flex flex-col items-center justify-center bg-gradient-to-br from-dark-800 to-dark-900'>
                  <div className='w-20 h-20 bg-primary-600/20 rounded-full flex items-center justify-center border-2 border-primary-500/30 mb-3'>
                    <span className='text-2xl font-bold text-primary-400'>
                      {username?.[0]?.toUpperCase()}
                    </span>
                  </div>
                  <span className='text-gray-400 font-medium'>You</span>
                </div>
              )}
              <div className='absolute bottom-4 left-4 flex items-center gap-2 bg-black/40 backdrop-blur-md px-3 py-1.5 rounded-full text-sm font-semibold border border-white/10'>
                {!micEnabled && <MicOff className='w-3.5 h-3.5 text-red-400' />}
                <span>You</span>
                {screenEnabled && (
                  <span className='text-primary-400 opacity-80'>
                    (Sharing Screen)
                  </span>
                )}
              </div>
            </div>

            {/* Remote Video Tiles */}
            {Array.from(participants.entries()).map(
              ([remoteUserId, remoteUsername]) => (
                <RemoteTile
                  key={remoteUserId}
                  userId={remoteUserId}
                  username={remoteUsername}
                  stream={remoteStreams.get(remoteUserId)}
                />
              )
            )}
          </div>
        )}
      </div>

      {/* Control Bar (Google Meet Style) */}
      {inCall && (
        <div className='fixed bottom-0 left-0 right-0 h-24 bg-dark-900/80 backdrop-blur-xl border-t border-white/5 flex items-center justify-between px-8 z-50'>
          <div className='hidden md:flex items-center gap-2'>
            <span className='text-sm font-semibold text-gray-300'>
              {meetingId}
            </span>
            <div className='w-1 h-1 bg-gray-500 rounded-full' />
            <span className='text-sm font-medium text-gray-500 uppercase tracking-widest'>
              {participants.size + 1} People
            </span>
          </div>

          <div className='flex items-center gap-4'>
            <button
              onClick={toggleMic}
              className={`p-4 rounded-full transition-all duration-300 border ${micEnabled
                ? 'bg-dark-800 border-white/10 hover:bg-dark-700 text-white'
                : 'bg-red-500 border-red-400/50 hover:bg-red-600 text-white'
                }`}
            >
              {micEnabled ? (
                <Mic className='w-6 h-6' />
              ) : (
                <MicOff className='w-6 h-6' />
              )}
            </button>
            <button
              onClick={toggleCamera}
              className={`p-4 rounded-full transition-all duration-300 border ${cameraEnabled
                ? 'bg-dark-800 border-white/10 hover:bg-dark-700 text-white'
                : 'bg-red-500 border-red-400/50 hover:bg-red-600 text-white'
                }`}
            >
              {cameraEnabled ? (
                <Video className='w-6 h-6' />
              ) : (
                <VideoOff className='w-6 h-6' />
              )}
            </button>
            <button
              onClick={toggleScreen}
              className={`p-4 rounded-full transition-all duration-300 border ${screenEnabled
                ? 'bg-primary-600 border-primary-400/50 hover:bg-primary-700 text-white'
                : 'bg-dark-800 border-white/10 hover:bg-dark-700 text-white'
                }`}
            >
              <Monitor className='w-6 h-6' />
            </button>
            <button
              onClick={leaveCall}
              className='p-4 px-6 bg-red-600 hover:bg-red-700 text-white rounded-full flex items-center gap-2 transition-all duration-300 border border-red-500/50'
            >
              <PhoneOff className='w-6 h-6' />
            </button>
          </div>

          <div className='hidden md:flex items-center gap-4'>
            <button className='p-3 hover:bg-dark-800 rounded-full text-gray-400 transition-colors'>
              <MoreVertical className='w-5 h-5' />
            </button>
          </div>
        </div>
      )}
    </div>
  );
};

// Remote participant tile
const RemoteTile = ({ userId, username, stream }) => {
  const videoRef = useRef(null);

  useEffect(() => {
    if (videoRef.current && stream) {
      videoRef.current.srcObject = stream;
    }
  }, [stream]);

  return (
    <div className='relative aspect-video bg-dark-800 rounded-3xl overflow-hidden border border-white/5 shadow-2xl group transition-transform duration-300 hover:scale-[1.01]'>
      {stream ? (
        <video
          ref={videoRef}
          autoPlay
          playsInline
          key={stream.id} // Re-mount video element if stream changes
          className='w-full h-full object-cover'
        />
      ) : (
        <div className='w-full h-full flex flex-col items-center justify-center bg-gradient-to-tr from-dark-800 to-dark-900'>
          <div className='w-20 h-20 bg-indigo-600/20 rounded-full flex items-center justify-center border-2 border-indigo-500/30 mb-3'>
            <span className='text-2xl font-bold text-indigo-400'>
              {username?.[0]?.toUpperCase() || <User className='w-10 h-10' />}
            </span>
          </div>
          <span className='text-gray-400 font-medium'>
            {username || `Participant ${userId}`}
          </span>
        </div>
      )}
      <div className='absolute bottom-4 left-4 bg-black/40 backdrop-blur-md px-3 py-1.5 rounded-full text-sm font-semibold border border-white/10'>
        {username || `User ${userId}`}
      </div>
    </div>
  );
};

export default VideoPanel;
