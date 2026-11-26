import { useEffect, useState, type ReactNode } from 'react';
import { Check, Monitor, Moon, RotateCcw, Sun, X } from 'lucide-react';
import { useSettings, type ThemeMode } from '../../app/Settings';

function Toggle({ checked, onChange }: { checked: boolean; onChange: (value: boolean) => void }) {
  return (
    <button
      type="button"
      role="switch"
      aria-checked={checked}
      onClick={() => onChange(!checked)}
      className={`relative h-5 w-9 rounded-full transition ${checked ? 'bg-[var(--accent)]' : 'bg-[var(--surface-soft)]'}`}
    >
      <span
        className={`absolute top-0.5 h-4 w-4 rounded-full bg-white shadow-sm transition ${checked ? 'left-[18px]' : 'left-0.5'}`}
      />
    </button>
  );
}

function Row({ label, hint, children }: { label: string; hint?: string; children: ReactNode }) {
  return (
    <div className="flex items-center justify-between gap-8 border-b border-[var(--line)] py-4 last:border-0">
      <div>
        <div className="text-[12px] text-strong">{label}</div>
        {hint && <div className="mt-1 text-[10px] leading-relaxed text-soft">{hint}</div>}
      </div>
      <div className="shrink-0">{children}</div>
    </div>
  );
}

function ThemeOption({
  value,
  current,
  onSelect,
  icon,
  label,
}: {
  value: ThemeMode;
  current: ThemeMode;
  onSelect: (value: ThemeMode) => void;
  icon: ReactNode;
  label: string;
}) {
  const active = current === value;
  return (
    <button
      type="button"
      onClick={() => onSelect(value)}
      className={`flex flex-1 items-center justify-center gap-2 rounded-md border py-2.5 text-[10px] transition ${
        active
          ? 'border-[var(--accent)] bg-[var(--accent-soft)] text-strong'
          : 'border-[var(--line)] text-soft hover:border-[var(--line-strong)] hover:text-strong'
      }`}
    >
      {icon}
      {label}
      {active && <Check size={10} className="text-accent" />}
    </button>
  );
}

