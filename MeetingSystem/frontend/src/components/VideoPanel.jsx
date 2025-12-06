import { useState, useRef, useEffect } from 'react';
import { 
  Video, VideoOff, Mic, MicOff, Monitor, Settings, 
  PhoneOff, Users, Grid, Maximize2 
} from 'lucide-react';
import { getUsername } from '../utils/api';

export default function VideoPanel({ meetingId }) {
  const [isVideoOn, setIsVideoOn] = useState(false);
  const [isAudioOn, setIsAudioOn] = useState(false);
  const [isScreenSharing, setIsScreenSharing] = useState(false);
  const [viewMode, setViewMode] = useState('grid'); // 'grid' or 'speaker'
  const localVideoRef = useRef(null);
  const username = getUsername();

  // Mock participants - replace with actual WebRTC implementation
  const [participants, setParticipants] = useState([
    { id: 1, name: 'Alice', isVideoOn: true, isAudioOn: true, isSpeaking: false },
    { id: 2, name: 'Bob', isVideoOn: false, isAudioOn: true, isSpeaking: true },
    { id: 3, name: 'Charlie', isVideoOn: true, isAudioOn: false, isSpeaking: false },
  ]);

  useEffect(() => {
    // Initialize video/audio streams
    return () => {
      // Cleanup streams
    };
  }, []);

  const toggleVideo = async () => {
    try {
      if (!isVideoOn) {
        const stream = await navigator.mediaDevices.getUserMedia({ video: true });
        if (localVideoRef.current) {
          localVideoRef.current.srcObject = stream;
        }
        setIsVideoOn(true);
      } else {
        const stream = localVideoRef.current?.srcObject;
        if (stream) {
          stream.getTracks().forEach(track => track.stop());
        }
        setIsVideoOn(false);
      }
    } catch (error) {
      console.error('Error accessing camera:', error);
      alert('Could not access camera. Please check permissions.');
    }
  };

  const toggleAudio = async () => {
    try {
      if (!isAudioOn) {
        const stream = await navigator.mediaDevices.getUserMedia({ audio: true });
        setIsAudioOn(true);
      } else {
        // Stop audio tracks
        setIsAudioOn(false);
      }
    } catch (error) {
      console.error('Error accessing microphone:', error);
      alert('Could not access microphone. Please check permissions.');
    }
  };

  const toggleScreenShare = async () => {
    try {
      if (!isScreenSharing) {
        const stream = await navigator.mediaDevices.getDisplayMedia({ video: true });
        setIsScreenSharing(true);
      } else {
        setIsScreenSharing(false);
      }
    } catch (error) {
      console.error('Error sharing screen:', error);
    }
  };

  return (
    <div className="h-full flex flex-col bg-dark-900">
      {/* Header */}
      <div className="px-6 py-4 border-b border-dark-700 bg-dark-800/50 backdrop-blur-sm">
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
                  ? 'bg-primary-600 text-white'
                  : 'bg-dark-800 text-gray-400 hover:bg-dark-700'
              }`}
              title="Grid view"
            >
              <Grid className="w-5 h-5" />
            </button>
            <button
              onClick={() => setViewMode('speaker')}
              className={`p-2 rounded-lg transition-all ${
                viewMode === 'speaker'
                  ? 'bg-primary-600 text-white'
                  : 'bg-dark-800 text-gray-400 hover:bg-dark-700'
              }`}
              title="Speaker view"
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
          {/* Local User Video */}
          <div className="relative bg-dark-800 rounded-2xl overflow-hidden border-2 border-primary-500 aspect-video">
            <video
              ref={localVideoRef}
              autoPlay
              muted
              playsInline
              className="w-full h-full object-cover"
            />
            {!isVideoOn && (
              <div className="absolute inset-0 flex items-center justify-center bg-dark-800">
                <div className="text-center">
                  <div className="w-24 h-24 bg-gradient-to-br from-primary-500 to-purple-600 rounded-full flex items-center justify-center mx-auto mb-4">
                    <span className="text-3xl font-bold text-white">
                      {username[0].toUpperCase()}
                    </span>
                  </div>
                  <p className="text-white font-semibold">{username}</p>
                </div>
              </div>
            )}

            {/* User Info Overlay */}
            <div className="absolute bottom-0 left-0 right-0 bg-gradient-to-t from-black/80 to-transparent p-4">
              <div className="flex items-center justify-between">
                <div className="flex items-center gap-2">
                  <span className="text-white font-semibold">{username} (You)</span>
                  {!isAudioOn && (
                    <MicOff className="w-4 h-4 text-red-400" />
                  )}
                </div>
                <div className="flex items-center gap-2">
                  <div className="w-2 h-2 bg-green-500 rounded-full animate-pulse"></div>
                  <span className="text-xs text-gray-300">Live</span>
                </div>
              </div>
            </div>
          </div>

          {/* Other Participants */}
          {participants.map((participant) => (
            <div
              key={participant.id}
              className={`relative bg-dark-800 rounded-2xl overflow-hidden border-2 aspect-video transition-all ${
                participant.isSpeaking
                  ? 'border-green-500 shadow-lg shadow-green-500/50'
                  : 'border-dark-700'
              }`}
            >
              {participant.isVideoOn ? (
                <div className="w-full h-full bg-gradient-to-br from-gray-700 to-gray-800 flex items-center justify-center">
                  <span className="text-gray-500 text-sm">Camera feed</span>
                </div>
              ) : (
                <div className="w-full h-full flex items-center justify-center bg-dark-800">
                  <div className="text-center">
                    <div className="w-24 h-24 bg-gradient-to-br from-blue-500 to-cyan-600 rounded-full flex items-center justify-center mx-auto mb-4">
                      <span className="text-3xl font-bold text-white">
                        {participant.name[0]}
                      </span>
                    </div>
                    <p className="text-white font-semibold">{participant.name}</p>
                  </div>
                </div>
              )}

              {/* Participant Info Overlay */}
              <div className="absolute bottom-0 left-0 right-0 bg-gradient-to-t from-black/80 to-transparent p-4">
                <div className="flex items-center justify-between">
                  <div className="flex items-center gap-2">
                    <span className="text-white font-semibold">{participant.name}</span>
                    {!participant.isAudioOn && (
                      <MicOff className="w-4 h-4 text-red-400" />
                    )}
                  </div>
                  {participant.isSpeaking && (
                    <div className="flex items-center gap-2">
                      <div className="flex gap-1">
                        <div className="w-1 h-4 bg-green-500 rounded-full animate-pulse"></div>
                        <div className="w-1 h-4 bg-green-500 rounded-full animate-pulse" style={{ animationDelay: '0.1s' }}></div>
                        <div className="w-1 h-4 bg-green-500 rounded-full animate-pulse" style={{ animationDelay: '0.2s' }}></div>
                      </div>
                    </div>
                  )}
                </div>
              </div>
            </div>
          ))}
        </div>
      </div>

      {/* Control Bar */}
      <div className="px-6 py-4 border-t border-dark-700 bg-dark-800/80 backdrop-blur-sm">
        <div className="flex justify-center items-center gap-4">
          {/* Video Toggle */}
          <button
            onClick={toggleVideo}
            className={`p-4 rounded-full transition-all ${
              isVideoOn
                ? 'bg-dark-700 hover:bg-dark-600 text-white'
                : 'bg-red-600 hover:bg-red-700 text-white'
            }`}
            title={isVideoOn ? 'Turn off camera' : 'Turn on camera'}
          >
            {isVideoOn ? (
              <Video className="w-6 h-6" />
            ) : (
              <VideoOff className="w-6 h-6" />
            )}
          </button>

          {/* Audio Toggle */}
          <button
            onClick={toggleAudio}
            className={`p-4 rounded-full transition-all ${
              isAudioOn
                ? 'bg-dark-700 hover:bg-dark-600 text-white'
                : 'bg-red-600 hover:bg-red-700 text-white'
            }`}
            title={isAudioOn ? 'Mute microphone' : 'Unmute microphone'}
          >
            {isAudioOn ? (
              <Mic className="w-6 h-6" />
            ) : (
              <MicOff className="w-6 h-6" />
            )}
          </button>

          {/* Screen Share */}
          <button
            onClick={toggleScreenShare}
            className={`p-4 rounded-full transition-all ${
              isScreenSharing
                ? 'bg-primary-600 hover:bg-primary-700 text-white'
                : 'bg-dark-700 hover:bg-dark-600 text-white'
            }`}
            title={isScreenSharing ? 'Stop sharing' : 'Share screen'}
          >
            <Monitor className="w-6 h-6" />
          </button>

          {/* Settings */}
          <button
            className="p-4 rounded-full bg-dark-700 hover:bg-dark-600 text-white transition-all"
            title="Settings"
          >
            <Settings className="w-6 h-6" />
          </button>

          {/* Leave Call */}
          <button
            className="p-4 rounded-full bg-red-600 hover:bg-red-700 text-white transition-all ml-4"
            title="Leave call"
          >
            <PhoneOff className="w-6 h-6" />
          </button>
        </div>

        <p className="text-xs text-gray-500 text-center mt-3">
          Video conferencing • End-to-end encrypted
        </p>
      </div>
    </div>
  );
}