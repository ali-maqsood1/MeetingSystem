import { useState, useRef, useEffect } from 'react';
import { 
  Video, VideoOff, Mic, MicOff, Monitor, MonitorOff, Settings, 
  PhoneOff, Users, Grid, Maximize2 
} from 'lucide-react';
import { getUsername, getUserId, getToken } from '../utils/api';

export default function VideoPanel({ meetingId }) {
  const [isVideoOn, setIsVideoOn] = useState(false);
  const [isAudioOn, setIsAudioOn] = useState(false);
  const [isScreenSharing, setIsScreenSharing] = useState(false);
  const [viewMode, setViewMode] = useState('grid');
  const [participants, setParticipants] = useState([]);
  const [screenShares, setScreenShares] = useState([]); // Array of {userId, username, frame}
  const [pinnedScreenShare, setPinnedScreenShare] = useState(null); // userId of pinned share
  
  const localVideoRef = useRef(null);
  const localStreamRef = useRef(null);
  const screenStreamRef = useRef(null);
  const screenPreviewRef = useRef(null);
  const uploadIntervalRef = useRef(null);
  
  const username = getUsername();
  const userId = getUserId();

  // Initialize local camera
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
        
        stream.getVideoTracks()[0].enabled = true;
        stream.getAudioTracks()[0].enabled = true;
        
      } catch (error) {
        console.error('Error accessing media devices:', error);
      }
    };

    initMedia();

    return () => {
      if (localStreamRef.current) {
        localStreamRef.current.getTracks().forEach(track => track.stop());
      }
      if (screenStreamRef.current) {
        screenStreamRef.current.getTracks().forEach(track => track.stop());
      }
    };
  }, []);

  // Poll for screen share frames
  useEffect(() => {
    const pollScreenShare = async () => {
      try {
        const token = getToken();
        if (!token) return;
        
        const response = await fetch(`http://localhost:8080/api/v1/meetings/${meetingId}/screenshare/frame`, {
          headers: { 'Authorization': `Bearer ${token}` }
        });
        
        if (!response.ok) {
          if (response.status !== 404) {
            console.error('Screen share poll failed:', response.status, response.statusText);
          }
          setScreenShares([]); // Clear if no shares active
          return;
        }
        
        const data = await response.json();
        
        if (data.success && data.frame) {
          const shareUserId = parseInt(data.user_id);
          // Don't show our own screen share in the viewer
          if (shareUserId !== parseInt(userId)) {
            setScreenShares([{
              userId: shareUserId,
              username: data.username,
              frame: `data:image/jpeg;base64,${data.frame}`
            }]);
          } else {
            setScreenShares([]);
          }
        } else {
          setScreenShares([]);
        }
      } catch (error) {
        // No screen share active, ignore
      }
    };

    const interval = setInterval(pollScreenShare, 500); // Poll every 500ms
    return () => clearInterval(interval);
  }, [meetingId, userId]);

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

  // Capture and upload screen frame
  const captureAndUploadFrame = async (stream) => {
    console.log('🎥 Setting up screen capture from stream');
    
    // Create a hidden video element just for capture
    const video = document.createElement('video');
    video.srcObject = stream;
    video.muted = true;
    video.autoplay = true;
    video.playsInline = true;
    
    // CRITICAL: Start playing immediately
    video.play().catch(err => console.error('Video play error:', err));
    
    // Wait for video to be fully ready (with timeout)
    await new Promise((resolve) => {
      if (video.readyState >= 2) {
        console.log('✅ Capture video ready immediately:', video.videoWidth, 'x', video.videoHeight);
        resolve();
      } else {
        console.log('⏳ Waiting for capture video to load...');
        
        const timeout = setTimeout(() => {
          console.warn('⚠️ Timeout waiting for video, dimensions:', video.videoWidth, 'x', video.videoHeight);
          resolve();
        }, 3000);
        
        video.addEventListener('loadeddata', () => {
          clearTimeout(timeout);
          console.log('✅ Capture video loaded:', video.videoWidth, 'x', video.videoHeight);
          resolve();
        }, { once: true });
      }
    });
    
    const canvas = document.createElement('canvas');
    const ctx = canvas.getContext('2d');
    
    // Set canvas size (reduce for better performance)
    canvas.width = 1280;
    canvas.height = 720;
    
    const uploadFrame = async () => {
      try {
        // Check if video has actual dimensions
        if (video.videoWidth === 0 || video.videoHeight === 0) {
          console.warn('⚠️ Video has no dimensions yet, skipping frame');
          return;
        }
        
        // Draw current video frame to canvas
        ctx.drawImage(video, 0, 0, canvas.width, canvas.height);
        
        // Convert to JPEG blob
        const blob = await new Promise(resolve => {
          canvas.toBlob(resolve, 'image/jpeg', 0.7); // 70% quality
        });
        
        // Log blob size for debugging
        if (blob.size < 10000) {
          console.warn('⚠️ Frame too small:', blob.size, 'bytes - might be blank');
        }
        
        // Convert blob to base64
        const reader = new FileReader();
        reader.onloadend = async () => {
          const base64 = reader.result.split(',')[1]; // Remove data:image/jpeg;base64,
          
          // Upload to server
          await fetch(`http://localhost:8080/api/v1/meetings/${meetingId}/screenshare/frame`, {
            method: 'POST',
            headers: {
              'Content-Type': 'application/json',
              'Authorization': `Bearer ${getToken()}`
            },
            body: JSON.stringify({
              frame: base64,
              width: canvas.width,
              height: canvas.height
            })
          });
        };
        reader.readAsDataURL(blob);
        
      } catch (error) {
        console.error('Error uploading frame:', error);
      }
    };
    
    // Upload frames every 200ms (5 FPS)
    uploadIntervalRef.current = setInterval(uploadFrame, 200);
  };

  // Toggle screen sharing
  const toggleScreenShare = async () => {
    if (!isScreenSharing) {
      try {
        // Start screen capture
        const stream = await navigator.mediaDevices.getDisplayMedia({
          video: {
            width: { ideal: 1920 },
            height: { ideal: 1080 },
            frameRate: { ideal: 10 }
          },
          audio: false
        });
        
        screenStreamRef.current = stream;
        setIsScreenSharing(true);
        
        // Show local preview IMMEDIATELY - must be synchronous
        if (screenPreviewRef.current) {
          const previewVideo = screenPreviewRef.current;
          previewVideo.srcObject = stream;
          previewVideo.muted = true;
          previewVideo.autoplay = true;
          
          // Force play immediately (critical for MediaStream)
          const playPromise = previewVideo.play();
          if (playPromise !== undefined) {
            playPromise
              .then(() => console.log('✅ Preview playing successfully'))
              .catch(err => {
                console.error('❌ Preview play error:', err);
                // Retry after short delay
                setTimeout(() => previewVideo.play(), 100);
              });
          }
        }
        
        // Handle user stopping screen share via browser UI
        stream.getVideoTracks()[0].onended = () => {
          toggleScreenShare(); // Stop sharing
        };
        
        // Notify server (non-blocking)
        fetch(`http://localhost:8080/api/v1/meetings/${meetingId}/screenshare/start`, {
          method: 'POST',
          headers: {
            'Content-Type': 'application/json',
            'Authorization': `Bearer ${getToken()}`
          }
        }).then(response => response.json())
          .then(async data => {
            if (data.success) {
              console.log('🚀 Backend confirmed, starting frame capture...');
              // Start capturing and uploading frames
              await captureAndUploadFrame(stream);
            } else {
              console.error('Failed to notify server:', data.error);
              // Revert state if backend rejects
              setIsScreenSharing(false);
              if (screenStreamRef.current) {
                screenStreamRef.current.getTracks().forEach(track => track.stop());
                screenStreamRef.current = null;
              }
              if (screenPreviewRef.current) {
                screenPreviewRef.current.srcObject = null;
              }
              alert('Screen share already active. Please stop the existing share first.');
            }
          })
          .catch(error => {
            console.error('Server notification failed:', error);
            // Revert state on error
            setIsScreenSharing(false);
            if (screenStreamRef.current) {
              screenStreamRef.current.getTracks().forEach(track => track.stop());
              screenStreamRef.current = null;
            }
            if (screenPreviewRef.current) {
              screenPreviewRef.current.srcObject = null;
            }
          });
        
      } catch (error) {
        console.error('Error sharing screen:', error);
        alert('Could not start screen sharing. Check permissions.');
      }
    } else {
      // Stop screen sharing
      if (uploadIntervalRef.current) {
        clearInterval(uploadIntervalRef.current);
        uploadIntervalRef.current = null;
      }
      
      if (screenStreamRef.current) {
        screenStreamRef.current.getTracks().forEach(track => track.stop());
        screenStreamRef.current = null;
      }
      
      // Clear preview
      if (screenPreviewRef.current) {
        screenPreviewRef.current.srcObject = null;
      }
      
      // Notify server
      await fetch(`http://localhost:8080/api/v1/meetings/${meetingId}/screenshare/stop`, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
          'Authorization': `Bearer ${getToken()}`
        }
      });
      
      setIsScreenSharing(false);
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
              {isScreenSharing && (
                <span className="ml-2 text-green-400">
                  • Sharing screen
                </span>
              )}
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
        {/* Your Screen Share Preview */}
        {isScreenSharing && (
          <div className="mb-4 bg-black rounded-2xl overflow-hidden border-2 border-blue-500">
            <div className="relative aspect-video bg-gray-900">
              <video 
                ref={screenPreviewRef}
                autoPlay
                muted
                playsInline
                className="w-full h-full object-contain bg-transparent"
                style={{ minHeight: '200px' }}
              />
              <div className="absolute top-4 left-4 bg-black/80 px-3 py-2 rounded-lg">
                <div className="flex items-center gap-2">
                  <Monitor className="w-4 h-4 text-blue-400" />
                  <span className="text-white font-semibold">
                    Your screen (Preview)
                  </span>
                </div>
              </div>
            </div>
          </div>
        )}

        {/* Screen Share Viewers (other people's screens) */}
        {screenShares.length > 0 && (
          <div className="mb-4 grid gap-4 grid-cols-1 lg:grid-cols-2">
            {screenShares.map((share) => {
              const isPinned = pinnedScreenShare === share.userId;
              const sizeClass = isPinned ? 'lg:col-span-2' : '';
              
              return (
                <div 
                  key={share.userId}
                  className={`bg-black rounded-2xl overflow-hidden border-2 border-green-500 ${sizeClass}`}
                >
                  <div className="relative aspect-video">
                    <img 
                      src={share.frame} 
                      alt={`${share.username}'s screen`}
                      className="w-full h-full object-contain"
                    />
                    <div className="absolute top-4 left-4 bg-black/80 px-3 py-2 rounded-lg">
                      <div className="flex items-center gap-2">
                        <Monitor className="w-4 h-4 text-green-400" />
                        <span className="text-white font-semibold">
                          {share.username}'s screen
                        </span>
                      </div>
                    </div>
                    
                    {/* Pin/Expand button */}
                    <button
                      onClick={() => setPinnedScreenShare(isPinned ? null : share.userId)}
                      className="absolute top-4 right-4 bg-black/80 hover:bg-black p-2 rounded-lg transition-all"
                      title={isPinned ? 'Unpin' : 'Pin to enlarge'}
                    >
                      {isPinned ? (
                        <Maximize2 className="w-5 h-5 text-green-400 rotate-180" />
                      ) : (
                        <Maximize2 className="w-5 h-5 text-white" />
                      )}
                    </button>
                  </div>
                </div>
              );
            })}
          </div>
        )}

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
                  {isScreenSharing && <Monitor className="w-4 h-4 text-green-400" />}
                </div>
                <div className="flex items-center gap-2">
                  <div className="w-2 h-2 bg-green-500 rounded-full animate-pulse"></div>
                  <span className="text-xs text-gray-300">Live</span>
                </div>
              </div>
            </div>
          </div>
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
            onClick={toggleScreenShare}
            className={`p-4 rounded-full transition-all ${
              isScreenSharing
                ? 'bg-green-600 hover:bg-green-700 text-white'
                : 'bg-gray-700 hover:bg-gray-600 text-white'
            }`}
            title={isScreenSharing ? 'Stop sharing' : 'Share screen'}
          >
            {isScreenSharing ? <MonitorOff className="w-6 h-6" /> : <Monitor className="w-6 h-6" />}
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

        <p className="text-xs text-gray-500 text-center mt-3">
          {isScreenSharing ? 'Sharing your screen at 5 FPS' : 'Click monitor icon to share screen'}
        </p>
      </div>
    </div>
  );
}