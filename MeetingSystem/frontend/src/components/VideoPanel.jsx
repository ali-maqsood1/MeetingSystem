import { useState, useRef, useEffect } from 'react';
import { 
  Video, VideoOff, Mic, MicOff, Monitor, Settings, 
  PhoneOff, Users, Grid, Maximize2 
} from 'lucide-react';
import { getUsername, getUserId, getToken } from '../utils/api';

export default function VideoPanel({ meetingId }) {
  const [isVideoOn, setIsVideoOn] = useState(false);
  const [isAudioOn, setIsAudioOn] = useState(false);
  const [viewMode, setViewMode] = useState('grid');
  const [participants, setParticipants] = useState([]);
  
  const localVideoRef = useRef(null);
  const localStreamRef = useRef(null);
  const peerConnectionsRef = useRef({});
  const remoteStreamsRef = useRef({});
  
  const username = getUsername();
  const userId = getUserId();

  // ICE servers (STUN for NAT traversal)
  const iceServers = {
    iceServers: [
      { urls: 'stun:stun.l.google.com:19302' },
      { urls: 'stun:stun1.l.google.com:19302' },
      { urls: 'stun:stun2.l.google.com:19302' }
    ]
  };

  // Initialize local media
  useEffect(() => {
    const initMedia = async () => {
      try {
        const stream = await navigator.mediaDevices.getUserMedia({
          video: { width: 640, height: 480, frameRate: 30 },
          audio: true
        });
        
        localStreamRef.current = stream;
        if (localVideoRef.current) {
          localVideoRef.current.srcObject = stream;
        }
        
        setIsVideoOn(true);
        setIsAudioOn(true);
        
        // Enable tracks
        stream.getVideoTracks()[0].enabled = true;
        stream.getAudioTracks()[0].enabled = true;
        
      } catch (error) {
        console.error('Error accessing media devices:', error);
        alert('Could not access camera/microphone. Check permissions.');
      }
    };

    initMedia();

    return () => {
      // Cleanup on unmount
      if (localStreamRef.current) {
        localStreamRef.current.getTracks().forEach(track => track.stop());
      }
      Object.values(peerConnectionsRef.current).forEach(pc => pc.close());
    };
  }, []);

  // Fetch participants and establish connections
  useEffect(() => {
    const fetchParticipants = async () => {
      try {
        const response = await fetch(`/api/v1/meetings/${meetingId}/participants`, {
          headers: { 'Authorization': `Bearer ${getToken()}` }
        });
        const data = await response.json();
        
        if (data.success && data.participants) {
          setParticipants(data.participants.filter(p => p.user_id !== parseInt(userId)));
          
          // Create peer connections for new participants
          for (const participant of data.participants) {
            if (participant.user_id !== parseInt(userId)) {
              await createPeerConnection(participant.user_id, participant.username);
            }
          }
        }
      } catch (error) {
        console.error('Error fetching participants:', error);
      }
    };

    fetchParticipants();
    const interval = setInterval(fetchParticipants, 5000);
    return () => clearInterval(interval);
  }, [meetingId, userId]);

  // Poll for WebRTC signals
  useEffect(() => {
    const pollSignals = async () => {
      try {
        const response = await fetch(`/api/v1/meetings/${meetingId}/webrtc/signals`, {
          headers: { 'Authorization': `Bearer ${getToken()}` }
        });
        const data = await response.json();
        
        if (data.success && data.signals && data.signals.length > 0) {
          for (const signal of data.signals) {
            await handleIncomingSignal(signal);
          }
        }
      } catch (error) {
        console.error('Error polling signals:', error);
      }
    };

    const interval = setInterval(pollSignals, 1000);
    return () => clearInterval(interval);
  }, [meetingId]);

  // Create peer connection for a remote user
  const createPeerConnection = async (remoteUserId, remoteUsername) => {
    if (peerConnectionsRef.current[remoteUserId]) {
      return peerConnectionsRef.current[remoteUserId];
    }

    const pc = new RTCPeerConnection(iceServers);
    peerConnectionsRef.current[remoteUserId] = pc;

    // Add local stream tracks
    if (localStreamRef.current) {
      localStreamRef.current.getTracks().forEach(track => {
        pc.addTrack(track, localStreamRef.current);
      });
    }

    // Handle incoming remote stream
    pc.ontrack = (event) => {
      console.log('Received remote track from user', remoteUserId);
      const [remoteStream] = event.streams;
      remoteStreamsRef.current[remoteUserId] = remoteStream;
      
      // Force re-render to display remote video
      setParticipants(prev => [...prev]);
    };

    // Handle ICE candidates
    pc.onicecandidate = (event) => {
      if (event.candidate) {
        sendSignal({
          type: 'ice-candidate',
          candidate: event.candidate.candidate,
          sdpMid: event.candidate.sdpMid,
          sdpMLineIndex: event.candidate.sdpMLineIndex,
          to: remoteUserId
        });
      }
    };

    // Handle connection state
    pc.onconnectionstatechange = () => {
      console.log(`Connection state with ${remoteUserId}: ${pc.connectionState}`);
      if (pc.connectionState === 'failed' || pc.connectionState === 'disconnected') {
        delete peerConnectionsRef.current[remoteUserId];
        delete remoteStreamsRef.current[remoteUserId];
      }
    };

    // Create and send offer
    try {
      const offer = await pc.createOffer();
      await pc.setLocalDescription(offer);
      
      await sendSignal({
        type: 'offer',
        sdp: offer.sdp,
        to: remoteUserId
      });
      
      console.log('Sent offer to user', remoteUserId);
    } catch (error) {
      console.error('Error creating offer:', error);
    }

    return pc;
  };

  // Send WebRTC signal to backend
  const sendSignal = async (signal) => {
    try {
      await fetch(`/api/v1/meetings/${meetingId}/webrtc/signal`, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
          'Authorization': `Bearer ${getToken()}`
        },
        body: JSON.stringify(signal)
      });
    } catch (error) {
      console.error('Error sending signal:', error);
    }
  };

  // Handle incoming WebRTC signals
  const handleIncomingSignal = async (signal) => {
    const remoteUserId = signal.from;

    try {
      if (signal.type === 'offer') {
        // Received offer - create answer
        let pc = peerConnectionsRef.current[remoteUserId];
        
        if (!pc) {
          pc = new RTCPeerConnection(iceServers);
          peerConnectionsRef.current[remoteUserId] = pc;

          // Add local tracks
          if (localStreamRef.current) {
            localStreamRef.current.getTracks().forEach(track => {
              pc.addTrack(track, localStreamRef.current);
            });
          }

          // Handle remote stream
          pc.ontrack = (event) => {
            const [remoteStream] = event.streams;
            remoteStreamsRef.current[remoteUserId] = remoteStream;
            setParticipants(prev => [...prev]);
          };

          // Handle ICE candidates
          pc.onicecandidate = (event) => {
            if (event.candidate) {
              sendSignal({
                type: 'ice-candidate',
                candidate: event.candidate.candidate,
                sdpMid: event.candidate.sdpMid,
                sdpMLineIndex: event.candidate.sdpMLineIndex,
                to: remoteUserId
              });
            }
          };
        }

        await pc.setRemoteDescription(new RTCSessionDescription({
          type: 'offer',
          sdp: signal.sdp
        }));

        const answer = await pc.createAnswer();
        await pc.setLocalDescription(answer);

        await sendSignal({
          type: 'answer',
          sdp: answer.sdp,
          to: remoteUserId
        });

        console.log('Sent answer to user', remoteUserId);

      } else if (signal.type === 'answer') {
        // Received answer
        const pc = peerConnectionsRef.current[remoteUserId];
        if (pc) {
          await pc.setRemoteDescription(new RTCSessionDescription({
            type: 'answer',
            sdp: signal.sdp
          }));
          console.log('Set remote answer from user', remoteUserId);
        }

      } else if (signal.type === 'ice-candidate') {
        // Received ICE candidate
        const pc = peerConnectionsRef.current[remoteUserId];
        if (pc && signal.candidate) {
          await pc.addIceCandidate(new RTCIceCandidate({
            candidate: signal.candidate,
            sdpMid: signal.sdpMid,
            sdpMLineIndex: signal.sdpMLineIndex
          }));
          console.log('Added ICE candidate from user', remoteUserId);
        }
      }
    } catch (error) {
      console.error('Error handling signal:', error);
    }
  };

  // Toggle video
  const toggleVideo = () => {
    if (localStreamRef.current) {
      const videoTrack = localStreamRef.current.getVideoTracks()[0];
      if (videoTrack) {
        videoTrack.enabled = !videoTrack.enabled;
        setIsVideoOn(videoTrack.enabled);
      }
    }
  };

  // Toggle audio
  const toggleAudio = () => {
    if (localStreamRef.current) {
      const audioTrack = localStreamRef.current.getAudioTracks()[0];
      if (audioTrack) {
        audioTrack.enabled = !audioTrack.enabled;
        setIsAudioOn(audioTrack.enabled);
      }
    }
  };

  return (
    <div className="h-full flex flex-col bg-gray-900">
      {/* Header */}
      <div className="px-6 py-4 border-b border-gray-700 bg-gray-800/50">
        <div className="flex justify-between items-center">
          <div>
            <h2 className="text-xl font-bold text-white flex items-center gap-2">
              <Video className="w-6 h-6" />
              Video Conference
            </h2>
            <p className="text-sm text-gray-400 mt-1">
              <Users className="w-4 h-4 inline mr-1" />
              {participants.length + 1} participant(s)
            </p>
          </div>

          {/* View Mode Toggle */}
          <div className="flex gap-2">
            <button
              onClick={() => setViewMode('grid')}
              className={`p-2 rounded-lg transition-all ${
                viewMode === 'grid'
                  ? 'bg-blue-600 text-white'
                  : 'bg-gray-800 text-gray-400 hover:bg-gray-700'
              }`}
            >
              <Grid className="w-5 h-5" />
            </button>
            <button
              onClick={() => setViewMode('speaker')}
              className={`p-2 rounded-lg transition-all ${
                viewMode === 'speaker'
                  ? 'bg-blue-600 text-white'
                  : 'bg-gray-800 text-gray-400 hover:bg-gray-700'
              }`}
            >
              <Maximize2 className="w-5 h-5" />
            </button>
          </div>
        </div>
      </div>

      {/* Video Grid */}
      <div className="flex-1 overflow-y-auto p-6">
        <div className={`grid gap-4 h-full ${
          viewMode === 'grid'
            ? 'grid-cols-1 md:grid-cols-2 lg:grid-cols-3'
            : 'grid-cols-1'
        }`}>
          {/* Local Video */}
          <div className="relative bg-gray-800 rounded-2xl overflow-hidden border-2 border-blue-500 aspect-video">
            <video
              ref={localVideoRef}
              autoPlay
              muted
              playsInline
              className="w-full h-full object-cover"
            />
            {!isVideoOn && (
              <div className="absolute inset-0 flex items-center justify-center bg-gray-800">
                <div className="text-center">
                  <div className="w-24 h-24 bg-gradient-to-br from-blue-500 to-purple-600 rounded-full flex items-center justify-center mx-auto mb-4">
                    <span className="text-3xl font-bold text-white">
                      {username[0]?.toUpperCase()}
                    </span>
                  </div>
                  <p className="text-white font-semibold">{username}</p>
                </div>
              </div>
            )}

            <div className="absolute bottom-0 left-0 right-0 bg-gradient-to-t from-black/80 to-transparent p-4">
              <div className="flex items-center justify-between">
                <div className="flex items-center gap-2">
                  <span className="text-white font-semibold">{username} (You)</span>
                  {!isAudioOn && <MicOff className="w-4 h-4 text-red-400" />}
                </div>
                <div className="flex items-center gap-2">
                  <div className="w-2 h-2 bg-green-500 rounded-full animate-pulse"></div>
                  <span className="text-xs text-gray-300">Live</span>
                </div>
              </div>
            </div>
          </div>

          {/* Remote Participants */}
          {participants.map((participant) => (
            <RemoteVideo
              key={participant.user_id}
              participant={participant}
              stream={remoteStreamsRef.current[participant.user_id]}
            />
          ))}
        </div>
      </div>

      {/* Control Bar */}
      <div className="px-6 py-4 border-t border-gray-700 bg-gray-800/80">
        <div className="flex justify-center items-center gap-4">
          <button
            onClick={toggleVideo}
            className={`p-4 rounded-full transition-all ${
              isVideoOn
                ? 'bg-gray-700 hover:bg-gray-600 text-white'
                : 'bg-red-600 hover:bg-red-700 text-white'
            }`}
            title={isVideoOn ? 'Turn off camera' : 'Turn on camera'}
          >
            {isVideoOn ? <Video className="w-6 h-6" /> : <VideoOff className="w-6 h-6" />}
          </button>

          <button
            onClick={toggleAudio}
            className={`p-4 rounded-full transition-all ${
              isAudioOn
                ? 'bg-gray-700 hover:bg-gray-600 text-white'
                : 'bg-red-600 hover:bg-red-700 text-white'
            }`}
            title={isAudioOn ? 'Mute' : 'Unmute'}
          >
            {isAudioOn ? <Mic className="w-6 h-6" /> : <MicOff className="w-6 h-6" />}
          </button>

          <button
            className="p-4 rounded-full bg-gray-700 hover:bg-gray-600 text-white transition-all"
          >
            <Settings className="w-6 h-6" />
          </button>

          <button
            className="p-4 rounded-full bg-red-600 hover:bg-red-700 text-white transition-all ml-4"
            onClick={() => window.location.href = '/lobby'}
          >
            <PhoneOff className="w-6 h-6" />
          </button>
        </div>
      </div>
    </div>
  );
}

// Remote video component
function RemoteVideo({ participant, stream }) {
  const videoRef = useRef(null);

  useEffect(() => {
    if (videoRef.current && stream) {
      videoRef.current.srcObject = stream;
    }
  }, [stream]);

  return (
    <div className="relative bg-gray-800 rounded-2xl overflow-hidden border-2 border-gray-700 aspect-video">
      {stream ? (
        <video
          ref={videoRef}
          autoPlay
          playsInline
          className="w-full h-full object-cover"
        />
      ) : (
        <div className="w-full h-full flex items-center justify-center">
          <div className="text-center">
            <div className="w-24 h-24 bg-gradient-to-br from-blue-500 to-cyan-600 rounded-full flex items-center justify-center mx-auto mb-4">
              <span className="text-3xl font-bold text-white">
                {participant.username?.[0]?.toUpperCase() || '?'}
              </span>
            </div>
            <p className="text-white font-semibold">{participant.username}</p>
          </div>
        </div>
      )}

      <div className="absolute bottom-0 left-0 right-0 bg-gradient-to-t from-black/80 to-transparent p-4">
        <span className="text-white font-semibold">{participant.username}</span>
      </div>
    </div>
  );
}