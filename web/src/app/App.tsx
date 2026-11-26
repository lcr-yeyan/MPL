import { useCallback, useEffect, useMemo, useRef, useState } from 'react';
import {
  ChevronRight,
  Download,
  FileCode2,
  FilePlus2,
  Loader2,
  Play,
  RefreshCw,
  Settings as SettingsIcon,
  Sidebar,
  X,
  Zap,
} from 'lucide-react';
import { CodeEditor, type EditorHandle } from '../components/editor/CodeEditor';
import { MenuBar } from '../components/layout/MenuBar';
import { FileTree } from '../features/file-tree/FileTree';
import { Inspector, type InspectorPanel } from '../features/inspector/Inspector';
import { SettingsDialog } from '../features/settings/SettingsDialog';
import { AboutDialog } from '../features/settings/AboutDialog';
import { buildSource, checkSource, fetchExamples, fetchHealth, fetchReport } from '../lib/api';
import { useSettings } from './Settings';
import type { AudioAnalysis, BuildResult, CompilerReport, Diagnostic, ExampleFile, SourceFile } from '../types';

const STARTER = `project Untitled {
  tempo = 120 bpm;
  meter = 4/4;
  key = C major;
}

asset PianoAsset = instrument("GrandPiano");

let Cell = notes span 1 whole {
  C4 @ 0 for 1/4 vel 96,
  E4 @ 1/4 for 1/4 vel 96,
  G4 @ 1/2 for 1/4 vel 96
};

track Piano { asset = PianoAsset; role = melodic; }
section A(1 bars) { clip Piano "Cell" { play Cell; } }
arrangement { play A; }
export midi("untitled.mid");
export wav("untitled.wav");
`;

type RunState = 'idle' | 'checking' | 'building' | 'ok' | 'error';

const statusLabel: Record<RunState, string> = {
  idle: 'Ready',
  checking: 'Checking',
  building: 'Building',
  ok: 'Compiled',
  error: 'Issues',
};

