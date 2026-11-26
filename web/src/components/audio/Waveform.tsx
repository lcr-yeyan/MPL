import { useEffect, useRef, useState } from 'react';
import { Pause, Play, Volume2 } from 'lucide-react';
import type { AudioAnalysis } from '../../types';

interface WaveformProps {
  src: string;
  analysis: AudioAnalysis | null;
  theme: 'light' | 'dark';
}

function formatTime(value: number) {
  if (!Number.isFinite(value)) return '0:00';
  const minutes = Math.floor(value / 60);
  const seconds = Math.floor(value % 60);
  return `${minutes}:${seconds.toString().padStart(2, '0')}`;
}

export function Waveform({ src, analysis, theme }: WaveformProps) {
  const audioRef = useRef<HTMLAudioElement>(null);
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const [peaks, setPeaks] = useState<number[]>([]);
  const [playing, setPlaying] = useState(false);
  const [duration, setDuration] = useState(0);
  const [current, setCurrent] = useState(0);

  useEffect(() => {
    let cancelled = false;
    setPeaks([]);
    setPlaying(false);
    setCurrent(0);
    setDuration(0);
    fetch(src)
      .then((response) => response.arrayBuffer())
      .then(async (buffer) => {
        const context = new AudioContext();
        try {
          const decoded = await context.decodeAudioData(buffer.slice(0));
          if (cancelled) return;
          const channel = decoded.getChannelData(0);
          const bars = 96;
          const step = Math.max(1, Math.floor(channel.length / bars));
          const next: number[] = [];
          for (let index = 0; index < bars; index += 1) {
            let peak = 0;
            const start = index * step;
            for (let offset = 0; offset < step; offset += 8) {
              peak = Math.max(peak, Math.abs(channel[start + offset] || 0));
            }
            next.push(peak);
          }
          const maximum = Math.max(0.001, ...next);
          setPeaks(next.map((value) => Math.pow(value / maximum, 0.72)));
        } finally {
          void context.close();
        }
      })
      .catch(() => {
        if (!cancelled) {
          setPeaks(Array.from({ length: 96 }, (_, index) => 0.2 + 0.6 * Math.abs(Math.sin(index * 0.35))));
        }
      });
    return () => {
      cancelled = true;
    };
  }, [src]);

  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const draw = () => {
      const rect = canvas.getBoundingClientRect();
      const dpr = window.devicePixelRatio || 1;
      canvas.width = Math.max(1, Math.floor(rect.width * dpr));
      canvas.height = Math.max(1, Math.floor(rect.height * dpr));
      const ctx = canvas.getContext('2d');
      if (!ctx) return;
      ctx.scale(dpr, dpr);
      ctx.clearRect(0, 0, rect.width, rect.height);
      const bars = peaks.length || 96;
      const gap = 1.7;
      const barWidth = Math.max(1.5, (rect.width - gap * bars) / bars);
      const mid = rect.height / 2;
      const progress = duration > 0 ? current / duration : 0;
      const palette =
        theme === 'light'
          ? { played: 'rgba(32,88,232,0.98)', rest: 'rgba(32,88,232,0.52)', playhead: 'rgba(32,88,232,0.95)' }
          : { played: 'rgba(200,243,106,0.92)', rest: 'rgba(141,185,255,0.30)', playhead: 'rgba(200,243,106,0.95)' };

      ctx.fillStyle = theme === 'light' ? 'rgba(27,29,25,0.10)' : 'rgba(242,239,229,0.08)';
      ctx.fillRect(0, mid, rect.width, 1);

      for (let index = 0; index < bars; index += 1) {
        const value = Math.max(0.12, peaks[index] ?? 0.2);
        const height = Math.max(4, Math.min(rect.height - 8, value * (rect.height - 10) * 1.7));
        const x = index * (barWidth + gap);
        const played = index / bars <= progress;
        ctx.globalAlpha = played ? 1 : 0.82;
        ctx.fillStyle = played ? palette.played : palette.rest;
        ctx.fillRect(x, mid - height / 2, barWidth, height);
      }
      ctx.globalAlpha = 1;
      ctx.fillStyle = palette.playhead;
      ctx.fillRect(progress * rect.width, 0, 1.5, rect.height);
    };
    draw();
    const observer = new ResizeObserver(draw);
    observer.observe(canvas);
    return () => observer.disconnect();
  }, [peaks, current, duration, theme]);

  const toggle = async () => {
    const audio = audioRef.current;
    if (!audio) return;
    if (audio.paused) {
      await audio.play();
      setPlaying(true);
    } else {
      audio.pause();
      setPlaying(false);
    }
  };

  const progress = duration > 0 ? Math.min(100, (current / duration) * 100) : 0;

  return (
    <div className="space-y-4">
      <div className="relative overflow-hidden border-y border-[var(--line)] bg-[color-mix(in_srgb,var(--surface-soft)_40%,transparent)] px-3 py-3">
        <canvas
          ref={canvasRef}
          className="h-28 w-full cursor-pointer"
          onClick={(event) => {
            const audio = audioRef.current;
            if (!audio || !duration) return;
            const rect = event.currentTarget.getBoundingClientRect();
            audio.currentTime = ((event.clientX - rect.left) / rect.width) * duration;
          }}
        />
        <div className="absolute bottom-2 left-3 right-3 flex items-center justify-between mono text-[9px] text-faint">
          <span>{formatTime(current)}</span>
          <span>{formatTime(duration)}</span>
        </div>
      </div>
      <div className="flex items-center gap-3">
        <button
          type="button"
          onClick={toggle}
          className="grid h-10 w-10 place-items-center rounded-full bg-[var(--primary)] text-[var(--primary-text)] shadow-[var(--shadow-soft)] transition hover:scale-105"
          aria-label={playing ? 'Pause' : 'Play'}
        >
          {playing ? <Pause size={16} fill="currentColor" /> : <Play size={16} fill="currentColor" className="ml-0.5" />}
        </button>
        <div className="flex-1">
          <input
            type="range"
            min={0}
            max={100}
            value={progress}
            onChange={(event) => {
              const audio = audioRef.current;
              if (!audio || !duration) return;
              audio.currentTime = (Number(event.target.value) / 100) * duration;
            }}
            className="h-1 w-full appearance-none rounded-full bg-[var(--line-strong)]"
            aria-label="Seek"
          />
        </div>
        <Volume2 size={14} className="text-faint" />
      </div>
      <div className="grid grid-cols-2 border-y border-[var(--line)]">
        {[
          ['Integrated', analysis?.integratedLoudnessLufs, 'LUFS'],
          ['True peak', analysis?.truePeakDbtp, 'dBTP'],
          ['Sample peak', analysis?.samplePeakDbfs, 'dBFS'],
          ['Crest', analysis?.crestFactorDb, 'dB'],
        ].map(([label, value, unit], index) => (
          <div
            key={String(label)}
            className={`py-4 ${index % 2 === 0 ? 'border-r border-[var(--line)] pr-3' : 'pl-3'} ${index < 2 ? 'border-b border-[var(--line)]' : ''}`}
          >
            <div className="mono text-[8px] uppercase tracking-[0.16em] text-faint">{label}</div>
            <div className="mono mt-2 text-[17px] leading-none text-strong">
              {typeof value === 'number' ? value.toFixed(2) : '—'}
              <span className="ml-1 text-[9px] text-faint">{unit}</span>
            </div>
          </div>
        ))}
      </div>
      <audio
        ref={audioRef}
        src={src}
        onLoadedMetadata={(event) => setDuration(event.currentTarget.duration || 0)}
        onTimeUpdate={(event) => setCurrent(event.currentTarget.currentTime || 0)}
        onEnded={() => setPlaying(false)}
        onPause={() => setPlaying(false)}
        onPlay={() => setPlaying(true)}
        preload="auto"
      />
    </div>
  );
}
