import { useState, useEffect } from 'react';
import { useNavigate } from 'react-router-dom';
import {
  Video,
  Plus,
  LogOut,
  Calendar,
  Users,
  Copy,
  Check,
  ArrowRight,
  Sparkles,
  TrendingUp,
} from 'lucide-react';
import api, { getToken, getUsername } from '../utils/api';

export default function Lobby() {
  const [meetings, setMeetings] = useState([]);
  const [meetingTitle, setMeetingTitle] = useState('');
  const [meetingCode, setMeetingCode] = useState('');
  const [copied, setCopied] = useState(null);
  const navigate = useNavigate();
  const username = getUsername();

  useEffect(() => {
    if (!getToken()) {
      navigate('/');
      return;
    }
    loadMeetings();
  }, [navigate]);

  const loadMeetings = async () => {
    try {
      const data = await api.getMyMeetings();
      if (data.success) {
        const parsed =
          typeof data.meetings === 'string'
            ? JSON.parse(data.meetings)
            : data.meetings;
        setMeetings(parsed || []);
      }
    } catch (error) {
      console.error('Error loading meetings:', error);
    }
  };

  const createMeeting = async () => {
    if (!meetingTitle.trim()) {
      alert('Please enter a meeting title');
      return;
    }

    try {
      const data = await api.createMeeting(meetingTitle);

      if (data.success) {
        // Parse meeting if it's a string (backend double-encodes it)
        const meeting =
          typeof data.meeting === 'string'
            ? JSON.parse(data.meeting)
            : data.meeting;

        alert(
          `Meeting created!\n\nCode: ${meeting.meeting_code}\n\nShare this code with others.`
        );
        setMeetingTitle('');
        loadMeetings();
      } else {
        alert('Error: ' + data.error);
      }
    } catch (error) {
      console.error('Create meeting error:', error);
      alert('Connection error');
    }
  };

  const joinMeeting = async () => {
    if (!meetingCode.trim()) {
      alert('Please enter a meeting code');
      return;
    }

    try {
      const data = await api.joinMeeting(meetingCode);
      if (data.success && data.meeting) {
        const meetingId = String(data.meeting.meeting_id);
        sessionStorage.setItem('current_meeting_id', meetingId);
        sessionStorage.setItem('current_meeting_title', data.meeting.title);
        sessionStorage.setItem('current_meeting_code', meetingCode);
        sessionStorage.setItem(
          'current_meeting_creator_id',
          String(data.meeting.creator_id)
        );
        navigate(`/meeting/${meetingId}`);
      } else {
        alert(data.error || 'Failed to join meeting');
      }
    } catch (error) {
      alert('Failed to join meeting');
    }
  };

  const joinMeetingById = (meetingId, title, code, creatorId) => {
    sessionStorage.setItem('current_meeting_id', String(meetingId));
    sessionStorage.setItem('current_meeting_title', title);
    sessionStorage.setItem('current_meeting_code', code);
    sessionStorage.setItem('current_meeting_creator_id', String(creatorId));
    navigate(`/meeting/${meetingId}`);
  };

  const copyCode = (code) => {
    navigator.clipboard.writeText(code);
    setCopied(code);
    setTimeout(() => setCopied(null), 2000);
  };

  const logout = async () => {
    await api.logout();
    sessionStorage.clear();
    navigate('/');
  };

  return (
    <div className='min-h-screen bg-gradient-to-br from-dark-950 via-dark-900 to-dark-950'>
      {/* Header */}
      <header className='border-b border-dark-700 bg-dark-900/80 backdrop-blur-xl sticky top-0 z-50'>
        <div className='max-w-7xl mx-auto px-6 py-4 flex justify-between items-center'>
          <div className='flex items-center gap-3'>
            <div className='w-10 h-10 bg-gradient-to-br from-primary-500 to-purple-600 rounded-xl flex items-center justify-center shadow-lg shadow-primary-500/50'>
              <Video className='w-6 h-6 text-white' />
            </div>
            <div>
              <h1 className='text-xl font-bold text-white'>Meeting System</h1>
              <p className='text-xs text-gray-400'>Powered by C++ Engine</p>
            </div>
          </div>

          <div className='flex items-center gap-4'>
            <div className='flex items-center gap-2 px-4 py-2 bg-dark-800 rounded-xl border border-dark-700'>
              <Users className='w-4 h-4 text-primary-400' />
              <span className='text-sm font-semibold text-white'>
                {username}
              </span>
            </div>
            <button
              onClick={logout}
              className='btn-danger flex items-center gap-2'
            >
              <LogOut className='w-4 h-4' />
              Logout
            </button>
          </div>
        </div>
      </header>

      <main className='max-w-7xl mx-auto px-6 py-8'>
        <div className='grid lg:grid-cols-2 gap-8'>
          {/* Create & Join Section */}
          <div className='space-y-6'>
            {/* Create Meeting */}
            <div className='card p-6 card-hover'>
              <div className='flex items-center gap-3 mb-6'>
                <div className='w-12 h-12 bg-gradient-to-br from-green-500 to-emerald-600 rounded-xl flex items-center justify-center'>
                  <Plus className='w-6 h-6 text-white' />
                </div>
                <div>
                  <h2 className='text-xl font-bold text-white'>
                    Create Meeting
                  </h2>
                  <p className='text-sm text-gray-400'>
                    Start a new collaborative session
                  </p>
                </div>
              </div>

              <input
                type='text'
                value={meetingTitle}
                onChange={(e) => setMeetingTitle(e.target.value)}
                className='input-field mb-4'
                placeholder='Enter meeting title (e.g., Team Standup)'
                onKeyPress={(e) => e.key === 'Enter' && createMeeting()}
              />

              <button
                onClick={createMeeting}
                className='btn-primary w-full flex items-center justify-center gap-2'
              >
                <Sparkles className='w-5 h-5' />
                Create Meeting
              </button>
            </div>

            {/* Join Meeting */}
            <div className='card p-6 card-hover'>
              <div className='flex items-center gap-3 mb-6'>
                <div className='w-12 h-12 bg-gradient-to-br from-blue-500 to-primary-600 rounded-xl flex items-center justify-center'>
                  <ArrowRight className='w-6 h-6 text-white' />
                </div>
                <div>
                  <h2 className='text-xl font-bold text-white'>Join Meeting</h2>
                  <p className='text-sm text-gray-400'>
                    Enter a meeting code to join
                  </p>
                </div>
              </div>

              <input
                type='text'
                value={meetingCode}
                onChange={(e) => setMeetingCode(e.target.value.toUpperCase())}
                className='input-field mb-4'
                placeholder='Enter code (e.g., ABC-DEF-123)'
                onKeyPress={(e) => e.key === 'Enter' && joinMeeting()}
              />

              <button
                onClick={joinMeeting}
                className='btn-primary w-full flex items-center justify-center gap-2'
              >
                <ArrowRight className='w-5 h-5' />
                Join Meeting
              </button>
            </div>
          </div>

          {/* My Meetings */}
          <div className='card p-6'>
            <div className='flex items-center gap-3 mb-6'>
              <div className='w-12 h-12 bg-gradient-to-br from-purple-500 to-pink-600 rounded-xl flex items-center justify-center'>
                <Calendar className='w-6 h-6 text-white' />
              </div>
              <div>
                <h2 className='text-xl font-bold text-white'>My Meetings</h2>
                <p className='text-sm text-gray-400'>
                  {meetings.length} active session(s)
                </p>
              </div>
            </div>

            <div className='space-y-3 max-h-[600px] overflow-y-auto pr-2'>
              {meetings.length === 0 ? (
                <div className='text-center py-12'>
                  <TrendingUp className='w-16 h-16 text-gray-600 mx-auto mb-4' />
                  <p className='text-gray-500'>No meetings yet</p>
                  <p className='text-sm text-gray-600 mt-2'>
                    Create your first meeting to get started!
                  </p>
                </div>
              ) : (
                meetings.map((meeting) => (
                  <div
                    key={meeting.meeting_id}
                    className='bg-dark-800 p-4 rounded-xl border border-dark-700 hover:border-primary-500/50 transition-all duration-300 group'
                  >
                    <div className='flex justify-between items-start mb-3'>
                      <div className='flex-1'>
                        <h3 className='text-lg font-semibold text-white mb-1 group-hover:text-primary-400 transition-colors'>
                          {meeting.title}
                        </h3>
                        <div className='flex items-center gap-2 mb-2'>
                          <span className='text-xs text-gray-500'>
                            {new Date(
                              meeting.created_at * 1000
                            ).toLocaleDateString()}
                          </span>
                        </div>
                        <div className='flex items-center gap-2'>
                          <code className='text-sm font-mono text-primary-400 bg-dark-900 px-2 py-1 rounded'>
                            {meeting.meeting_code}
                          </code>
                          <button
                            onClick={() => copyCode(meeting.meeting_code)}
                            className='p-1 hover:bg-dark-700 rounded transition-colors'
                            title='Copy code'
                          >
                            {copied === meeting.meeting_code ? (
                              <Check className='w-4 h-4 text-green-400' />
                            ) : (
                              <Copy className='w-4 h-4 text-gray-400' />
                            )}
                          </button>
                        </div>
                      </div>

                      <button
                        onClick={() =>
                          joinMeetingById(
                            meeting.meeting_id,
                            meeting.title,
                            meeting.meeting_code,
                            meeting.creator_id
                          )
                        }
                        className='px-4 py-2 bg-gradient-to-r from-primary-500 to-purple-600 rounded-lg text-white font-semibold hover:shadow-lg hover:shadow-primary-500/50 transition-all duration-300 flex items-center gap-2'
                      >
                        Join
                        <ArrowRight className='w-4 h-4' />
                      </button>
                    </div>
                  </div>
                ))
              )}
            </div>
          </div>
        </div>
      </main>
    </div>
  );
}
