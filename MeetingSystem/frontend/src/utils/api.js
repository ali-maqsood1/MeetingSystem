const API_URL = 'http://localhost:8080/api/v1';

export const getToken = () => sessionStorage.getItem('token');
export const getUsername = () => sessionStorage.getItem('username');
export const getUserId = () => sessionStorage.getItem('user_id');

export const api = {
  // Auth endpoints
  login: async (email, password) => {
    const response = await fetch(`${API_URL}/auth/login`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ email, password })
    });
    return response.json();
  },

  register: async (username, email, password) => {
    const response = await fetch(`${API_URL}/auth/register`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ username, email, password })
    });
    return response.json();
  },

  logout: async () => {
    const token = getToken();
    const response = await fetch(`${API_URL}/auth/logout`, {
      method: 'POST',
      headers: { 'Authorization': `Bearer ${token}` }
    });
    return response.json();
  },

  // Meeting endpoints
  getMyMeetings: async () => {
    const token = getToken();
    const response = await fetch(`${API_URL}/meetings/my-meetings`, {
      headers: { 'Authorization': `Bearer ${token}` }
    });
    return response.json();
  },

  createMeeting: async (title) => {
    const token = getToken();
    const response = await fetch(`${API_URL}/meetings/create`, {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
        'Authorization': `Bearer ${token}`
      },
      body: JSON.stringify({ title })
    });
    return response.json();
  },

  joinMeeting: async (meetingCode) => {
    const token = getToken();
    const response = await fetch(`${API_URL}/meetings/join`, {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
        'Authorization': `Bearer ${token}`
      },
      body: JSON.stringify({ meeting_code: meetingCode })
    });
    return response.json();
  },

  // Chat endpoints
  getMessages: async (meetingId) => {
    const token = getToken();
    const response = await fetch(`${API_URL}/meetings/${meetingId}/messages`, {
      headers: { 'Authorization': `Bearer ${token}` }
    });
    return response.json();
  },

  sendMessage: async (meetingId, content) => {
    const token = getToken();
    const response = await fetch(`${API_URL}/meetings/${meetingId}/messages`, {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
        'Authorization': `Bearer ${token}`
      },
      body: JSON.stringify({ content })
    });
    return response.json();
  },

  // Whiteboard endpoints
  getWhiteboardElements: async (meetingId) => {
    const token = getToken();
    const response = await fetch(`${API_URL}/meetings/${meetingId}/whiteboard/elements`, {
      headers: { 'Authorization': `Bearer ${token}` }
    });
    return response.json();
  },

  drawOnWhiteboard: async (meetingId, element) => {
    const token = getToken();
    const response = await fetch(`${API_URL}/meetings/${meetingId}/whiteboard/draw`, {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
        'Authorization': `Bearer ${token}`
      },
      body: JSON.stringify(element)
    });
    return response.json();
  },

  clearWhiteboard: async (meetingId) => {
    const token = getToken();
    const response = await fetch(`${API_URL}/meetings/${meetingId}/whiteboard/clear`, {
      method: 'POST',
      headers: { 'Authorization': `Bearer ${token}` }
    });
    return response.json();
  }
};

export default api;