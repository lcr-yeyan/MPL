import { useMemo, useState, type ReactNode } from 'react';
import {
  AlertCircle,
  AlertTriangle,
  ArrowUpRight,
  Braces,
  Check,
  CircleDot,
  Clock3,
  Download,
  FileCode2,
  Gauge,
  Info,
  ListFilter,
  Loader2,
  Play,
  RefreshCw,
  ScrollText,
  Terminal,
  Volume2,
  X,
} from 'lucide-react';
import { Waveform } from '../../components/audio/Waveform';
import type { BuildResult, CompilerReport, Diagnostic } from '../../types';

export type InspectorPanel = 'problems' | 'output' | 'report' | 'sound';

interface InspectorProps {
  panel: InspectorPanel;
  onPanelChange: (panel: InspectorPanel) => void;
  diagnostics: Diagnostic[];
  buildResult: BuildResult | null;
  building: boolean;
  theme: 'light' | 'dark';
  compilerVersion: string;
  lastChecked: string;
  onCheck: () => void;
  onBuild: () => void;
  onReport: () => void;
  onDiagnostic: (diagnostic: Diagnostic) => void;
}

type SeverityFilter = 'all' | 'error' | 'warning' | 'info';

const prettyBytes = (bytes: number) => {
  if (bytes < 1024) return `${bytes} B`;
  if (bytes < 1024 * 1024) return `${(bytes / 1024).toFixed(1)} KB`;
  return `${(bytes / (1024 * 1024)).toFixed(1)} MB`;
};

const severityIcon = (severity: string, size = 13) => {
  if (severity === 'error') return <AlertCircle size={size} className="text-danger" />;
  if (severity === 'warning') return <AlertTriangle size={size} className="text-warning" />;
  if (severity === 'info') return <Info size={size} className="text-accent" />;
  return <CircleDot size={size} className="text-faint" />;
};

function SectionLabel({ children, action }: { children: ReactNode; action?: ReactNode }) {
  return (
    <div className="flex h-7 items-center justify-between">
      <span className="mono text-[8px] font-medium uppercase tracking-[0.18em] text-faint">{children}</span>
      {action}
    </div>
  );
}

function Empty({ icon, title, children }: { icon: ReactNode; title: string; children: ReactNode }) {
  return (
    <div className="px-1 py-12 text-center">
      <div className="mx-auto grid h-11 w-11 place-items-center rounded-full bg-[var(--accent-soft)] text-accent">{icon}</div>
      <div className="mt-4 text-[13px] font-medium text-strong">{title}</div>
      <div className="mx-auto mt-2 max-w-[230px] text-[10px] leading-relaxed text-soft">{children}</div>
    </div>
  );
}

