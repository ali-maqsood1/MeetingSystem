import { useState, useEffect } from 'react';
import {
  Upload,
  Download,
  File,
  Trash2,
  FileText,
  Image,
  Video,
  Music,
  AlertCircle,
  Loader,
  Lock,
} from 'lucide-react';
import api, { getUsername } from '../utils/api';

export default function FilesPanel({
  meetingId,
  currentUserId,
  meetingCreatorId,
}) {
  const [files, setFiles] = useState([]);
  const [selectedFile, setSelectedFile] = useState(null);
  const [showUploadModal, setShowUploadModal] = useState(false);
  const [uploading, setUploading] = useState(false);
  const [uploadProgress, setUploadProgress] = useState(0);
  const [uploadStatus, setUploadStatus] = useState('');
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState(null);
  const username = getUsername();
  const userId = Number(currentUserId);

  const canDeleteFile = (file) => {
    if (!userId || !file) return false;
    return (
      file.uploader_id === userId ||
      (meetingCreatorId && userId === Number(meetingCreatorId))
    );
  };

  useEffect(() => {
    loadFiles();
    // Auto-refresh files every 5 seconds
    const interval = setInterval(loadFiles, 5000);
    return () => clearInterval(interval);
  }, [meetingId]);

  const loadFiles = async () => {
    try {
      const data = await api.getFiles(meetingId);
      if (data.success) {
        const filesArray =
          typeof data.files === 'string'
            ? JSON.parse(data.files)
            : data.files || [];
        setFiles(filesArray);
      }
      setLoading(false);
    } catch (err) {
      console.error('Error loading files:', err);
      setError('Failed to load files');
      setLoading(false);
    }
  };

  const handleFileSelect = (e) => {
    const file = e.target.files[0];
    if (!file) return;

    // Check size (10MB limit)
    if (file.size > 10 * 1024 * 1024) {
      alert('File too large! Maximum size is 10MB');
      e.target.value = '';
      return;
    }

    setSelectedFile(file);
  };

  const handleFileUpload = async () => {
    if (!selectedFile) return;

    setUploading(true);
    setError(null);
    setUploadProgress(0);
    setUploadStatus('Preparing file...');

    try {
      // Read file as base64
      const reader = new FileReader();

      // Show encoding progress
      reader.onprogress = (e) => {
        if (e.lengthComputable) {
          const progress = Math.round((e.loaded / e.total) * 30); // 0-30%
          setUploadProgress(progress);
        }
      };

      reader.onload = async (e) => {
        try {
          setUploadProgress(35);
          setUploadStatus('Encoding file...');

          const base64Data = e.target.result.split(',')[1];

          setUploadProgress(50);
          setUploadStatus('Uploading to server...');

          const data = await api.uploadFile(
            meetingId,
            selectedFile.name,
            base64Data
          );

          setUploadProgress(90);
          setUploadStatus('Processing...');

          if (data.success) {
            setUploadProgress(100);
            setUploadStatus('Upload complete!');

            // Brief success message
            await new Promise((resolve) => setTimeout(resolve, 500));

            // Reset file input BEFORE closing modal
            const fileInput = document.getElementById('file-upload');
            if (fileInput) fileInput.value = '';

            setSelectedFile(null);
            setShowUploadModal(false);
            setUploadProgress(0);
            setUploadStatus('');
            await loadFiles();
          } else {
            setError(data.error || 'Upload failed');
          }
        } catch (err) {
          console.error('Upload error:', err);
          setError('Upload failed: ' + err.message);
        } finally {
          setUploading(false);
        }
      };

      reader.onerror = () => {
        setError('Failed to read file');
        setUploading(false);
      };

      reader.readAsDataURL(selectedFile);
    } catch (err) {
      console.error('File read error:', err);
      setError('Failed to process file');
      setUploading(false);
    }
  };

  const downloadFile = async (file) => {
    try {
      const data = await api.downloadFile(meetingId, file.file_id);

      if (data.success) {
        // Convert base64 back to blob
        const byteCharacters = atob(data.data);
        const byteNumbers = new Array(byteCharacters.length);
        for (let i = 0; i < byteCharacters.length; i++) {
          byteNumbers[i] = byteCharacters.charCodeAt(i);
        }
        const byteArray = new Uint8Array(byteNumbers);
        const blob = new Blob([byteArray]);

        // Download
        const url = window.URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.href = url;
        a.download = file.filename;
        document.body.appendChild(a);
        a.click();
        window.URL.revokeObjectURL(url);
        document.body.removeChild(a);
      } else {
        alert('Download failed: ' + (data.error || 'Unknown error'));
      }
    } catch (err) {
      console.error('Download error:', err);
      alert('Download failed');
    }
  };

  const deleteFile = async (fileId) => {
    if (!confirm('Delete this file? This action cannot be undone.')) return;

    try {
      const data = await api.deleteFile(meetingId, fileId);

      if (data.success) {
        await loadFiles();
      } else {
        alert('Delete failed: ' + (data.error || 'Unknown error'));
      }
    } catch (err) {
      console.error('Delete error:', err);
      alert('Delete failed');
    }
  };

  const getFileIcon = (filename) => {
    const ext = filename.split('.').pop().toLowerCase();

    if (['jpg', 'jpeg', 'png', 'gif', 'bmp', 'svg'].includes(ext)) {
      return <Image className='w-8 h-8 text-blue-400' />;
    } else if (['mp4', 'avi', 'mov', 'mkv', 'webm'].includes(ext)) {
      return <Video className='w-8 h-8 text-purple-400' />;
    } else if (['mp3', 'wav', 'flac', 'ogg', 'm4a'].includes(ext)) {
      return <Music className='w-8 h-8 text-pink-400' />;
    }
    return <FileText className='w-8 h-8 text-green-400' />;
  };

  const formatFileSize = (bytes) => {
    if (bytes < 1024) return bytes + ' B';
    if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(2) + ' KB';
    return (bytes / (1024 * 1024)).toFixed(2) + ' MB';
  };

  if (loading) {
    return (
      <div className='h-full flex items-center justify-center bg-dark-900'>
        <div className='text-center'>
          <Loader className='w-12 h-12 text-primary-500 animate-spin mx-auto mb-4' />
          <p className='text-gray-400'>Loading files...</p>
        </div>
      </div>
    );
  }

  return (
    <div className='h-full flex flex-col bg-dark-900'>
      {/* Header */}
      <div className='px-6 py-4 border-b border-dark-700 bg-dark-800/50 backdrop-blur-sm'>
        <div className='flex justify-between items-center'>
          <div>
            <h2 className='text-xl font-bold text-white flex items-center gap-2'>
              <File className='w-6 h-6' />
              Shared Files
            </h2>
            <p className='text-sm text-gray-400 mt-1'>
              {files.length} file(s) uploaded • Max 10MB per file
            </p>
          </div>
          <button
            onClick={() => setShowUploadModal(true)}
            className='btn-primary flex items-center gap-2'
          >
            <Upload className='w-5 h-5' />
            Upload File
          </button>
        </div>
      </div>

      {/* Error Message */}
      {error && (
        <div className='mx-6 mt-4 p-4 bg-red-500/10 border border-red-500/50 rounded-lg flex items-start gap-3'>
          <AlertCircle className='w-5 h-5 text-red-400 flex-shrink-0 mt-0.5' />
          <div className='flex-1'>
            <p className='text-red-400 font-semibold'>Error</p>
            <p className='text-red-300 text-sm mt-1'>{error}</p>
          </div>
          <button
            onClick={() => setError(null)}
            className='text-red-400 hover:text-red-300'
          >
            ×
          </button>
        </div>
      )}

      {/* Files List */}
      <div className='flex-1 overflow-y-auto px-6 py-4'>
        {files.length === 0 ? (
          <div className='h-full flex items-center justify-center'>
            <div className='text-center'>
              <File className='w-16 h-16 text-gray-600 mx-auto mb-4' />
              <p className='text-gray-500 text-lg'>No files shared yet</p>
              <p className='text-gray-600 text-sm mt-2'>
                Upload documents, images, or any file up to 10MB
              </p>
            </div>
          </div>
        ) : (
          <div className='grid gap-4'>
            {files.map((file) => (
              <div
                key={file.file_id}
                className='card p-4 hover:border-primary-500/50 transition-all duration-300'
              >
                <div className='flex items-start gap-4'>
                  {/* File Icon */}
                  <div className='flex-shrink-0 w-16 h-16 bg-dark-800 rounded-xl flex items-center justify-center'>
                    {getFileIcon(file.filename)}
                  </div>

                  {/* File Info */}
                  <div className='flex-1 min-w-0'>
                    <h3 className='text-white font-semibold truncate'>
                      {file.filename}
                    </h3>
                    <div className='flex items-center gap-4 mt-2 text-sm text-gray-400'>
                      <span>{formatFileSize(file.file_size)}</span>
                      <span>•</span>
                      <span>
                        {new Date(file.uploaded_at * 1000).toLocaleString()}
                      </span>
                    </div>
                  </div>

                  {/* Actions */}
                  <div className='flex gap-2'>
                    <button
                      onClick={() => downloadFile(file)}
                      className='p-2 bg-dark-800 hover:bg-primary-600 text-gray-400 hover:text-white rounded-lg transition-colors'
                      title='Download'
                    >
                      <Download className='w-5 h-5' />
                    </button>
                    {canDeleteFile(file) ? (
                      <button
                        onClick={() => deleteFile(file.file_id)}
                        className='p-2 bg-dark-800 hover:bg-red-600 text-gray-400 hover:text-white rounded-lg transition-colors'
                        title='Delete'
                      >
                        <Trash2 className='w-5 h-5' />
                      </button>
                    ) : (
                      <button
                        disabled
                        className='p-2 bg-dark-800/50 text-gray-600 rounded-lg cursor-not-allowed'
                        title='Only the uploader or meeting creator can delete this file'
                      >
                        <Lock className='w-5 h-5' />
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
        <div className='fixed inset-0 bg-black/80 backdrop-blur-sm flex items-center justify-center z-50 p-4'>
          <div className='card p-6 max-w-md w-full animate-slide-up'>
            <h3 className='text-xl font-bold text-white mb-4'>Upload File</h3>

            {/* File Input */}
            <div className='mb-4'>
              <input
                type='file'
                onChange={handleFileSelect}
                className='hidden'
                id='file-upload'
                disabled={uploading}
              />
              <label
                htmlFor='file-upload'
                className={`flex items-center justify-center gap-3 p-4 border-2 border-dashed rounded-xl transition-colors ${
                  uploading
                    ? 'border-dark-600 cursor-not-allowed opacity-50'
                    : 'border-dark-600 hover:border-primary-500 cursor-pointer'
                }`}
              >
                <Upload className='w-6 h-6 text-gray-400' />
                <span className='text-gray-400'>
                  {selectedFile
                    ? selectedFile.name
                    : 'Choose a file (max 10MB)'}
                </span>
              </label>
              {selectedFile && (
                <p className='text-xs text-gray-500 mt-2'>
                  Size: {formatFileSize(selectedFile.size)}
                </p>
              )}
            </div>

            {/* Upload Progress Bar */}
            {uploading && (
              <div className='mb-4'>
                <div className='flex justify-between text-sm mb-2'>
                  <span className='text-gray-400'>{uploadStatus}</span>
                  <span className='text-primary-400'>{uploadProgress}%</span>
                </div>
                <div className='h-2 bg-dark-700 rounded-full overflow-hidden'>
                  <div
                    className='h-full bg-gradient-to-r from-primary-500 to-primary-400 transition-all duration-300 ease-out'
                    style={{ width: `${uploadProgress}%` }}
                  />
                </div>
              </div>
            )}

            {/* Actions */}
            <div className='flex gap-3'>
              <button
                onClick={() => {
                  setShowUploadModal(false);
                  setSelectedFile(null);
                  setError(null);
                  document.getElementById('file-upload').value = '';
                }}
                className='btn-secondary flex-1'
                disabled={uploading}
              >
                Cancel
              </button>
              <button
                onClick={handleFileUpload}
                className='btn-primary flex-1 flex items-center justify-center gap-2'
                disabled={!selectedFile || uploading}
              >
                {uploading ? (
                  <>
                    <Loader className='w-4 h-4 animate-spin' />
                    Uploading...
                  </>
                ) : (
                  <>
                    <Upload className='w-4 h-4' />
                    Upload
                  </>
                )}
              </button>
            </div>
          </div>
        </div>
      )}
    </div>
  );
}
