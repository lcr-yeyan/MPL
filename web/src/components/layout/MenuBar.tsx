import { useEffect, useRef, useState, type ReactNode } from 'react';
import {
  Download,
  FilePlus2,
  FolderOpen,
  Info,
  Keyboard,
  Monitor,
  Moon,
  PanelRight,
  Play,
  Settings as SettingsIcon,
  Sun,
  TerminalSquare,
  X,
  Zap,
} from 'lucide-react';
import { useSettings, type ThemeMode } from '../../app/Settings';

export interface MenuActions {
  canClose: boolean;
  onNew: () => void;
  onOpen: () => void;
  onDownload: () => void;
  onClose: () => void;
  onCheck: () => void;
  onBuild: () => void;
  onToggleSidebar: () => void;
  onToggleInspector: () => void;
  onSettings: () => void;
  onShortcuts: () => void;
  onAbout: () => void;
}

interface MenuBarProps extends MenuActions {}

const menuLabels = ['File', 'Edit', 'View', 'Help'] as const;
type MenuLabel = (typeof menuLabels)[number];

function Item({
  icon,
  label,
  shortcut,
  disabled,
  onClick,
}: {
  icon: ReactNode;
  label: string;
  shortcut?: string;
  disabled?: boolean;
  onClick?: () => void;
}) {
  return (
    <button type="button" className="menu-item" disabled={disabled} onClick={onClick}>
      <span className="grid w-4 place-items-center text-faint">{icon}</span>
      <span className="flex-1">{label}</span>
      {shortcut && <span className="mono text-[8px] text-faint">{shortcut}</span>}
    </button>
  );
}

function Separator() {
  return <div className="my-1 h-px bg-[var(--line)]" />;
}

export function MenuBar(actions: MenuBarProps) {
  const { settings, update } = useSettings();
  const [open, setOpen] = useState<MenuLabel | null>(null);
  const ref = useRef<HTMLDivElement>(null);

  useEffect(() => {
    const onClick = (event: MouseEvent) => {
      if (!ref.current?.contains(event.target as Node)) setOpen(null);
    };
    window.addEventListener('mousedown', onClick);
    return () => window.removeEventListener('mousedown', onClick);
  }, []);

  const run = (action: () => void) => {
    setOpen(null);
    action();
  };

  const setTheme = (theme: ThemeMode) => {
    update('theme', theme);
    setOpen(null);
  };

  return (
    <div ref={ref} className="relative hidden h-9 items-center gap-0.5 md:flex">
      {menuLabels.map((label) => (
        <div key={label} className="relative">
          <button
            type="button"
            onClick={() => setOpen((current) => (current === label ? null : label))}
            className={`h-7 rounded px-2.5 text-[11px] transition ${
              open === label ? 'bg-[var(--accent-soft)] text-strong' : 'text-soft hover:bg-[color-mix(in_srgb,var(--text)_6%,transparent)] hover:text-strong'
            }`}
          >
            {label}
          </button>
          {open === label && (
            <div className="menu-popover fade-in absolute left-0 top-8 z-50">
              {label === 'File' && (
                <>
                  <Item icon={<FilePlus2 size={13} />} label="New score" shortcut="Ctrl N" onClick={() => run(actions.onNew)} />
                  <Item icon={<FolderOpen size={13} />} label="Open source…" onClick={() => run(actions.onOpen)} />
                  <Item icon={<Download size={13} />} label="Download source" shortcut="Ctrl S" onClick={() => run(actions.onDownload)} />
                  <Separator />
                  <Item icon={<X size={13} />} label="Close tab" disabled={!actions.canClose} onClick={() => run(actions.onClose)} />
                </>
              )}
              {label === 'Edit' && (
                <>
                  <Item icon={<Zap size={13} />} label="Check score" shortcut="Ctrl Enter" onClick={() => run(actions.onCheck)} />
                  <Item icon={<Play size={13} />} label="Build & Play" shortcut="Ctrl Shift Enter" onClick={() => run(actions.onBuild)} />
                  <Separator />
                  <Item icon={<TerminalSquare size={13} />} label="Open inspector" onClick={() => run(actions.onToggleInspector)} />
                </>
              )}
              {label === 'View' && (
                <>
                  <Item icon={<PanelRight size={13} />} label="Toggle file tree" onClick={() => run(actions.onToggleSidebar)} />
                  <Item icon={<PanelRight size={13} />} label="Toggle inspector" onClick={() => run(actions.onToggleInspector)} />
                  <Separator />
                  <Item
                    icon={<Sun size={13} />}
                    label={settings.theme === 'light' ? 'Light theme  ✓' : 'Light theme'}
                    onClick={() => setTheme('light')}
                  />
                  <Item icon={<Moon size={13} />} label={settings.theme === 'dark' ? 'Dark theme  ✓' : 'Dark theme'} onClick={() => setTheme('dark')} />
                  <Item icon={<Monitor size={13} />} label={settings.theme === 'system' ? 'Follow system  ✓' : 'Follow system'} onClick={() => setTheme('system')} />
                  <Separator />
                  <Item icon={<SettingsIcon size={13} />} label="Settings…" onClick={() => run(actions.onSettings)} />
                </>
              )}
              {label === 'Help' && (
                <>
                  <Item icon={<Keyboard size={13} />} label="Keyboard shortcuts" onClick={() => run(actions.onShortcuts)} />
                  <Item icon={<Info size={13} />} label="About MPL Studio" onClick={() => run(actions.onAbout)} />
                </>
              )}
            </div>
          )}
        </div>
      ))}
    </div>
  );
}