export function SettingsDialog({
  onClose,
  initialTab = 'appearance',
}: {
  onClose: () => void;
  initialTab?: 'appearance' | 'editor' | 'workflow' | 'shortcuts';
}) {
  const { settings, update, reset } = useSettings();
  const [tab, setTab] = useState<'appearance' | 'editor' | 'workflow' | 'shortcuts'>(initialTab);

  useEffect(() => {
    const onKey = (event: KeyboardEvent) => {
      if (event.key === 'Escape') onClose();
    };
    window.addEventListener('keydown', onKey);
    return () => window.removeEventListener('keydown', onKey);
  }, [onClose]);

  return (
    <div className="dialog-backdrop fade-in fixed inset-0 z-[80] grid place-items-center p-6" onMouseDown={onClose}>
      <div
        className="dialog-panel flex max-h-[80vh] w-[720px] max-w-full flex-col overflow-hidden rounded-xl"
        onMouseDown={(event) => event.stopPropagation()}
      >
        <div className="flex items-center justify-between border-b border-[var(--line)] px-6 py-4">
          <div>
            <div className="text-[13px] font-semibold tracking-[-0.01em] text-strong">Settings</div>
            <div className="mono mt-1 text-[8px] uppercase tracking-[0.16em] text-faint">MPL Studio preferences</div>
          </div>
          <button type="button" onClick={onClose} className="icon-btn h-8 w-8" aria-label="Close settings">
            <X size={15} />
          </button>
        </div>

        <div className="flex min-h-0 flex-1">
          <div className="w-[164px] shrink-0 border-r border-[var(--line)] p-3">
            {([
              ['appearance', 'Appearance'],
              ['editor', 'Editor'],
              ['workflow', 'Workflow'],
              ['shortcuts', 'Shortcuts'],
            ] as const).map(([key, label]) => (
              <button
                key={key}
                type="button"
                onClick={() => setTab(key)}
                className={`relative flex w-full items-center rounded-md px-3 py-2 text-left text-[10px] transition ${
                  tab === key ? 'bg-[var(--accent-soft)] text-strong' : 'text-soft hover:text-strong'
                }`}
              >
                {tab === key && <span className="absolute left-0 h-4 w-[2px] bg-[var(--accent)]" />}
                {label}
              </button>
            ))}
          </div>

          <div className="min-h-0 flex-1 overflow-y-auto px-6 py-5">
            {tab === 'appearance' && (
              <div>
                <div className="mono mb-3 text-[8px] uppercase tracking-[0.18em] text-faint">Theme</div>
                <div className="flex gap-2">
                  <ThemeOption value="light" current={settings.theme} onSelect={(value) => update('theme', value)} icon={<Sun size={13} />} label="Light" />
                  <ThemeOption value="dark" current={settings.theme} onSelect={(value) => update('theme', value)} icon={<Moon size={13} />} label="Dark" />
                  <ThemeOption value="system" current={settings.theme} onSelect={(value) => update('theme', value)} icon={<Monitor size={13} />} label="System" />
                </div>
                <div className="mt-6">
                  <Row label="Interface motion" hint="Small transitions and the live compiler dot.">
                    <Toggle checked={settings.animations} onChange={(value) => update('animations', value)} />
                  </Row>
                </div>
              </div>
            )}

            {tab === 'editor' && (
              <div>
                <Row label="Font size" hint="CodeMirror editor text size.">
                  <div className="flex items-center gap-3">
                    <input
                      type="range"
                      min={11}
                      max={20}
                      step={0.5}
                      value={settings.fontSize}
                      onChange={(event) => update('fontSize', Number(event.target.value))}
                      className="w-32"
                    />
                    <span className="mono w-10 text-right text-[10px] text-soft">{settings.fontSize}px</span>
                  </div>
                </Row>
                <Row label="Soft wrap" hint="Wrap long lines instead of horizontal scrolling.">
                  <Toggle checked={settings.wordWrap} onChange={(value) => update('wordWrap', value)} />
                </Row>
                <Row label="Line numbers" hint="Always shown in this editor build.">
                  <span className="mono text-[9px] text-faint">on</span>
                </Row>
              </div>
            )}

            {tab === 'workflow' && (
              <div>
                <Row label="Automatic checks" hint="Run the compiler check after a short idle period.">
                  <Toggle checked={settings.autoCheck} onChange={(value) => update('autoCheck', value)} />
                </Row>
                <Row label="Compiler" hint="The local binary bundled with the desktop app.">
                  <span className="mono text-[9px] text-soft">mplc</span>
                </Row>
                <Row label="Audio" hint="Rendered WAV is kept in memory for this session.">
                  <span className="mono text-[9px] text-soft">48k · 2ch</span>
                </Row>
              </div>
            )}

            {tab === 'shortcuts' && (
              <div className="space-y-2">
                {[
                  ['Check score', 'Ctrl / ⌘ + Enter'],
                  ['Build & Play', 'Ctrl / ⌘ + Shift + Enter'],
                  ['Download source', 'Ctrl / ⌘ + S'],
                  ['New score', 'Ctrl / ⌘ + N'],
                  ['Settings', 'Ctrl / ⌘ + ,'],
                  ['Dismiss dialog', 'Esc'],
                ].map(([label, keys]) => (
                  <div key={label} className="flex items-center justify-between border-b border-[var(--line)] py-3 last:border-0">
                    <span className="text-[11px] text-soft">{label}</span>
                    <span className="mono text-[9px] text-strong">{keys}</span>
                  </div>
                ))}
              </div>
            )}
          </div>
        </div>

        <div className="flex items-center justify-between border-t border-[var(--line)] px-6 py-3">
          <button
            type="button"
            onClick={reset}
            className="flex items-center gap-1.5 text-[10px] text-faint transition hover:text-strong"
          >
            <RotateCcw size={11} />
            Reset defaults
          </button>
          <button type="button" onClick={onClose} className="rounded-md bg-[var(--primary)] px-4 py-1.5 text-[10px] font-semibold text-[var(--primary-text)]">
            Done
          </button>
        </div>
      </div>
    </div>
  );
}