export default function App() {
  const { settings, resolvedTheme } = useSettings();
  const [examples, setExamples] = useState<ExampleFile[]>([]);
  const [files, setFiles] = useState<SourceFile[]>([]);
  const [activeId, setActiveId] = useState('');
  const [diagnostics, setDiagnostics] = useState<Diagnostic[]>([]);
  const [buildResult, setBuildResult] = useState<BuildResult | null>(null);
  const [panel, setPanel] = useState<InspectorPanel>('problems');
  const [runState, setRunState] = useState<RunState>('idle');
  const [cursor, setCursor] = useState({ line: 1, column: 1 });
  const [lastChecked, setLastChecked] = useState('');
  const [sidebarOpen, setSidebarOpen] = useState(true);
  const [inspectorOpen, setInspectorOpen] = useState(true);
  const [inspectorWidth, setInspectorWidth] = useState(410);
  const [compilerVersion, setCompilerVersion] = useState('0.5.0-beta');
  const [toast, setToast] = useState('');
  const [settingsOpen, setSettingsOpen] = useState(false);
  const [settingsTab, setSettingsTab] = useState<'appearance' | 'editor' | 'workflow' | 'shortcuts'>('appearance');
  const [aboutOpen, setAboutOpen] = useState(false);
  const editorRef = useRef<EditorHandle>(null);
  const fileInputRef = useRef<HTMLInputElement>(null);
  const checkSeq = useRef(0);
  const resizeRef = useRef<{ startX: number; startWidth: number } | null>(null);

  const openSettings = (tab: 'appearance' | 'editor' | 'workflow' | 'shortcuts' = 'appearance') => {
    setSettingsTab(tab);
    setSettingsOpen(true);
  };

  const activeFile = useMemo(() => files.find((file) => file.id === activeId) ?? null, [files, activeId]);
  const errorCount = diagnostics.filter((diagnostic) => diagnostic.severity === 'error').length;
  const warningCount = diagnostics.filter((diagnostic) => diagnostic.severity === 'warning').length;

  useEffect(() => {
    let cancelled = false;
    fetchExamples()
      .then((loaded) => {
        if (cancelled) return;
        setExamples(loaded);
        const first = loaded.find((example) => example.name === 'math-rock-4min.mpl') ?? loaded[0];
        if (first) {
          const file: SourceFile = { id: first.id, name: first.name, content: first.content, dirty: false, kind: 'example' };
          setFiles([file]);
          setActiveId(file.id);
        }
      })
      .catch((error) => {
        if (!cancelled) {
          setRunState('error');
          setToast(`Compiler bridge unavailable: ${error.message}`);
        }
      });
    fetchHealth()
      .then((health) => health?.version && setCompilerVersion(health.version))
      .catch(() => undefined);
    return () => {
      cancelled = true;
    };
  }, []);

  useEffect(() => {
    if (!toast) return undefined;
    const timer = window.setTimeout(() => setToast(''), 4200);
    return () => window.clearTimeout(timer);
  }, [toast]);

  const updateActiveContent = useCallback(
    (content: string) => {
      if (!activeId) return;
      setFiles((current) => current.map((file) => (file.id === activeId ? { ...file, content, dirty: true } : file)));
    },
    [activeId],
  );

  const performCheck = useCallback(async (file: SourceFile, explicit: boolean) => {
    if (!file.content.trim()) {
      setDiagnostics([]);
      setRunState('idle');
      return;
    }
    const seq = ++checkSeq.current;
    if (explicit) setPanel('problems');
    setRunState('checking');
    try {
      const result = await checkSource(file.content, file.name);
      if (seq !== checkSeq.current) return;
      setDiagnostics(result.diagnostics);
      setRunState(result.diagnostics.some((item) => item.severity === 'error') ? 'error' : 'ok');
      setLastChecked(new Date().toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' }));
    } catch (error) {
      if (seq !== checkSeq.current) return;
      setRunState('error');
      setToast(error instanceof Error ? error.message : 'Check failed');
    }
  }, []);

  useEffect(() => {
    if (!activeFile || !settings.autoCheck) return undefined;
    const timer = window.setTimeout(() => void performCheck(activeFile, false), 850);
    return () => window.clearTimeout(timer);
  }, [activeFile?.id, activeFile?.content, performCheck, settings.autoCheck]);

  const runBuild = useCallback(async () => {
    if (!activeFile) return;
    setRunState('building');
    setPanel('sound');
    setBuildResult(null);
    try {
      const response = await buildSource(activeFile.content, activeFile.name);
      if (!response.ok) {
        setRunState('error');
        if (response.diagnostics) setDiagnostics(response.diagnostics);
        setToast(response.error || response.stderr || 'Build failed');
        setPanel('problems');
        return;
      }
      const result: BuildResult = {
        audioUrl: response.audioUrl,
        audioName: response.audioName,
        analysis: response.analysis as AudioAnalysis | null,
        report: response.report as CompilerReport | null,
        artifacts: response.artifacts,
        diagnostics: response.diagnostics ?? [],
        stdout: response.stdout,
        stderr: response.stderr,
      };
      setBuildResult(result);
      setDiagnostics(result.diagnostics);
      setRunState(result.diagnostics.some((item) => item.severity === 'error') ? 'error' : 'ok');
      setLastChecked(new Date().toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' }));
      if (result.artifacts.length) setToast(`Built ${result.artifacts.length} artifact(s)`);
    } catch (error) {
      setRunState('error');
      setToast(error instanceof Error ? error.message : 'Build failed');
    }
  }, [activeFile]);

  const runReport = useCallback(async () => {
    if (!activeFile) return;
    try {
      const response = await fetchReport(activeFile.content, activeFile.name);
      if (response.ok && response.report) {
        setBuildResult((current) => ({
          audioUrl: current?.audioUrl ?? '',
          audioName: current?.audioName ?? '',
          analysis: current?.analysis ?? null,
          artifacts: current?.artifacts ?? [],
          diagnostics: current?.diagnostics ?? [],
          stdout: current?.stdout,
          stderr: current?.stderr,
          ...current,
          report: response.report as CompilerReport,
        }));
        setPanel('report');
      } else {
        setToast('No report produced');
      }
    } catch (error) {
      setToast(error instanceof Error ? error.message : 'Report failed');
    }
  }, [activeFile]);

  const openExample = (example: ExampleFile) => {
    setFiles((current) => {
      if (current.some((file) => file.id === example.id)) return current;
      return [...current, { id: example.id, name: example.name, content: example.content, dirty: false, kind: 'example' }];
    });
    setActiveId(example.id);
    setDiagnostics([]);
  };

  const newFile = () => {
    const index = files.filter((file) => file.kind === 'untitled').length + 1;
    const id = `untitled:${Date.now()}`;
    setFiles((current) => [
      ...current,
      { id, name: `untitled-${index}.mpl`, content: STARTER.replace('Untitled', `Untitled${index}`), dirty: false, kind: 'untitled' },
    ]);
    setActiveId(id);
    setDiagnostics([]);
    window.setTimeout(() => editorRef.current?.focus(), 40);
  };

  const closeFile = (id: string) => {
    const index = files.findIndex((file) => file.id === id);
    const next = files.filter((file) => file.id !== id);
    setFiles(next);
    if (id === activeId) {
      const fallback = next[Math.max(0, index - 1)] ?? next[0];
      setActiveId(fallback?.id ?? '');
    }
  };

  const openUpload = () => fileInputRef.current?.click();

  const handleUpload = async (file: File | undefined) => {
    if (!file) return;
    const content = await file.text();
    const id = `upload:${Date.now()}`;
    setFiles((current) => [...current, { id, name: file.name, content, dirty: false, kind: 'untitled' }]);
    setActiveId(id);
    setDiagnostics([]);
  };

  const downloadActive = () => {
    if (!activeFile) return;
    const blob = new Blob([activeFile.content], { type: 'text/plain;charset=utf-8' });
    const url = URL.createObjectURL(blob);
    const anchor = document.createElement('a');
    anchor.href = url;
    anchor.download = activeFile.name;
    anchor.click();
    URL.revokeObjectURL(url);
    setToast(`Downloaded ${activeFile.name}`);
  };

  const closeActive = () => {
    if (activeFile) closeFile(activeFile.id);
  };

  useEffect(() => {
    const onKeyDown = (event: KeyboardEvent) => {
      const meta = event.ctrlKey || event.metaKey;
      if (!meta) return;
      if (event.key === 'Enter' && event.shiftKey) {
        event.preventDefault();
        void runBuild();
      } else if (event.key === 'Enter') {
        event.preventDefault();
        if (activeFile) void performCheck(activeFile, true);
      } else if (event.key.toLowerCase() === 's') {
        event.preventDefault();
        downloadActive();
      } else if (event.key.toLowerCase() === 'n') {
        event.preventDefault();
        newFile();
      } else if (event.key === ',') {
        event.preventDefault();
        setSettingsOpen(true);
      }
    };
    window.addEventListener('keydown', onKeyDown);
    return () => window.removeEventListener('keydown', onKeyDown);
  });

  useEffect(() => {
    const onMove = (event: MouseEvent) => {
      if (!resizeRef.current) return;
      setInspectorWidth(Math.max(340, Math.min(560, resizeRef.current.startWidth - (event.clientX - resizeRef.current.startX))));
    };
    const onUp = () => {
      resizeRef.current = null;
      document.body.style.cursor = '';
    };
    window.addEventListener('mousemove', onMove);
    window.addEventListener('mouseup', onUp);
    return () => {
      window.removeEventListener('mousemove', onMove);
      window.removeEventListener('mouseup', onUp);
    };
  }, []);

  return (
    <div className="app-shell" data-theme={resolvedTheme}>
      <header className="relative z-30 flex h-[56px] shrink-0 items-center gap-3 border-b border-[var(--line)] bg-[var(--surface)] px-3">
        <button type="button" onClick={() => setSidebarOpen((value) => !value)} className="icon-btn h-8 w-8 md:hidden" aria-label="Toggle files">
          <Sidebar size={15} />
        </button>
        <div className="flex items-center gap-2.5 pl-1">
          <img src="/brand/logo.png" alt="" className="brand-logo h-8 w-8 object-contain" />
          <div className="leading-none">
            <div className="text-[13px] font-semibold tracking-[-0.02em] text-strong">
              MPL <span className="font-normal text-soft">Studio</span>
            </div>
            <div className="mono mt-1 text-[7.5px] uppercase tracking-[0.24em] text-faint">music, typed</div>
          </div>
        </div>
        <div className="mx-1 hidden h-4 w-px bg-[var(--line)] md:block" />
        <MenuBar
          canClose={Boolean(activeFile)}
          onNew={newFile}
          onOpen={openUpload}
          onDownload={downloadActive}
          onClose={closeActive}
          onCheck={() => activeFile && void performCheck(activeFile, true)}
          onBuild={() => void runBuild()}
          onToggleSidebar={() => setSidebarOpen((value) => !value)}
          onToggleInspector={() => setInspectorOpen((value) => !value)}
          onSettings={() => openSettings()}
          onShortcuts={() => openSettings('shortcuts')}
          onAbout={() => setAboutOpen(true)}
        />
        <div className="ml-auto flex items-center gap-2">
          <div className="hidden items-center gap-2 lg:flex">
            <span className={`h-1.5 w-1.5 rounded-full ${runState === 'error' ? 'bg-[var(--danger)]' : runState === 'building' ? 'bg-[var(--warning)]' : 'bg-[var(--success)]'} live-dot`} />
            <span className="mono text-[8.5px] uppercase tracking-[0.12em] text-faint">
              {statusLabel[runState]} <span className="mx-1 opacity-40">/</span> {compilerVersion}
            </span>
          </div>
          <button
            type="button"
            onClick={() => activeFile && void performCheck(activeFile, true)}
            disabled={!activeFile || runState === 'checking' || runState === 'building'}
            className="flex h-8 items-center gap-1.5 px-2 text-[10px] font-medium text-soft transition hover:text-strong disabled:opacity-40"
          >
            {runState === 'checking' ? <Loader2 size={12} className="animate-spin" /> : <Zap size={12} className="text-accent" />}
            Check
          </button>
          <button
            type="button"
            onClick={() => void runBuild()}
            disabled={!activeFile || runState === 'building'}
            className="flex h-8 items-center gap-1.5 rounded-full bg-[var(--primary)] px-3.5 text-[10px] font-semibold text-[var(--primary-text)] transition hover:brightness-105 disabled:opacity-50"
          >
            {runState === 'building' ? <Loader2 size={12} className="animate-spin" /> : <Play size={12} fill="currentColor" />}
            Build &amp; Play
          </button>
          <button type="button" onClick={() => openSettings()} className="icon-btn h-8 w-8" aria-label="Settings">
            <SettingsIcon size={14} />
          </button>
          <button type="button" onClick={() => setInspectorOpen((value) => !value)} className="icon-btn h-8 w-8" aria-label="Toggle inspector">
            <ChevronRight size={15} className={`transition ${inspectorOpen ? '' : 'rotate-180'}`} />
          </button>
        </div>
      </header>

      <div className="flex min-h-0 flex-1">
        {sidebarOpen && (
          <div className="desktop-sidebar flex min-h-0">
            <FileTree
              files={files}
              examples={examples}
              activeId={activeId}
              compilerVersion={compilerVersion}
              onNew={newFile}
              onSelectFile={setActiveId}
              onCloseFile={closeFile}
              onOpenExample={openExample}
              onSettings={() => openSettings()}
            />
          </div>
        )}

        <main className="flex min-w-0 flex-1 flex-col bg-editor">
          <div className="flex h-[38px] shrink-0 items-center gap-1 overflow-x-auto px-3">
            {files.map((file) => {
              const active = file.id === activeId;
              return (
                <div key={file.id} className={`group flex shrink-0 items-center gap-2 border-b py-2 ${active ? 'border-[var(--accent)]' : 'border-transparent'}`}>
                  <button type="button" onClick={() => setActiveId(file.id)} className="flex items-center gap-1.5">
                    <FileCode2 size={11} className={active ? 'text-accent' : 'text-faint'} />
                    <span className={`mono max-w-[160px] truncate text-[9.5px] ${active ? 'text-strong' : 'text-soft'}`}>{file.name}</span>
                    {file.dirty && <span className="h-1 w-1 rounded-full bg-[var(--accent)]" />}
                  </button>
                  <button type="button" onClick={() => closeFile(file.id)} className="hidden h-4 w-4 place-items-center rounded-full text-faint hover:text-strong group-hover:grid" aria-label={`Close ${file.name}`}>
                    <X size={9} />
                  </button>
                </div>
              );
            })}
            <button type="button" onClick={newFile} className="ml-1 grid h-6 w-6 shrink-0 place-items-center rounded-full text-faint transition hover:bg-[var(--accent-soft)] hover:text-strong" aria-label="New file">
              <FilePlus2 size={12} />
            </button>
            <div className="ml-auto flex shrink-0 items-center gap-3 pl-4">
              <span className="mono text-[8px] text-faint">MPL 0.4</span>
              <span className="h-2.5 w-px bg-[var(--line-strong)]" />
              <span className="mono text-[8px] text-faint">UTF-8</span>
            </div>
          </div>

          <div className="flex h-[34px] shrink-0 items-center gap-2 px-4">
            <span className="mono text-[8px] text-faint">examples</span>
            <ChevronRight size={9} className="text-faint" />
            <span className="mono text-[9px] text-soft">{activeFile?.name ?? '—'}</span>
            <div className="ml-auto flex items-center gap-1">
              <button type="button" onClick={downloadActive} className="icon-btn h-6 w-6" title="Download source">
                <Download size={11} />
              </button>
              <button type="button" onClick={() => activeFile && void performCheck(activeFile, true)} className="icon-btn h-6 w-6" title="Check">
                <RefreshCw size={11} />
              </button>
            </div>
          </div>

          <div className="relative min-h-0 flex-1 bg-editor">
            {activeFile ? (
              <CodeEditor
                ref={editorRef}
                value={activeFile.content}
                documentId={activeFile.id}
                diagnostics={diagnostics}
                wordWrap={settings.wordWrap}
                onChange={updateActiveContent}
                onCursor={(line, column) => setCursor({ line, column })}
              />
            ) : (
              <div className="grid h-full place-items-center">
                <div className="text-center">
                  <img src="/brand/logo.png" alt="" className="brand-logo mx-auto h-12 w-12 object-contain opacity-60" />
                  <div className="mt-4 text-[13px] font-medium text-strong">No score open</div>
                  <div className="mt-1 text-[10px] text-soft">Choose an example or start a new score.</div>
                  <button type="button" onClick={newFile} className="mt-4 rounded-full bg-[var(--primary)] px-4 py-1.5 text-[10px] font-semibold text-[var(--primary-text)]">
                    New score
                  </button>
                </div>
              </div>
            )}
            {runState === 'building' && (
              <div className="pointer-events-none absolute inset-x-0 top-0 h-px overflow-hidden bg-[var(--line)]">
                <div className="shimmer h-full w-full" />
              </div>
            )}
          </div>

          <div className="flex h-[26px] shrink-0 items-center gap-4 border-t border-[var(--line)] px-4 mono text-[8.5px] text-faint">
            <span>Ln {cursor.line}, Col {cursor.column}</span>
            <span>{activeFile?.content.length.toLocaleString() ?? 0} chars</span>
            <span>{activeFile ? activeFile.content.split('\n').length : 0} lines</span>
            <span className="ml-auto flex items-center gap-1.5">
              {errorCount > 0 ? <X size={9} className="text-danger" /> : <span className="text-success">✓</span>}
              {errorCount} errors <span className="opacity-40">/</span> {warningCount} warnings
            </span>
            <span className="hidden sm:inline">{settings.autoCheck ? `checked ${lastChecked || '—'}` : 'auto-check off'}</span>
          </div>
        </main>

        {inspectorOpen && (
          <>
            <div
              className="resize-handle"
              onMouseDown={(event) => {
                resizeRef.current = { startX: event.clientX, startWidth: inspectorWidth };
                document.body.style.cursor = 'col-resize';
              }}
            />
            <div style={{ width: inspectorWidth }} className="flex shrink-0 flex-col border-l border-[var(--line)]">
              <Inspector
                panel={panel}
                onPanelChange={setPanel}
                diagnostics={diagnostics}
                buildResult={buildResult}
                building={runState === 'building'}
                theme={resolvedTheme}
                compilerVersion={compilerVersion}
                lastChecked={lastChecked}
                onCheck={() => activeFile && void performCheck(activeFile, true)}
                onBuild={() => void runBuild()}
                onReport={() => void runReport()}
                onDiagnostic={(diagnostic) => editorRef.current?.gotoLine(diagnostic.line, diagnostic.column)}
              />
            </div>
          </>
        )}
      </div>

      <input ref={fileInputRef} type="file" accept=".mpl,text/plain" className="hidden" onChange={(event) => void handleUpload(event.target.files?.[0])} />
      {settingsOpen && <SettingsDialog onClose={() => setSettingsOpen(false)} initialTab={settingsTab} />}
      {aboutOpen && <AboutDialog onClose={() => setAboutOpen(false)} compilerVersion={compilerVersion} />}
      {toast && (
        <div className="slide-up fixed bottom-5 left-1/2 z-[90] -translate-x-1/2 rounded-full bg-[var(--primary)] px-4 py-2.5 text-[10px] font-medium text-[var(--primary-text)] shadow-[var(--shadow-panel)]">
          {toast}
        </div>
      )}
    </div>
  );
}
