import { useState, useEffect, useRef } from 'react';
import { Pencil, Square, Circle, Eraser, Trash2, Palette } from 'lucide-react';
import api from '../utils/api';

export default function WhiteboardPanel({ meetingId }) {
  const canvasRef = useRef(null);
  const [currentTool, setCurrentTool] = useState('line');
  const [isDrawing, setIsDrawing] = useState(false);
  const [startPos, setStartPos] = useState({ x: 0, y: 0 });
  const [currentColor, setCurrentColor] = useState('#667eea');
  const [brushSize, setBrushSize] = useState(3);

  const colors = [
    '#667eea', '#ef4444', '#10b981', '#f59e0b', '#8b5cf6', 
    '#ec4899', '#14b8a6', '#f97316', '#ffffff', '#000000'
  ];

  useEffect(() => {
    loadWhiteboard();
    const interval = setInterval(loadWhiteboard, 5000);
    return () => clearInterval(interval);
  }, [meetingId]);

  const loadWhiteboard = async () => {
    try {
      const data = await api.getWhiteboardElements(meetingId);
      if (data.success) {
        const parsed = typeof data.elements === 'string'
          ? JSON.parse(data.elements)
          : data.elements;
        renderWhiteboard(parsed || []);
      }
    } catch (error) {
      console.error('Error loading whiteboard:', error);
    }
  };

  const renderWhiteboard = (elements) => {
    const canvas = canvasRef.current;
    if (!canvas) return;

    const ctx = canvas.getContext('2d');
    ctx.clearRect(0, 0, canvas.width, canvas.height);

    elements.forEach((elem) => {
      ctx.strokeStyle = currentColor;
      ctx.lineWidth = brushSize;
      ctx.lineCap = 'round';
      ctx.lineJoin = 'round';
      ctx.beginPath();

      if (elem.element_type === 0) {
        // Line
        ctx.moveTo(elem.x1, elem.y1);
        ctx.lineTo(elem.x2, elem.y2);
      } else if (elem.element_type === 1) {
        // Rectangle
        ctx.rect(elem.x1, elem.y1, elem.x2 - elem.x1, elem.y2 - elem.y1);
      } else if (elem.element_type === 2) {
        // Circle
        const radius = Math.sqrt((elem.x2 - elem.x1) ** 2 + (elem.y2 - elem.y1) ** 2);
        ctx.arc(elem.x1, elem.y1, radius, 0, 2 * Math.PI);
      }

      ctx.stroke();
    });
  };

  const getCanvasCoords = (e) => {
    const canvas = canvasRef.current;
    const rect = canvas.getBoundingClientRect();
    return {
      x: e.clientX - rect.left,
      y: e.clientY - rect.top
    };
  };

  const handleMouseDown = (e) => {
    setIsDrawing(true);
    const pos = getCanvasCoords(e);
    setStartPos(pos);
  };

  const handleMouseUp = async (e) => {
    if (!isDrawing) return;
    setIsDrawing(false);

    const endPos = getCanvasCoords(e);
    const elementType = currentTool === 'line' ? 0 : currentTool === 'rect' ? 1 : 2;

    try {
      await api.drawOnWhiteboard(meetingId, {
        element_type: elementType,
        x1: Math.floor(startPos.x),
        y1: Math.floor(startPos.y),
        x2: Math.floor(endPos.x),
        y2: Math.floor(endPos.y)
      });
      await loadWhiteboard();
    } catch (error) {
      console.error('Error drawing:', error);
    }
  };

  const clearWhiteboard = async () => {
    if (!confirm('Clear the entire whiteboard? This action cannot be undone.')) return;

    try {
      await api.clearWhiteboard(meetingId);
      const canvas = canvasRef.current;
      const ctx = canvas.getContext('2d');
      ctx.clearRect(0, 0, canvas.width, canvas.height);
    } catch (error) {
      console.error('Error clearing whiteboard:', error);
    }
  };

  const tools = [
    { id: 'line', icon: Pencil, label: 'Line' },
    { id: 'rect', icon: Square, label: 'Rectangle' },
    { id: 'circle', icon: Circle, label: 'Circle' }
  ];

  return (
    <div className="h-full flex flex-col bg-dark-900">
      {/* Header */}
      <div className="px-6 py-4 border-b border-dark-700 bg-dark-800/50 backdrop-blur-sm">
        <h2 className="text-xl font-bold text-white flex items-center gap-2">
          <Palette className="w-6 h-6" />
          Collaborative Whiteboard
        </h2>
        <p className="text-sm text-gray-400 mt-1">Draw together in real-time</p>
      </div>

      {/* Toolbar */}
      <div className="px-6 py-4 border-b border-dark-700 bg-dark-800/30">
        <div className="flex flex-wrap items-center gap-4">
          {/* Tools */}
          <div className="flex gap-2">
            {tools.map((tool) => {
              const Icon = tool.icon;
              return (
                <button
                  key={tool.id}
                  onClick={() => setCurrentTool(tool.id)}
                  className={`p-3 rounded-xl transition-all duration-300 ${
                    currentTool === tool.id
                      ? 'bg-gradient-to-br from-primary-500 to-purple-600 text-white shadow-lg'
                      : 'bg-dark-800 text-gray-400 hover:bg-dark-700 hover:text-white'
                  }`}
                  title={tool.label}
                >
                  <Icon className="w-5 h-5" />
                </button>
              );
            })}
          </div>

          {/* Divider */}
          <div className="h-8 w-px bg-dark-700"></div>

          {/* Colors */}
          <div className="flex gap-2 flex-wrap">
            {colors.map((color) => (
              <button
                key={color}
                onClick={() => setCurrentColor(color)}
                className={`w-8 h-8 rounded-lg transition-all duration-300 ${
                  currentColor === color
                    ? 'ring-2 ring-primary-500 ring-offset-2 ring-offset-dark-900 scale-110'
                    : 'hover:scale-110'
                }`}
                style={{ backgroundColor: color }}
                title={color}
              />
            ))}
          </div>

          {/* Divider */}
          <div className="h-8 w-px bg-dark-700"></div>

          {/* Brush Size */}
          <div className="flex items-center gap-3">
            <span className="text-sm text-gray-400">Size:</span>
            <input
              type="range"
              min="1"
              max="20"
              value={brushSize}
              onChange={(e) => setBrushSize(parseInt(e.target.value))}
              className="w-32"
            />
            <span className="text-sm font-semibold text-white w-8">{brushSize}px</span>
          </div>

          {/* Divider */}
          <div className="h-8 w-px bg-dark-700"></div>

          {/* Clear Button */}
          <button
            onClick={clearWhiteboard}
            className="flex items-center gap-2 px-4 py-2 bg-red-600 hover:bg-red-700 text-white rounded-xl transition-colors"
          >
            <Trash2 className="w-4 h-4" />
            Clear All
          </button>
        </div>
      </div>

      {/* Canvas Area */}
      <div className="flex-1 flex items-center justify-center p-6 overflow-auto">
        <div className="bg-white rounded-2xl shadow-2xl overflow-hidden">
          <canvas
            ref={canvasRef}
            width={1200}
            height={700}
            onMouseDown={handleMouseDown}
            onMouseUp={handleMouseUp}
            className="cursor-crosshair"
          />
        </div>
      </div>

      {/* Instructions */}
      <div className="px-6 py-3 border-t border-dark-700 bg-dark-800/30">
        <p className="text-xs text-gray-500 text-center">
          Click and drag to draw • Changes sync automatically • All participants can see your drawings
        </p>
      </div>
    </div>
  );
}