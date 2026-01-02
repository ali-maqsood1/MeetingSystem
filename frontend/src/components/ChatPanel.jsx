import { useState, useEffect, useRef } from 'react';
import { Send, Smile } from 'lucide-react';
import api, { getUsername } from '../utils/api';

export default function ChatPanel({ meetingId }) {
  const [messages, setMessages] = useState([]);
  const [inputMessage, setInputMessage] = useState('');
  const messagesEndRef = useRef(null);
  const lastMessageIdRef = useRef(0);
  const username = getUsername();

  useEffect(() => {
    loadMessages();
    // Simple polling every 500ms
    const interval = setInterval(loadMessages, 500);
    return () => clearInterval(interval);
  }, [meetingId]);

  useEffect(() => {
    scrollToBottom();
  }, [messages]);

  const scrollToBottom = () => {
    messagesEndRef.current?.scrollIntoView({ behavior: 'smooth' });
  };

  const loadMessages = async () => {
    try {
      const data = await api.getMessages(meetingId);
      if (data.success) {
        const parsed =
          typeof data.messages === 'string'
            ? JSON.parse(data.messages)
            : data.messages;

        const newMessages = parsed || [];

        // Only update if messages changed
        if (newMessages.length > 0) {
          const latestId = newMessages[newMessages.length - 1]?.message_id;
          if (latestId !== lastMessageIdRef.current) {
            lastMessageIdRef.current = latestId;
            setMessages(newMessages);
          }
        } else if (messages.length > 0) {
          setMessages([]);
          lastMessageIdRef.current = 0;
        }
      }
    } catch (error) {
      console.error('Error loading messages:', error);
    }
  };

  const sendMessage = async () => {
    if (!inputMessage.trim()) return;

    const messageContent = inputMessage.trim();
    setInputMessage('');

    try {
      await api.sendMessage(meetingId, messageContent);
      
      await loadMessages();
    } catch (error) {
      console.error('Error sending message:', error);
      setInputMessage(messageContent); 
    }
  };

  const handleKeyPress = (e) => {
    if (e.key === 'Enter' && !e.shiftKey) {
      e.preventDefault();
      sendMessage();
    }
  };

  return (
    <div className='h-full flex flex-col bg-dark-900'>
      {/* Chat Header */}
      <div className='px-6 py-4 border-b border-dark-700 bg-dark-800/50 backdrop-blur-sm'>
        <h2 className='text-xl font-bold text-white flex items-center gap-2'>
          <div className='w-3 h-3 bg-green-500 rounded-full animate-pulse'></div>
          Live Chat
        </h2>
        <p className='text-sm text-gray-400 mt-1'>
          {messages.length} message(s)
        </p>
      </div>

      {/* Messages Area */}
      <div className='flex-1 overflow-y-auto px-6 py-4 space-y-4'>
        {messages.length === 0 ? (
          <div className='h-full flex items-center justify-center'>
            <div className='text-center'>
              <Smile className='w-16 h-16 text-gray-600 mx-auto mb-4' />
              <p className='text-gray-500 text-lg'>No messages yet</p>
              <p className='text-gray-600 text-sm mt-2'>
                Start the conversation!
              </p>
            </div>
          </div>
        ) : (
          messages.map((msg, idx) => {
            const isMe = msg.username === username;
            return (
              <div
                key={idx}
                className={`flex ${
                  isMe ? 'justify-end' : 'justify-start'
                } animate-fade-in`}
              >
                <div className={`max-w-md ${isMe ? 'order-2' : 'order-1'}`}>
                  <div className='flex items-center gap-2 mb-1'>
                    <span
                      className={`text-xs font-semibold ${
                        isMe ? 'text-green-400' : 'text-primary-400'
                      }`}
                    >
                      {isMe ? 'You' : msg.username}
                    </span>
                    <span className='text-xs text-gray-500'>
                      {new Date(msg.timestamp * 1000).toLocaleTimeString()}
                    </span>
                  </div>
                  <div
                    className={`px-4 py-3 rounded-2xl ${
                      isMe
                        ? 'bg-gradient-to-br from-primary-500 to-purple-600 text-white rounded-br-sm'
                        : 'bg-dark-800 text-gray-100 rounded-bl-sm border border-dark-700'
                    }`}
                  >
                    <p className='text-sm leading-relaxed break-words'>
                      {msg.content}
                    </p>
                  </div>
                </div>
              </div>
            );
          })
        )}
        <div ref={messagesEndRef} />
      </div>

      {/* Input Area */}
      <div className='px-6 py-4 border-t border-dark-700 bg-dark-800/50 backdrop-blur-sm'>
        <div className='flex gap-3'>
          <textarea
            value={inputMessage}
            onChange={(e) => setInputMessage(e.target.value)}
            onKeyPress={handleKeyPress}
            placeholder='Type your message...'
            rows='1'
            className='flex-1 bg-dark-900 border-2 border-dark-700 rounded-xl px-4 py-3 text-white placeholder-gray-500 focus:border-primary-500 focus:outline-none transition-colors resize-none'
            style={{ maxHeight: '120px' }}
          />
          <button
            onClick={sendMessage}
            disabled={!inputMessage.trim()}
            className='btn-primary px-6 flex items-center gap-2 disabled:opacity-50 disabled:cursor-not-allowed'
          >
            <Send className='w-5 h-5' />
          </button>
        </div>
        <p className='text-xs text-gray-500 mt-2'>
          Press Enter to send, Shift+Enter for new line
        </p>
      </div>
    </div>
  );
}