export function Inspector({
  panel,
  onPanelChange,
  diagnostics,
  buildResult,
  building,
  theme,
  compilerVersion,
  lastChecked,
  onCheck,
  onBuild,
  onReport,
  onDiagnostic,
}: InspectorProps) {
  const [filter, setFilter] = useState<SeverityFilter>('all');
  const counts = (buildResult?.report?.counts ?? {}) as NonNullable<CompilerReport['counts']>;
  const modules = (buildResult?.report?.modules ?? []) as NonNullable<CompilerReport['modules']>;
  const errorCount = diagnostics.filter((item) => item.severity === 'error').length;
  const warningCount = diagnostics.filter((item) => item.severity === 'warning').length;
  const infoCount = diagnostics.filter((item) => item.severity === 'info' || item.severity === 'hint').length;
  const duration = buildResult?.analysis?.frames && buildResult.analysis.sampleRate
    ? buildResult.analysis.frames / buildResult.analysis.sampleRate
    : null;
  const filtered = filter === 'all' ? diagnostics : diagnostics.filter((item) => item.severity === filter);
  const logLines = [
    buildResult?.stdout,
    buildResult?.stderr,
    buildResult ? `\n[artifacts]\n${buildResult.artifacts.map((artifact) => `${artifact.name}  ${prettyBytes(artifact.bytes)}`).join('\n')}` : '',
  ]
    .filter(Boolean)
    .join('\n');

  const health = diagnostics.length === 0 ? 'clean' : errorCount > 0 ? 'error' : 'warning';
  const healthColor = health === 'clean' ? 'var(--success)' : health === 'warning' ? 'var(--warning)' : 'var(--danger)';

  const tabs = useMemo(
    () =>
      ([
        ['problems', 'Problems', ScrollText, diagnostics.length],
        ['output', 'Output', Terminal, buildResult?.artifacts.length ?? 0],
        ['report', 'Report', Gauge, modules.length],
        ['sound', 'Sound', Volume2, buildResult?.audioUrl ? 1 : 0],
      ] as const),
    [diagnostics.length, buildResult?.artifacts.length, modules.length, buildResult?.audioUrl],
  );

  return (
    <aside className="flex min-h-0 flex-1 flex-col bg-[color-mix(in_srgb,var(--surface)_70%,transparent)]">
      <div className="shrink-0 px-5 pb-3 pt-4">
        <div className="flex items-center justify-between">
          <div className="flex items-center gap-2">
            <span className="h-1.5 w-1.5 rounded-full" style={{ background: healthColor }} />
            <span className="text-[9px] font-semibold uppercase tracking-[0.2em] text-soft">Inspector</span>
          </div>
          <span className="mono text-[8px] uppercase tracking-[0.12em] text-faint">mplc {compilerVersion}</span>
        </div>

        <div className="mt-3 flex items-end justify-between gap-3">
          <div className="min-w-0">
            <div className="flex items-baseline gap-2">
              <span className="text-[26px] font-medium leading-none tracking-[-0.05em] text-strong">
                {diagnostics.length === 0 ? 'Clean' : `${diagnostics.length}`}
              </span>
              <span className="text-[11px] text-soft">{diagnostics.length === 0 ? 'compile' : diagnostics.length === 1 ? 'issue' : 'issues'}</span>
            </div>
            <div className="mt-1.5 text-[9px] text-faint">
              {diagnostics.length === 0 ? 'No compiler diagnostics.' : `${errorCount} errors · ${warningCount} warnings · ${infoCount} notes`}
            </div>
          </div>
          <button
            type="button"
            onClick={onCheck}
            className="flex shrink-0 items-center gap-1.5 pb-0.5 text-[9px] text-accent transition hover:opacity-70"
          >
            <RefreshCw size={10} />
            Run check
          </button>
        </div>

        <div className="mt-4 grid grid-cols-3 border-y border-[var(--line)] py-2">
          <div className="pr-2">
            <div className="mono text-[8px] uppercase tracking-[0.14em] text-faint">Errors</div>
            <div className={`mono mt-1 text-[13px] ${errorCount ? 'text-danger' : 'text-strong'}`}>{errorCount}</div>
          </div>
          <div className="border-x border-[var(--line)] px-2.5">
            <div className="mono text-[8px] uppercase tracking-[0.14em] text-faint">Warnings</div>
            <div className={`mono mt-1 text-[13px] ${warningCount ? 'text-warning' : 'text-strong'}`}>{warningCount}</div>
          </div>
          <div className="pl-2.5">
            <div className="mono text-[8px] uppercase tracking-[0.14em] text-faint">Last check</div>
            <div className="mono mt-1 text-[11px] text-soft">{lastChecked || '—'}</div>
          </div>
        </div>
      </div>

      <div className="grid h-[38px] shrink-0 grid-cols-4 border-b border-[var(--line)] px-2">
        {tabs.map(([key, label, Icon, count]) => (
          <button
            key={key}
            type="button"
            onClick={() => {
              onPanelChange(key);
              if (key === 'report' && !buildResult?.report) onReport();
            }}
            className={`relative flex items-center justify-center gap-1.5 text-[9px] font-medium transition ${
              panel === key ? 'text-strong' : 'text-faint hover:text-soft'
            }`}
          >
            <Icon size={11} />
            {label}
            {key === 'problems' && count > 0 && <span className="mono text-[8px] text-danger">{count}</span>}
            {key !== 'problems' && count > 0 && <span className="mono text-[8px] text-faint">{count}</span>}
            {panel === key && <span className="absolute inset-x-2 bottom-0 h-[1.5px] bg-[var(--accent)]" />}
          </button>
        ))}
      </div>

      <div className="min-h-0 flex-1 overflow-y-auto">
        {panel === 'problems' && (
          <div className="px-5 py-4">
            <SectionLabel
              action={
                diagnostics.length > 0 ? (
                  <button
                    type="button"
                    onClick={() => {
                      const order: SeverityFilter[] = ['all', 'error', 'warning', 'info'];
                      setFilter(order[(order.indexOf(filter) + 1) % order.length]);
                    }}
                    className="flex items-center gap-1 text-[8px] text-faint transition hover:text-strong"
                  >
                    <ListFilter size={9} />
                    {filter === 'all' ? 'filter' : filter}
                  </button>
                ) : undefined
              }
            >
              Diagnostics
            </SectionLabel>

            {diagnostics.length === 0 ? (
              <div className="mt-2">
                <div className="border-y border-[var(--line)]">
                  {[
                    ['Syntax', 'parser'],
                    ['Semantics', 'lowering'],
                    ['Exports', 'artifacts'],
                  ].map(([label, detail]) => (
                    <div key={label} className="flex items-center gap-3 border-b border-[var(--line)] py-3 last:border-0">
                      <span className="grid h-5 w-5 place-items-center rounded-full bg-[color-mix(in_srgb,var(--success)_12%,transparent)]">
                        <Check size={11} className="text-success" />
                      </span>
                      <span className="text-[11px] text-strong">{label}</span>
                      <span className="mono ml-auto text-[8px] uppercase tracking-[0.1em] text-faint">{detail}</span>
                    </div>
                  ))}
                </div>
                <div className="mt-4 flex gap-2 text-[9px] leading-relaxed text-soft">
                  <ArrowUpRight size={12} className="mt-0.5 shrink-0 text-accent" />
                  <p>
                    Ready for sound. Use <span className="text-strong">Build &amp; Play</span> to compile, analyze and
                    render this score.
                  </p>
                </div>
                <div className="mt-6">
                  <SectionLabel>Run context</SectionLabel>
                  <div className="grid grid-cols-2 gap-x-4 gap-y-2 border-t border-[var(--line)] pt-3">
                    <span className="text-[9px] text-faint">Engine</span>
                    <span className="mono text-right text-[9px] text-soft">mplc {compilerVersion}</span>
                    <span className="text-[9px] text-faint">Artifacts</span>
                    <span className="mono text-right text-[9px] text-soft">{buildResult?.artifacts.length ?? 0}</span>
                    <span className="text-[9px] text-faint">Render</span>
                    <span className="mono text-right text-[9px] text-soft">48k stereo</span>
                  </div>
                </div>
              </div>
            ) : filtered.length === 0 ? (
              <div className="py-10 text-center text-[10px] text-faint">No {filter} diagnostics.</div>
            ) : (
              <div className="mt-1">
                {filtered.map((diagnostic, index) => (
                  <button
                    key={`${diagnostic.code}-${index}`}
                    type="button"
                    onClick={() => onDiagnostic(diagnostic)}
                    className="group flex w-full gap-2.5 border-b border-[var(--line)] py-3 text-left last:border-0"
                  >
                    <span className="mt-0.5 shrink-0">{severityIcon(String(diagnostic.severity))}</span>
                    <span className="min-w-0 flex-1">
                      <span className="block text-[10.5px] leading-snug text-soft group-hover:text-strong">{diagnostic.message}</span>
                      <span className="mono mt-1 flex items-center gap-2 text-[8.5px] text-faint">
                        <span>{diagnostic.code}</span>
                        <span>Ln {diagnostic.line}:{diagnostic.column}</span>
                      </span>
                    </span>
                    <ArrowUpRight size={11} className="mt-1 shrink-0 text-faint opacity-0 transition group-hover:opacity-100" />
                  </button>
                ))}
              </div>
            )}
          </div>
        )}

        {panel === 'output' && (
          <div className="flex min-h-full flex-col px-5 py-4">
            <SectionLabel action={<span className="mono text-[8px] text-faint">{buildResult ? `${buildResult.artifacts.length} artifacts` : 'idle'}</span>}>
              Build output
            </SectionLabel>
            <div className="-mx-5 mt-2 border-y border-[var(--line)]">
              <div className="flex items-center gap-2 bg-[color-mix(in_srgb,var(--surface-soft)_35%,transparent)] px-5 py-2.5">
                <Terminal size={11} className="text-accent" />
                <span className="mono text-[9px] text-soft">mplc build</span>
                <span className="ml-auto mono text-[8px] text-faint">local</span>
              </div>
              <pre className="max-h-[420px] min-h-[220px] overflow-auto whitespace-pre-wrap bg-[color-mix(in_srgb,var(--surface-soft)_22%,transparent)] px-5 py-3 mono text-[8.5px] leading-relaxed text-soft">
                {logLines || 'Build output will appear here.'}
              </pre>
            </div>
            {buildResult && buildResult.artifacts.length > 0 && (
              <div className="mt-5">
                <SectionLabel>Artifacts</SectionLabel>
                {buildResult.artifacts.map((artifact) => (
                  <div key={artifact.name} className="flex items-center gap-2 border-b border-[var(--line)] py-2.5 last:border-0">
                    <FileCode2 size={11} className="text-accent" />
                    <span className="mono min-w-0 flex-1 truncate text-[9px] text-soft">{artifact.name}</span>
                    <span className="mono text-[8px] text-faint">{prettyBytes(artifact.bytes)}</span>
                  </div>
                ))}
              </div>
            )}
          </div>
        )}

        {panel === 'report' && (
          <div className="px-5 py-4">
            <SectionLabel
              action={
                <button type="button" onClick={onReport} className="mono text-[8px] text-accent transition hover:opacity-70">
                  refresh
                </button>
              }
            >
              Compiler report
            </SectionLabel>
            {!buildResult?.report ? (
              <Empty icon={<Gauge size={16} />} title="No report yet">
                Build the score first; the compiler report lists counts, modules and phases.
              </Empty>
            ) : (
              <div className="mt-2">
                <div className="grid grid-cols-2 border-y border-[var(--line)]">
                  {[
                    ['Objects', counts.musicObjects ?? '—'],
                    ['Notes', counts.notes ?? '—'],
                    ['Tracks', counts.tracks ?? '—'],
                    ['Clips', counts.clips ?? '—'],
                  ].map(([label, value], index) => (
                    <div
                      key={String(label)}
                      className={`py-4 ${index % 2 === 0 ? 'border-r border-[var(--line)] pr-3' : 'pl-3'} ${index < 2 ? 'border-b border-[var(--line)]' : ''}`}
                    >
                      <div className="mono text-[8px] uppercase tracking-[0.15em] text-faint">{label}</div>
                      <div className="mono mt-2 text-[25px] leading-none tracking-[-0.05em] text-strong">{String(value)}</div>
                    </div>
                  ))}
                </div>

                <div className="mt-5">
                  <SectionLabel action={<span className="mono text-[8px] text-faint">{modules.length} modules</span>}>Modules</SectionLabel>
                  {modules.map((module) => (
                    <div key={module.name} className="flex items-center gap-2 border-b border-[var(--line)] py-2.5 last:border-0">
                      <span className="h-1 w-1 rounded-full bg-[var(--accent)]" />
                      <span className="mono min-w-0 flex-1 truncate text-[9px] text-soft">{module.name}</span>
                      <span className="mono text-[8px] text-faint">{module.contentHash?.slice(0, 8)}</span>
                    </div>
                  ))}
                  {modules.length === 0 && <div className="py-3 text-[9px] text-faint">No module data.</div>}
                </div>

                <div className="mt-5">
                  <SectionLabel>Manifest</SectionLabel>
                  <div className="grid grid-cols-2 gap-x-4 gap-y-2 border-t border-[var(--line)] pt-3">
                    <span className="text-[9px] text-faint">Format</span>
                    <span className="mono text-right text-[9px] text-soft">{String(buildResult.report.format ?? '—')}</span>
                    <span className="text-[9px] text-faint">Language</span>
                    <span className="mono text-right text-[9px] text-soft">{String(buildResult.report.languageVersion ?? '—')}</span>
                    <span className="text-[9px] text-faint">Stdlib</span>
                    <span className="mono text-right text-[9px] text-soft">{String(buildResult.report.stdlibVersion ?? '—')}</span>
                    <span className="text-[9px] text-faint">Benchmark</span>
                    <span className="mono text-right text-[9px] text-soft">{String(buildResult.report.benchmarkVersion ?? '—')}</span>
                  </div>
                </div>

                <details className="group mt-4 border-y border-[var(--line)]">
                  <summary className="flex cursor-pointer list-none items-center gap-2 py-3 text-[9px] text-soft transition hover:text-strong">
                    <Braces size={11} className="text-faint" />
                    Raw report
                    <span className="mono ml-auto text-[8px] text-faint">JSON</span>
                  </summary>
                  <pre className="max-h-[280px] overflow-auto border-t border-[var(--line)] bg-[color-mix(in_srgb,var(--surface-soft)_24%,transparent)] py-3 mono text-[8px] leading-relaxed text-soft">
                    {JSON.stringify(buildResult.report, null, 2)}
                  </pre>
                </details>
              </div>
            )}
          </div>
        )}

        {panel === 'sound' && (
          <div className="px-5 py-4">
            <SectionLabel
              action={
                buildResult?.audioUrl ? (
                  <div className="flex items-center gap-3">
                    <button type="button" onClick={onBuild} disabled={building} className="flex items-center gap-1 text-[8px] text-faint transition hover:text-strong disabled:opacity-40">
                      {building ? <Loader2 size={9} className="animate-spin" /> : <RefreshCw size={9} />}
                      rebuild
                    </button>
                    <a href={buildResult.audioUrl} download={buildResult.audioName} className="text-faint transition hover:text-strong" title="Download WAV">
                      <Download size={10} />
                    </a>
                  </div>
                ) : undefined
              }
            >
              Rendered sound
            </SectionLabel>

            {!buildResult?.audioUrl ? (
              <div className="mt-2">
                <div className="border-y border-[var(--line)] py-4">
                  <div className="flex items-center gap-3">
                    <span className="grid h-8 w-8 place-items-center rounded-full bg-[var(--accent-soft)] text-accent">
                      <Volume2 size={15} />
                    </span>
                    <div>
                      <div className="text-[12px] font-medium text-strong">Not rendered</div>
                      <div className="mt-0.5 text-[9px] text-soft">A trusted WAV will appear here.</div>
                    </div>
                  </div>
                </div>
                <div className="mt-4">
                  {[
                    ['01', 'Compile score', 'lowering + validation'],
                    ['02', 'Render WAV', '48k stereo'],
                    ['03', 'Inspect loudness', 'LUFS + true peak'],
                  ].map(([step, label, detail], index) => (
                    <div key={step} className="flex items-center gap-3 border-b border-[var(--line)] py-2.5 last:border-0">
                      <span className={`mono text-[9px] ${index === 0 ? 'text-accent' : 'text-faint'}`}>{step}</span>
                      <span className="text-[10px] text-soft">{label}</span>
                      <span className="mono ml-auto text-[8px] text-faint">{detail}</span>
                    </div>
                  ))}
                </div>
                <button
                  type="button"
                  onClick={onBuild}
                  className="mt-5 flex w-full items-center justify-center gap-1.5 rounded-full bg-[var(--primary)] py-2.5 text-[10px] font-semibold text-[var(--primary-text)]"
                >
                  <Play size={11} fill="currentColor" /> Build &amp; Play
                </button>
                <div className="mt-5">
                  <SectionLabel>Output spec</SectionLabel>
                  <div className="grid grid-cols-2 gap-x-4 gap-y-2 border-t border-[var(--line)] pt-3">
                    <span className="text-[9px] text-faint">Format</span>
                    <span className="mono text-right text-[9px] text-soft">WAV · float32</span>
                    <span className="text-[9px] text-faint">Sample rate</span>
                    <span className="mono text-right text-[9px] text-soft">48 kHz</span>
                    <span className="text-[9px] text-faint">Channels</span>
                    <span className="mono text-right text-[9px] text-soft">2 · stereo</span>
                    <span className="text-[9px] text-faint">Tail</span>
                    <span className="mono text-right text-[9px] text-soft">0.75 s</span>
                  </div>
                </div>
              </div>
            ) : (
              <div className="mt-1">
                <div className="-mx-5">
                  <Waveform src={buildResult.audioUrl} analysis={buildResult.analysis} theme={theme} />
                </div>
                <div className="mt-4 border-y border-[var(--line)]">
                  <div className="flex items-center gap-2 py-2.5">
                    <span className="h-1.5 w-1.5 rounded-full bg-success" />
                    <span className="mono text-[8.5px] uppercase tracking-[0.12em] text-soft">Trusted render</span>
                    <span className="ml-auto mono text-[8px] text-faint">{buildResult.audioName}</span>
                  </div>
                  <div className="grid grid-cols-3 border-t border-[var(--line)] py-2.5">
                    <div>
                      <div className="mono text-[7.5px] uppercase tracking-[0.12em] text-faint">Length</div>
                      <div className="mono mt-1 text-[10px] text-strong">
                        {duration !== null ? `${Math.floor(duration / 60)}:${Math.floor(duration % 60).toString().padStart(2, '0')}` : '—'}
                      </div>
                    </div>
                    <div className="border-l border-[var(--line)] pl-3">
                      <div className="mono text-[7.5px] uppercase tracking-[0.12em] text-faint">Rate</div>
                      <div className="mono mt-1 text-[10px] text-strong">{buildResult.analysis?.sampleRate ? `${(buildResult.analysis.sampleRate / 1000).toFixed(0)}k` : '—'}</div>
                    </div>
                    <div className="border-l border-[var(--line)] pl-3">
                      <div className="mono text-[7.5px] uppercase tracking-[0.12em] text-faint">Channels</div>
                      <div className="mono mt-1 text-[10px] text-strong">{buildResult.analysis?.channels ?? '—'}</div>
                    </div>
                  </div>
                </div>
                <div className="mt-3 flex items-center gap-2 text-[9px] text-faint">
                  <Clock3 size={11} />
                  Frames {buildResult.analysis?.frames?.toLocaleString() ?? '—'}
                  <span className="ml-auto mono text-[8px]">{buildResult.audioName}</span>
                </div>
                {buildResult.artifacts.length > 0 && (
                  <div className="mt-5">
                    <SectionLabel action={<span className="mono text-[8px] text-faint">{buildResult.artifacts.length}</span>}>
                      Render artifacts
                    </SectionLabel>
                    {buildResult.artifacts.map((artifact) => (
                      <div key={artifact.name} className="flex items-center gap-2 border-b border-[var(--line)] py-2.5 last:border-0">
                        <FileCode2 size={11} className="text-accent" />
                        <span className="mono min-w-0 flex-1 truncate text-[9px] text-soft">{artifact.name}</span>
                        <span className="mono text-[8px] text-faint">{prettyBytes(artifact.bytes)}</span>
                      </div>
                    ))}
                  </div>
                )}
              </div>
            )}
          </div>
        )}
      </div>
      <div className="flex h-8 shrink-0 items-center border-t border-[var(--line)] px-5 mono text-[8px] text-faint">
        <span>MPL 0.4</span>
        <span className="mx-2 opacity-40">/</span>
        <span>mplc {compilerVersion}</span>
        <span className="ml-auto">local · deterministic</span>
      </div>
    </aside>
  );
}
