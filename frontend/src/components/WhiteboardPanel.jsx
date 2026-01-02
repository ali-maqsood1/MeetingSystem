import { useState, useEffect, useRef } from 'react';
import {
  Pencil,
  Square,
  Circle,
  Trash2,
  Palette,
  Type,
  Triangle,
  ArrowRight,
  Star,
  ZoomIn,
  ZoomOut,
  Move,
  Eraser,
} from 'lucide-react';
import api from '../utils/api';

export default function WhiteboardPanel({ meetingId }) {
  const canvasRef = useRef(null);
  const textInputRef = useRef(null);
  const [currentTool, setCurrentTool] = useState('line');
  const [isDrawing, setIsDrawing] = useState(false);
  const [startPos, setStartPos] = useState({ x: 0, y: 0 });
  const [currentColor, setCurrentColor] = useState('#667eea');
  const [brushSize, setBrushSize] = useState(3);
  const [fillEnabled, setFillEnabled] = useState(false);
  const [zoom, setZoom] = useState(1);
  const [pan, setPan] = useState({ x: 0, y: 0 });
  const [isPanning, setIsPanning] = useState(false);
  const [panStart, setPanStart] = useState({ x: 0, y: 0 });
  const [fontSize, setFontSize] = useState(24);
  const [currentPos, setCurrentPos] = useState({ x: 0, y: 0 });
  const [elements, setElements] = useState([]);
  const [isAddingText, setIsAddingText] = useState(false);
  const [textInput, setTextInput] = useState('');
  const [textPos, setTextPos] = useState({ x: 0, y: 0 });

  const colors = [
    '#667eea',
    '#ef4444',
    '#10b981',
    '#f59e0b',
    '#8b5cf6',
    '#ec4899',
    '#14b8a6',
    '#f97316',
    '#ffffff',
    '#000000',
  ];

  useEffect(() => {
    loadWhiteboard();
    const interval = setInterval(loadWhiteboard, 500); 
    return () => clearInterval(interval);
  }, [meetingId]);

  useEffect(() => {
    renderWhiteboard();
  }, [
    elements,
    zoom,
    pan,
  ]);

  const loadWhiteboard = async () => {
    try {
      const data = await api.getWhiteboardElements(meetingId);
      if (data.success) {
        setElements(data.elements || []);
      }
    } catch (error) {
      console.error('Error loading whiteboard:', error);
    }
  };

  const renderWhiteboard = () => {
    const canvas = canvasRef.current;
    if (!canvas) return;

    const ctx = canvas.getContext('2d');
    ctx.save();
    ctx.fillStyle = '#ffffff';
    ctx.fillRect(0, 0, canvas.width, canvas.height);
    ctx.translate(pan.x, pan.y);
    ctx.scale(zoom, zoom);

    elements.forEach((elem) => {
      // Use ?? instead of || to allow 0 values (for black color: rgb(0,0,0))
      const r = elem.color_r !== undefined ? elem.color_r : 102;
      const g = elem.color_g !== undefined ? elem.color_g : 126;
      const b = elem.color_b !== undefined ? elem.color_b : 234;
      const color = `rgb(${r}, ${g}, ${b})`;
      ctx.strokeStyle = color;
      ctx.fillStyle = color;
      ctx.lineWidth = elem.stroke_width % 100 || 3;
      ctx.lineCap = 'round';
      ctx.lineJoin = 'round';

      const isFilled = elem.stroke_width >= 100;

      if (elem.element_type === 0) {
        // Line
        ctx.beginPath();
        ctx.moveTo(elem.x1, elem.y1);
        ctx.lineTo(elem.x2, elem.y2);
        ctx.stroke();
      } else if (elem.element_type === 1) {
        // Rectangle
        ctx.beginPath();
        ctx.rect(elem.x1, elem.y1, elem.x2 - elem.x1, elem.y2 - elem.y1);
        if (isFilled) ctx.fill();
        ctx.stroke();
      } else if (elem.element_type === 2) {
        // Circle
        ctx.beginPath();
        const radius = Math.sqrt(
          (elem.x2 - elem.x1) ** 2 + (elem.y2 - elem.y1) ** 2
        );
        ctx.arc(elem.x1, elem.y1, radius, 0, 2 * Math.PI);
        if (isFilled) ctx.fill();
        ctx.stroke();
      } else if (elem.element_type === 3) {
        // Text
        const textSize = elem.stroke_width || 24;
        ctx.font = `${textSize}px Inter, sans-serif`;
        ctx.fillStyle = color;
        ctx.fillText(elem.text || '', elem.x1, elem.y1);
      } else if (elem.element_type === 4) {
        // Triangle
        ctx.beginPath();
        const midX = (elem.x1 + elem.x2) / 2;
        ctx.moveTo(midX, elem.y1);
        ctx.lineTo(elem.x2, elem.y2);
        ctx.lineTo(elem.x1, elem.y2);
        ctx.closePath();
        if (isFilled) ctx.fill();
        ctx.stroke();
      } else if (elem.element_type === 5) {
        // Arrow
        ctx.beginPath();
        const angle = Math.atan2(elem.y2 - elem.y1, elem.x2 - elem.x1);
        const headLen = 20;
        ctx.moveTo(elem.x1, elem.y1);
        ctx.lineTo(elem.x2, elem.y2);
        ctx.lineTo(
          elem.x2 - headLen * Math.cos(angle - Math.PI / 6),
          elem.y2 - headLen * Math.sin(angle - Math.PI / 6)
        );
        ctx.moveTo(elem.x2, elem.y2);
        ctx.lineTo(
          elem.x2 - headLen * Math.cos(angle + Math.PI / 6),
          elem.y2 - headLen * Math.sin(angle + Math.PI / 6)
        );
        ctx.stroke();
      } else if (elem.element_type === 6) {
        // Star
        ctx.beginPath();
        const cx = (elem.x1 + elem.x2) / 2;
        const cy = (elem.y1 + elem.y2) / 2;
        const outerRadius =
          Math.sqrt((elem.x2 - elem.x1) ** 2 + (elem.y2 - elem.y1) ** 2) / 2;
        const innerRadius = outerRadius / 2;
        const spikes = 5;
        for (let i = 0; i < spikes * 2; i++) {
          const radius = i % 2 === 0 ? outerRadius : innerRadius;
          const angle = (Math.PI / spikes) * i - Math.PI / 2;
          const x = cx + Math.cos(angle) * radius;
          const y = cy + Math.sin(angle) * radius;
          if (i === 0) ctx.moveTo(x, y);
          else ctx.lineTo(x, y);
        }
        ctx.closePath();
        if (isFilled) ctx.fill();
        ctx.stroke();
      }
    });

    // Draw live preview
    if (isDrawing && currentTool !== 'text' && currentTool !== 'pan') {
      const hex = currentColor.replace('#', '');
      const r = parseInt(hex.substr(0, 2), 16);
      const g = parseInt(hex.substr(2, 2), 16);
      const b = parseInt(hex.substr(4, 2), 16);
      ctx.strokeStyle = `rgb(${r}, ${g}, ${b})`;
      ctx.fillStyle = `rgb(${r}, ${g}, ${b})`;
      ctx.lineWidth = brushSize;
      ctx.globalAlpha = 0.5;

      if (currentTool === 'line') {
        ctx.beginPath();
        ctx.moveTo(startPos.x, startPos.y);
        ctx.lineTo(currentPos.x, currentPos.y);
        ctx.stroke();
      } else if (currentTool === 'rect') {
        ctx.beginPath();
        ctx.rect(
          startPos.x,
          startPos.y,
          currentPos.x - startPos.x,
          currentPos.y - startPos.y
        );
        if (fillEnabled) ctx.fill();
        ctx.stroke();
      } else if (currentTool === 'circle') {
        ctx.beginPath();
        const radius = Math.sqrt(
          (currentPos.x - startPos.x) ** 2 + (currentPos.y - startPos.y) ** 2
        );
        ctx.arc(startPos.x, startPos.y, radius, 0, 2 * Math.PI);
        if (fillEnabled) ctx.fill();
        ctx.stroke();
      } else if (currentTool === 'triangle') {
        ctx.beginPath();
        const midX = (startPos.x + currentPos.x) / 2;
        ctx.moveTo(midX, startPos.y);
        ctx.lineTo(currentPos.x, currentPos.y);
        ctx.lineTo(startPos.x, currentPos.y);
        ctx.closePath();
        if (fillEnabled) ctx.fill();
        ctx.stroke();
      } else if (currentTool === 'arrow') {
        ctx.beginPath();
        const angle = Math.atan2(
          currentPos.y - startPos.y,
          currentPos.x - startPos.x
        );
        const headLen = 20;
        ctx.moveTo(startPos.x, startPos.y);
        ctx.lineTo(currentPos.x, currentPos.y);
        ctx.lineTo(
          currentPos.x - headLen * Math.cos(angle - Math.PI / 6),
          currentPos.y - headLen * Math.sin(angle - Math.PI / 6)
        );
        ctx.moveTo(currentPos.x, currentPos.y);
        ctx.lineTo(
          currentPos.x - headLen * Math.cos(angle + Math.PI / 6),
          currentPos.y - headLen * Math.sin(angle + Math.PI / 6)
        );
        ctx.stroke();
      } else if (currentTool === 'star') {
        ctx.beginPath();
        const cx = (startPos.x + currentPos.x) / 2;
        const cy = (startPos.y + currentPos.y) / 2;
        const outerRadius =
          Math.sqrt(
            (currentPos.x - startPos.x) ** 2 + (currentPos.y - startPos.y) ** 2
          ) / 2;
        const innerRadius = outerRadius / 2;
        const spikes = 5;
        for (let i = 0; i < spikes * 2; i++) {
          const radius = i % 2 === 0 ? outerRadius : innerRadius;
          const angle = (Math.PI / spikes) * i - Math.PI / 2;
          const x = cx + Math.cos(angle) * radius;
          const y = cy + Math.sin(angle) * radius;
          if (i === 0) ctx.moveTo(x, y);
          else ctx.lineTo(x, y);
        }
        ctx.closePath();
        if (fillEnabled) ctx.fill();
        ctx.stroke();
      }

      ctx.globalAlpha = 1.0;
    }

    // Draw text input preview
    if (isAddingText && textInput) {
      ctx.font = `${fontSize}px Inter, sans-serif`;
      const hex = currentColor.replace('#', '');
      const r = parseInt(hex.substr(0, 2), 16);
      const g = parseInt(hex.substr(2, 2), 16);
      const b = parseInt(hex.substr(4, 2), 16);
      ctx.fillStyle = `rgba(${r}, ${g}, ${b}, 0.5)`;
      ctx.fillText(textInput, textPos.x, textPos.y);
    }

    ctx.restore();
  };

  const getCanvasCoords = (e) => {
    const canvas = canvasRef.current;
    const rect = canvas.getBoundingClientRect();
    const x = (e.clientX - rect.left - pan.x) / zoom;
    const y = (e.clientY - rect.top - pan.y) / zoom;
    return { x, y };
  };

  const findElementAtPoint = (x, y) => {
    // Check elements in reverse order (top to bottom)
    for (let i = elements.length - 1; i >= 0; i--) {
      const elem = elements[i];
      const hitMargin = 10; // Click tolerance

      if (elem.element_type === 0) {
        // Line
        const dist = pointToLineDistance(
          x,
          y,
          elem.x1,
          elem.y1,
          elem.x2,
          elem.y2
        );
        if (dist < hitMargin) return elem;
      } else if (elem.element_type === 1) {
        // Rectangle
        const minX = Math.min(elem.x1, elem.x2);
        const maxX = Math.max(elem.x1, elem.x2);
        const minY = Math.min(elem.y1, elem.y2);
        const maxY = Math.max(elem.y1, elem.y2);
        if (
          x >= minX - hitMargin &&
          x <= maxX + hitMargin &&
          y >= minY - hitMargin &&
          y <= maxY + hitMargin
        )
          return elem;
      } else if (elem.element_type === 2) {
        // Circle
        const radius = Math.sqrt(
          (elem.x2 - elem.x1) ** 2 + (elem.y2 - elem.y1) ** 2
        );
        const dist = Math.sqrt((x - elem.x1) ** 2 + (y - elem.y1) ** 2);
        if (Math.abs(dist - radius) < hitMargin || dist < radius) return elem;
      } else if (elem.element_type === 3) {
        // Text
        const textWidth = elem.text.length * (elem.stroke_width || 24) * 0.6;
        if (
          x >= elem.x1 - hitMargin &&
          x <= elem.x1 + textWidth + hitMargin &&
          y >= elem.y1 - (elem.stroke_width || 24) - hitMargin &&
          y <= elem.y1 + hitMargin
        )
          return elem;
      } else if (
        elem.element_type === 4 ||
        elem.element_type === 5 ||
        elem.element_type === 6
      ) {
        // Triangle, Arrow, Star
        const minX = Math.min(elem.x1, elem.x2);
        const maxX = Math.max(elem.x1, elem.x2);
        const minY = Math.min(elem.y1, elem.y2);
        const maxY = Math.max(elem.y1, elem.y2);
        if (
          x >= minX - hitMargin &&
          x <= maxX + hitMargin &&
          y >= minY - hitMargin &&
          y <= maxY + hitMargin
        )
          return elem;
      }
    }
    return null;
  };

  const pointToLineDistance = (px, py, x1, y1, x2, y2) => {
    const A = px - x1;
    const B = py - y1;
    const C = x2 - x1;
    const D = y2 - y1;
    const dot = A * C + B * D;
    const lenSq = C * C + D * D;
    let param = -1;
    if (lenSq !== 0) param = dot / lenSq;
    let xx, yy;
    if (param < 0) {
      xx = x1;
      yy = y1;
    } else if (param > 1) {
      xx = x2;
      yy = y2;
    } else {
      xx = x1 + param * C;
      yy = y1 + param * D;
    }
    const dx = px - xx;
    const dy = py - yy;
    return Math.sqrt(dx * dx + dy * dy);
  };

  const handleMouseDown = async (e) => {
    const pos = getCanvasCoords(e);

    if (currentTool === 'pan') {
      setIsPanning(true);
      setPanStart({ x: e.clientX - pan.x, y: e.clientY - pan.y });
      return;
    }

    if (currentTool === 'eraser') {
      const element = findElementAtPoint(pos.x, pos.y);
      if (element) {
        try {
          await api.deleteWhiteboardElement(meetingId, element.element_id);
          await loadWhiteboard();
        } catch (error) {
          console.error('Error deleting element:', error);
        }
      }
      return;
    }

    if (currentTool === 'text') {
      setIsAddingText(true);
      setTextPos(pos);
      setTextInput('');
      setTimeout(() => textInputRef.current?.focus(), 0);
      return;
    }

    setIsDrawing(true);
    setStartPos(pos);
  };

  const handleMouseMove = (e) => {
    const pos = getCanvasCoords(e);
    setCurrentPos(pos);

    if (isPanning) {
      setPan({ x: e.clientX - panStart.x, y: e.clientY - panStart.y });
      return;
    }

    // Manual render trigger when drawing to show preview
    if (isDrawing) {
      renderWhiteboard();
    }
  };

  const handleMouseUp = async (e) => {
    if (isPanning) {
      setIsPanning(false);
      return;
    }

    if (!isDrawing) return;
    setIsDrawing(false);

    const endPos = getCanvasCoords(e);

    const toolMap = {
      line: 0,
      rect: 1,
      circle: 2,
      triangle: 4,
      arrow: 5,
      star: 6,
    };
    const elementType = toolMap[currentTool] || 0;

    const hex = currentColor.replace('#', '');
    const r = parseInt(hex.substr(0, 2), 16);
    const g = parseInt(hex.substr(2, 2), 16);
    const b = parseInt(hex.substr(4, 2), 16);

    const strokeWidth = fillEnabled ? brushSize + 100 : brushSize;

    try {
      console.log('Sending color values:', { r, g, b, currentColor });

      // Optimistic update - add element immediately
      const optimisticElement = {
        element_id: Date.now(), // Temporary ID
        element_type: elementType,
        x1: Math.floor(startPos.x),
        y1: Math.floor(startPos.y),
        x2: Math.floor(endPos.x),
        y2: Math.floor(endPos.y),
        color_r: r,
        color_g: g,
        color_b: b,
        stroke_width: strokeWidth,
        text: '',
      };
      setElements([...elements, optimisticElement]);

      await api.drawOnWhiteboard(meetingId, {
        element_type: elementType,
        x1: Math.floor(startPos.x),
        y1: Math.floor(startPos.y),
        x2: Math.floor(endPos.x),
        y2: Math.floor(endPos.y),
        color_r: r,
        color_g: g,
        color_b: b,
        stroke_width: strokeWidth,
        text: '',
      });
      await loadWhiteboard(); 
    } catch (error) {
      console.error('Error drawing:', error);
      await loadWhiteboard(); 
    }
  };

  const handleTextSubmit = async () => {
    if (!textInput.trim()) {
      setIsAddingText(false);
      return;
    }

    const hex = currentColor.replace('#', '');
    const r = parseInt(hex.substr(0, 2), 16);
    const g = parseInt(hex.substr(2, 2), 16);
    const b = parseInt(hex.substr(4, 2), 16);

    try {
      // Optimistic update - add text element immediately
      const optimisticElement = {
        element_id: Date.now(), // Temporary ID
        element_type: 3,
        x1: Math.floor(textPos.x),
        y1: Math.floor(textPos.y),
        x2: 0,
        y2: 0,
        color_r: r,
        color_g: g,
        color_b: b,
        stroke_width: fontSize,
        text: textInput,
      };
      setElements([...elements, optimisticElement]);
      setIsAddingText(false);
      setTextInput('');

      await api.drawOnWhiteboard(meetingId, {
        element_type: 3,
        x1: Math.floor(textPos.x),
        y1: Math.floor(textPos.y),
        x2: 0,
        y2: 0,
        color_r: r,
        color_g: g,
        color_b: b,
        stroke_width: fontSize,
        text: textInput,
      });
      await loadWhiteboard(); 
    } catch (error) {
      console.error('Error saving text:', error);
      await loadWhiteboard(); 
    }
  };

  const clearWhiteboard = async () => {
    if (!confirm('Clear the entire whiteboard?')) return;
    try {
      await api.clearWhiteboard(meetingId);
      await loadWhiteboard();
    } catch (error) {
      console.error('Error clearing whiteboard:', error);
    }
  };

  const tools = [
    { id: 'line', icon: Pencil, label: 'Line' },
    { id: 'rect', icon: Square, label: 'Rectangle' },
    { id: 'circle', icon: Circle, label: 'Circle' },
    { id: 'triangle', icon: Triangle, label: 'Triangle' },
    { id: 'arrow', icon: ArrowRight, label: 'Arrow' },
    { id: 'star', icon: Star, label: 'Star' },
    { id: 'text', icon: Type, label: 'Text' },
    { id: 'eraser', icon: Eraser, label: 'Eraser' },
    { id: 'pan', icon: Move, label: 'Pan' },
  ];

  return (
    <div className='h-full flex flex-col bg-gray-900'>
      <div className='px-4 py-2 border-b border-gray-700 bg-gray-800/50'>
        <div className='flex items-center justify-between'>
          <h2 className='text-lg font-bold text-white flex items-center gap-2'>
            <Palette className='w-5 h-5' />
            Infinite Whiteboard
          </h2>
          <span className='text-xs text-gray-400'>
            Zoom: {Math.round(zoom * 100)}%
          </span>
        </div>
      </div>

      <div className='px-4 py-2 border-b border-gray-700 bg-gray-800/30'>
        <div className='flex flex-wrap items-center gap-2'>
          <div className='flex gap-1 flex-wrap'>
            {tools.map((tool) => {
              const Icon = tool.icon;
              return (
                <button
                  key={tool.id}
                  onClick={() => {
                    setCurrentTool(tool.id);
                    setIsAddingText(false);
                  }}
                  className={`p-2 rounded-lg transition-all ${
                    currentTool === tool.id
                      ? 'bg-gradient-to-br from-blue-500 to-purple-600 text-white'
                      : 'bg-gray-800 text-gray-400 hover:bg-gray-700'
                  }`}
                  title={tool.label}
                >
                  <Icon className='w-4 h-4' />
                </button>
              );
            })}
          </div>

          <div className='h-6 w-px bg-gray-700'></div>

          <div className='flex gap-1 flex-wrap'>
            {colors.map((color) => (
              <button
                key={color}
                onClick={() => setCurrentColor(color)}
                className={`w-6 h-6 rounded-md transition-all ${
                  currentColor === color
                    ? 'ring-2 ring-blue-500 ring-offset-1 ring-offset-gray-900 scale-105'
                    : 'hover:scale-105'
                }`}
                style={{ backgroundColor: color }}
              />
            ))}
          </div>

          <div className='h-6 w-px bg-gray-700'></div>

          <div className='flex items-center gap-2'>
            <span className='text-xs text-gray-400'>
              {currentTool === 'text' ? 'Font:' : 'Size:'}
            </span>
            <input
              type='range'
              min={currentTool === 'text' ? '12' : '1'}
              max={currentTool === 'text' ? '72' : '20'}
              value={currentTool === 'text' ? fontSize : brushSize}
              onChange={(e) =>
                currentTool === 'text'
                  ? setFontSize(parseInt(e.target.value))
                  : setBrushSize(parseInt(e.target.value))
              }
              className='w-24'
            />
            <span className='text-xs font-semibold text-white w-8'>
              {currentTool === 'text' ? `${fontSize}px` : `${brushSize}px`}
            </span>
          </div>

          {!['line', 'arrow', 'text', 'pan'].includes(currentTool) && (
            <>
              <div className='h-6 w-px bg-gray-700'></div>
              <button
                onClick={() => setFillEnabled(!fillEnabled)}
                className={`flex items-center gap-1 px-2 py-1 rounded-lg text-xs transition-all ${
                  fillEnabled
                    ? 'bg-gradient-to-br from-blue-500 to-purple-600 text-white'
                    : 'bg-gray-800 text-gray-400 hover:bg-gray-700'
                }`}
              >
                <Palette className='w-3 h-3' />
                Fill
              </button>
            </>
          )}

          <div className='h-6 w-px bg-gray-700'></div>

          <div className='flex items-center gap-1'>
            <button
              onClick={() => setZoom(Math.max(zoom / 1.2, 0.1))}
              className='p-1 bg-gray-800 hover:bg-gray-700 text-gray-400 hover:text-white rounded'
            >
              <ZoomOut className='w-3 h-3' />
            </button>
            <span className='text-xs font-semibold text-white w-12 text-center'>
              {Math.round(zoom * 100)}%
            </span>
            <button
              onClick={() => setZoom(Math.min(zoom * 1.2, 5))}
              className='p-1 bg-gray-800 hover:bg-gray-700 text-gray-400 hover:text-white rounded'
            >
              <ZoomIn className='w-3 h-3' />
            </button>
            <button
              onClick={() => {
                setZoom(1);
                setPan({ x: 0, y: 0 });
              }}
              className='px-2 py-1 bg-gray-800 hover:bg-gray-700 text-gray-400 hover:text-white text-xs rounded'
            >
              Reset
            </button>
          </div>

          <div className='h-6 w-px bg-gray-700'></div>

          <button
            onClick={clearWhiteboard}
            className='flex items-center gap-1 px-2 py-1 bg-red-600 hover:bg-red-700 text-white rounded-lg text-xs'
          >
            <Trash2 className='w-3 h-3' />
            Clear
          </button>
        </div>
      </div>

      <div className='flex-1 flex items-center justify-center p-4 overflow-hidden bg-gray-200 relative'>
        <div className='bg-white rounded-2xl shadow-2xl overflow-hidden relative'>
          <canvas
            ref={canvasRef}
            width={3200}
            height={2400}
            onMouseDown={handleMouseDown}
            onMouseMove={handleMouseMove}
            onMouseUp={handleMouseUp}
            className={
              currentTool === 'pan'
                ? 'cursor-grab'
                : currentTool === 'eraser'
                ? 'cursor-pointer'
                : 'cursor-crosshair'
            }
            style={{ display: 'block' }}
          />

          {isAddingText && (
            <input
              ref={textInputRef}
              type='text'
              value={textInput}
              onChange={(e) => setTextInput(e.target.value)}
              onKeyDown={(e) => {
                if (e.key === 'Enter') handleTextSubmit();
                else if (e.key === 'Escape') setIsAddingText(false);
              }}
              onBlur={handleTextSubmit}
              className='absolute border-2 border-blue-500 bg-white/90 px-2 py-1 outline-none'
              style={{
                left: `${textPos.x * zoom + pan.x}px`,
                top: `${(textPos.y - fontSize) * zoom + pan.y}px`,
                color: currentColor,
                fontSize: `${fontSize * zoom}px`,
                minWidth: '100px',
              }}
            />
          )}
        </div>
      </div>

      <div className='px-4 py-2 border-t border-gray-700 bg-gray-800/30'>
        <p className='text-xs text-gray-500 text-center'>
          {currentTool === 'text'
            ? 'Click to place text • Type and press Enter • ESC to cancel'
            : currentTool === 'pan'
            ? 'Click and drag to move around • Use zoom controls'
            : 'Click and drag to draw • Use zoom/pan to navigate • Changes save automatically'}
        </p>
      </div>
    </div>
  );
}
