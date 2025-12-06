import { useState } from 'react';
import { Upload, Download, File, Users, User, Trash2, Share2, FileText, Image, Video, Music } from 'lucide-react';
import { getUsername } from '../utils/api';

export default function FilesPanel({ meetingId }) {
  const [files, setFiles] = useState([
    // Mock data - replace with actual API calls
    {
      id: 1,
      name: 'Project_Presentation.pdf',
      size: '2.4 MB',
      type: 'pdf',
      uploadedBy: 'Alice',
      sharedWith: 'all',
      timestamp: Date.now() - 3600000
    },
    {
      id: 2,
      name: 'Design_Mockups.png',
      size: '1.8 MB',
      type: 'image',
      uploadedBy: 'Bob',
      sharedWith: 'specific',
      timestamp: Date.now() - 7200000
    }
  ]);
  
  const [selectedFile, setSelectedFile] = useState(null);
  const [shareTarget, setShareTarget] = useState('all');
  const [showUploadModal, setShowUploadModal] = useState(false);
  const username = getUsername();

  const participants = ['Alice', 'Bob', 'Charlie', 'Diana']; // Mock - replace with actual participants

  const handleFileUpload = (e) => {
    const uploadedFile = e.target.files[0];
    if (!uploadedFile) return;

    // Mock upload - replace with actual API call
    const newFile = {
      id: Date.now(),
      name: uploadedFile.name,
      size: (uploadedFile.size / (1024 * 1024)).toFixed(2) + ' MB',
      type: uploadedFile.type.split('/')[0],
      uploadedBy: username,
      sharedWith: shareTarget,
      timestamp: Date.now()
    };

    setFiles([newFile, ...files]);
    setShowUploadModal(false);
    setSelectedFile(null);
    setShareTarget('all');
  };

  const deleteFile = (fileId) => {
    if (confirm('Delete this file? This action cannot be undone.')) {
      setFiles(files.filter(f => f.id !== fileId));
    }
  };

  const downloadFile = (file) => {
    // Mock download - replace with actual API call
    alert(`Downloading ${file.name}...`);
  };

  const getFileIcon = (type) => {
    switch (type) {
      case 'image':
        return <Image className="w-8 h-8 text-blue-400" />;
      case 'video':
        return <Video className="w-8 h-8 text-purple-400" />;
      case 'audio':
        return <Music className="w-8 h-8 text-pink-400" />;
      default:
        return <FileText className="w-8 h-8 text-green-400" />;
    }
  };

  return (
    <div className="h-full flex flex-col bg-dark-900">
      {/* Header */}
      <div className="px-6 py-4 border-b border-dark-700 bg-dark-800/50 backdrop-blur-sm">
        <div className="flex justify-between items-center">
          <div>
            <h2 className="text-xl font-bold text-white flex items-center gap-2">
              <File className="w-6 h-6" />
              Shared Files
            </h2>
            <p className="text-sm text-gray-400 mt-1">{files.length} file(s) uploaded</p>
          </div>
          <button
            onClick={() => setShowUploadModal(true)}
            className="btn-primary flex items-center gap-2"
          >
            <Upload className="w-5 h-5" />
            Upload File
          </button>
        </div>
      </div>

      {/* Files List */}
      <div className="flex-1 overflow-y-auto px-6 py-4">
        {files.length === 0 ? (
          <div className="h-full flex items-center justify-center">
            <div className="text-center">
              <File className="w-16 h-16 text-gray-600 mx-auto mb-4" />
              <p className="text-gray-500 text-lg">No files shared yet</p>
              <p className="text-gray-600 text-sm mt-2">Upload a file to get started</p>
            </div>
          </div>
        ) : (
          <div className="grid gap-4">
            {files.map((file) => (
              <div
                key={file.id}
                className="card p-4 hover:border-primary-500/50 transition-all duration-300"
              >
                <div className="flex items-start gap-4">
                  {/* File Icon */}
                  <div className="flex-shrink-0 w-16 h-16 bg-dark-800 rounded-xl flex items-center justify-center">
                    {getFileIcon(file.type)}
                  </div>

                  {/* File Info */}
                  <div className="flex-1 min-w-0">
                    <h3 className="text-white font-semibold truncate">{file.name}</h3>
                    <div className="flex items-center gap-4 mt-2 text-sm text-gray-400">
                      <span>{file.size}</span>
                      <span>•</span>
                      <span>Uploaded by {file.uploadedBy}</span>
                      <span>•</span>
                      <span>{new Date(file.timestamp).toLocaleString()}</span>
                    </div>
                    <div className="flex items-center gap-2 mt-2">
                      {file.sharedWith === 'all' ? (
                        <span className="inline-flex items-center gap-1 px-2 py-1 bg-green-500/10 text-green-400 rounded-lg text-xs">
                          <Users className="w-3 h-3" />
                          Shared with everyone
                        </span>
                      ) : (
                        <span className="inline-flex items-center gap-1 px-2 py-1 bg-blue-500/10 text-blue-400 rounded-lg text-xs">
                          <User className="w-3 h-3" />
                          Shared with specific users
                        </span>
                      )}
                    </div>
                  </div>

                  {/* Actions */}
                  <div className="flex gap-2">
                    <button
                      onClick={() => downloadFile(file)}
                      className="p-2 bg-dark-800 hover:bg-primary-600 text-gray-400 hover:text-white rounded-lg transition-colors"
                      title="Download"
                    >
                      <Download className="w-5 h-5" />
                    </button>
                    {file.uploadedBy === username && (
                      <button
                        onClick={() => deleteFile(file.id)}
                        className="p-2 bg-dark-800 hover:bg-red-600 text-gray-400 hover:text-white rounded-lg transition-colors"
                        title="Delete"
                      >
                        <Trash2 className="w-5 h-5" />
                      </button>
                    )}
                  </div>
                </div>
              </div>
            ))}
          </div>
        )}
      </div>

      {/* Upload Modal */}
      {showUploadModal && (
        <div className="fixed inset-0 bg-black/80 backdrop-blur-sm flex items-center justify-center z-50 p-4">
          <div className="card p-6 max-w-md w-full animate-slide-up">
            <h3 className="text-xl font-bold text-white mb-4">Upload File</h3>

            {/* Share Options */}
            <div className="mb-4">
              <label className="block text-sm font-semibold text-gray-300 mb-2">
                Share with
              </label>
              <div className="space-y-2">
                <label className="flex items-center gap-3 p-3 bg-dark-800 rounded-xl cursor-pointer hover:bg-dark-700 transition-colors">
                  <input
                    type="radio"
                    name="shareTarget"
                    value="all"
                    checked={shareTarget === 'all'}
                    onChange={(e) => setShareTarget(e.target.value)}
                    className="w-4 h-4 text-primary-500"
                  />
                  <Users className="w-5 h-5 text-green-400" />
                  <span className="text-white">Everyone in meeting</span>
                </label>

                <label className="flex items-center gap-3 p-3 bg-dark-800 rounded-xl cursor-pointer hover:bg-dark-700 transition-colors">
                  <input
                    type="radio"
                    name="shareTarget"
                    value="specific"
                    checked={shareTarget === 'specific'}
                    onChange={(e) => setShareTarget(e.target.value)}
                    className="w-4 h-4 text-primary-500"
                  />
                  <User className="w-5 h-5 text-blue-400" />
                  <span className="text-white">Specific participants</span>
                </label>
              </div>
            </div>

            {/* Participant Selection (if specific) */}
            {shareTarget === 'specific' && (
              <div className="mb-4">
                <label className="block text-sm font-semibold text-gray-300 mb-2">
                  Select participants
                </label>
                <div className="space-y-2 max-h-40 overflow-y-auto">
                  {participants.map((participant) => (
                    <label
                      key={participant}
                      className="flex items-center gap-3 p-2 bg-dark-800 rounded-lg cursor-pointer hover:bg-dark-700 transition-colors"
                    >
                      <input type="checkbox" className="w-4 h-4 text-primary-500" />
                      <span className="text-white text-sm">{participant}</span>
                    </label>
                  ))}
                </div>
              </div>
            )}

            {/* File Input */}
            <div className="mb-4">
              <input
                type="file"
                onChange={handleFileUpload}
                className="hidden"
                id="file-upload"
              />
              <label
                htmlFor="file-upload"
                className="flex items-center justify-center gap-3 p-4 border-2 border-dashed border-dark-600 rounded-xl cursor-pointer hover:border-primary-500 transition-colors"
              >
                <Upload className="w-6 h-6 text-gray-400" />
                <span className="text-gray-400">
                  {selectedFile ? selectedFile.name : 'Choose a file'}
                </span>
              </label>
            </div>

            {/* Actions */}
            <div className="flex gap-3">
              <button
                onClick={() => {
                  setShowUploadModal(false);
                  setSelectedFile(null);
                  setShareTarget('all');
                }}
                className="btn-secondary flex-1"
              >
                Cancel
              </button>
            </div>
          </div>
        </div>
      )}
    </div>
  );
}