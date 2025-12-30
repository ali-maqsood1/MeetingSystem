import { useState, useEffect, useRef } from 'react';
import { useParams, useNavigate } from 'react-router-dom';
import {
  MessageSquare,
  Palette,
  FileText,
  Video as VideoIcon,
  Send,
  Download,
  Upload,
  Users,
  X,
  Maximize2,
} from 'lucide-react';
import ChatPanel from '../components/ChatPanel';
import WhiteboardPanel from '../components/WhiteboardPanel';
import FilesPanel from '../components/FilesPanel';
import VideoPanel from '../components/VideoPanel';
import api, { getToken, getUsername, getUserId } from '../utils/api';

export default function Meeting() {
  const { id: meetingId } = useParams();
  const navigate = useNavigate();
  const [activePanel, setActivePanel] = useState('chat');
  const [meetingTitle, setMeetingTitle] = useState('Meeting Room');
  const [meetingCreatorId, setMeetingCreatorId] = useState(null);
  const [isSidebarOpen, setIsSidebarOpen] = useState(true);
  const currentUserId = getUserId();

  useEffect(() => {
    if (!getToken() || !meetingId) {
      navigate('/lobby');
      return;
    }

    const title = sessionStorage.getItem('current_meeting_title');
    const creatorId = sessionStorage.getItem('current_meeting_creator_id');
    if (title) setMeetingTitle(title);
    if (creatorId) setMeetingCreatorId(Number(creatorId));
  }, [meetingId, navigate]);

  const leaveMeeting = () => {
    if (confirm('Are you sure you want to leave this meeting?')) {
      navigate('/lobby');
    }
  };

  const panels = [
    {
      id: 'chat',
      icon: MessageSquare,
      label: 'Chat',
      color: 'from-blue-500 to-cyan-500',
    },
    {
      id: 'whiteboard',
      icon: Palette,
      label: 'Whiteboard',
      color: 'from-purple-500 to-pink-500',
    },
    {
      id: 'files',
      icon: FileText,
      label: 'Files',
      color: 'from-green-500 to-emerald-500',
    },
    {
      id: 'video',
      icon: VideoIcon,
      label: 'Video',
      color: 'from-red-500 to-orange-500',
    },
  ];

  return (
    <div className='h-screen bg-dark-950 flex flex-col overflow-hidden'>
      {/* Header */}
      <header className='bg-dark-900 border-b border-dark-700 px-6 py-4 flex-shrink-0'>
        <div className='flex justify-between items-center'>
          <div className='flex items-center gap-4'>
            <div className='w-10 h-10 bg-gradient-to-br from-primary-500 to-purple-600 rounded-xl flex items-center justify-center shadow-lg'>
              <VideoIcon className='w-6 h-6 text-white' />
            </div>
            <div>
              <h1 className='text-xl font-bold text-white'>{meetingTitle}</h1>
              <p className='text-xs text-gray-400'>Meeting ID: {meetingId}</p>
            </div>
          </div>

          <div className='flex items-center gap-3'>
            <button
              onClick={() => setIsSidebarOpen(!isSidebarOpen)}
              className='p-2 hover:bg-dark-800 rounded-lg transition-colors'
              title={isSidebarOpen ? 'Hide sidebar' : 'Show sidebar'}
            >
              <Maximize2 className='w-5 h-5 text-gray-400' />
            </button>
            <button
              onClick={leaveMeeting}
              className='btn-danger flex items-center gap-2'
            >
              <X className='w-4 h-4' />
              Leave
            </button>
          </div>
        </div>
      </header>

      {/* Main Content */}
      <div className='flex-1 flex overflow-hidden'>
        {/* Sidebar Navigation */}
        {isSidebarOpen && (
          <aside className='w-20 bg-dark-900 border-r border-dark-700 flex flex-col items-center py-6 gap-4'>
            {panels.map((panel) => {
              const Icon = panel.icon;
              const isActive = activePanel === panel.id;
              return (
                <button
                  key={panel.id}
                  onClick={() => setActivePanel(panel.id)}
                  className={`group relative w-14 h-14 rounded-xl flex items-center justify-center transition-all duration-300 ${
                    isActive
                      ? `bg-gradient-to-br ${panel.color} shadow-lg`
                      : 'bg-dark-800 hover:bg-dark-700'
                  }`}
                  title={panel.label}
                >
                  <Icon
                    className={`w-6 h-6 ${
                      isActive
                        ? 'text-white'
                        : 'text-gray-400 group-hover:text-white'
                    }`}
                  />

                  {/* Tooltip */}
                  <div className='absolute left-full ml-4 px-3 py-2 bg-dark-800 rounded-lg text-sm font-semibold text-white opacity-0 group-hover:opacity-100 transition-opacity pointer-events-none whitespace-nowrap border border-dark-700 shadow-xl'>
                    {panel.label}
                  </div>

                  {/* Active Indicator */}
                  {isActive && (
                    <div className='absolute -right-2 w-1 h-8 bg-white rounded-full'></div>
                  )}
                </button>
              );
            })}
          </aside>
        )}

        {/* Content Area */}
        <main className='flex-1 overflow-hidden'>
          {activePanel === 'chat' && <ChatPanel meetingId={meetingId} />}
          {activePanel === 'whiteboard' && (
            <WhiteboardPanel meetingId={meetingId} />
          )}
          {activePanel === 'files' && (
            <FilesPanel
              meetingId={meetingId}
              currentUserId={currentUserId}
              meetingCreatorId={meetingCreatorId}
            />
          )}
          {activePanel === 'video' && (
            <VideoPanel
              meetingId={meetingId}
              userId={currentUserId}
              username={getUsername()}
            />
          )}
        </main>
      </div>
    </div>
  );
}
