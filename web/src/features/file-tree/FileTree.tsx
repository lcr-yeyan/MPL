import { useState, type ReactNode } from 'react';
import {
  ChevronDown,
  ChevronRight,
  FileCode2,
  Folder,
  FolderOpen,
  Plus,
  Settings as SettingsIcon,
  Sparkles,
  X,
} from 'lucide-react';
import type { ExampleFile, SourceFile } from '../../types';

interface FileTreeProps {
  files: SourceFile[];
  examples: ExampleFile[];
  activeId: string;
  compilerVersion: string;
  onNew: () => void;
  onSelectFile: (id: string) => void;
  onCloseFile: (id: string) => void;
  onOpenExample: (example: ExampleFile) => void;
  onSettings: () => void;
}

function FolderRow({
  label,
  count,
  open,
  onToggle,
  action,
}: {
  label: string;
  count?: number;
  open: boolean;
  onToggle: () => void;
  action?: ReactNode;
}) {
  return (
    <div className="group flex h-8 items-center gap-1.5 px-3">
      <button type="button" onClick={onToggle} className="flex min-w-0 flex-1 items-center gap-1.5 text-left">
        <span className="grid h-4 w-4 place-items-center text-faint">
          {open ? <ChevronDown size={11} /> : <ChevronRight size={11} />}
        </span>
        {open ? <FolderOpen size={12} className="text-[var(--text-soft)]" /> : <Folder size={12} className="text-[var(--text-soft)]" />}
        <span className="text-[11px] font-medium text-[var(--text-soft)]">{label}</span>
        {typeof count === 'number' && <span className="mono ml-auto text-[8px] text-faint">{count}</span>}
      </button>
      {action}
    </div>
  );
}

function FileRow({
  name,
  active,
  dirty,
  example,
  onClick,
  onClose,
}: {
  name: string;
  active: boolean;
  dirty?: boolean;
  example?: boolean;
  onClick: () => void;
  onClose?: () => void;
}) {
  return (
    <div
      className={`group relative flex h-8 items-center gap-2 pl-8 pr-3 transition ${
        active ? 'bg-[var(--accent-soft)]' : 'hover:bg-[color-mix(in_srgb,var(--text)_4%,transparent)]'
      }`}
    >
      {active && <span className="absolute inset-y-1.5 left-0 w-[2px] bg-[var(--accent)]" />}
      {example ? (
        <Sparkles size={11} className={active ? 'text-accent' : 'text-faint'} />
      ) : (
        <FileCode2 size={11} className={active ? 'text-accent' : 'text-faint'} />
      )}
      <button type="button" onClick={onClick} className="mono min-w-0 flex-1 truncate text-left text-[10.5px] text-[var(--text-soft)] group-hover:text-[var(--text)]">
        {name}
      </button>
      {dirty && <span className="h-1 w-1 rounded-full bg-[var(--accent)]" />}
      {onClose && (
        <button
          type="button"
          onClick={(event) => {
            event.stopPropagation();
            onClose();
          }}
          className="hidden h-4 w-4 place-items-center rounded-full text-faint hover:bg-[color-mix(in_srgb,var(--text)_8%,transparent)] hover:text-[var(--text)] group-hover:grid"
          aria-label={`Close ${name}`}
        >
          <X size={9} />
        </button>
      )}
    </div>
  );
}

export function FileTree({
  files,
  examples,
  activeId,
  compilerVersion,
  onNew,
  onSelectFile,
  onCloseFile,
  onOpenExample,
  onSettings,
}: FileTreeProps) {
  const scoreFiles = files.filter((file) => file.kind !== 'example');
  const [scoresOpen, setScoresOpen] = useState(true);
  const [examplesOpen, setExamplesOpen] = useState(true);

  return (
    <aside className="flex w-[262px] shrink-0 flex-col border-r border-[var(--line)] bg-[color-mix(in_srgb,var(--surface)_78%,transparent)]">
      <div className="flex h-12 items-center justify-between px-4">
        <div>
          <div className="text-[9px] font-semibold uppercase tracking-[0.18em] text-faint">Explorer</div>
          <div className="mono mt-1 text-[9px] text-[var(--text-soft)]">workspace / local</div>
        </div>
        <button type="button" onClick={onNew} className="icon-btn h-7 w-7" title="New score">
          <Plus size={14} />
        </button>
      </div>

      <div className="min-h-0 flex-1 overflow-y-auto pb-4">
        <FolderRow
          label="My scores"
          count={scoreFiles.length}
          open={scoresOpen}
          onToggle={() => setScoresOpen((value) => !value)}
          action={
            <button type="button" onClick={onNew} className="hidden h-5 w-5 place-items-center rounded-full text-faint hover:text-[var(--text)] group-hover:grid" title="New score">
              <Plus size={11} />
            </button>
          }
        />
        {scoresOpen && (
          <div>
            {scoreFiles.map((file) => (
              <FileRow
                key={file.id}
                name={file.name}
                active={file.id === activeId}
                dirty={file.dirty}
                onClick={() => onSelectFile(file.id)}
                onClose={() => onCloseFile(file.id)}
              />
            ))}
            {scoreFiles.length === 0 && <div className="px-8 py-3 text-[10px] text-faint">No saved scores yet</div>}
          </div>
        )}

        <div className="mt-3">
          <FolderRow
            label="Examples"
            count={examples.length}
            open={examplesOpen}
            onToggle={() => setExamplesOpen((value) => !value)}
          />
          {examplesOpen && (
            <div>
              {examples.map((example) => (
                <FileRow
                  key={example.id}
                  name={example.name}
                  active={example.id === activeId}
                  example
                  onClick={() => onOpenExample(example)}
                />
              ))}
            </div>
          )}
        </div>

        <div className="mx-4 mt-5 border-t border-[var(--line)] pt-4">
          <div className="mono text-[8px] uppercase tracking-[0.16em] text-faint">Workspace</div>
          <div className="mt-2 grid grid-cols-2 gap-x-3 gap-y-2 text-[9px] text-[var(--text-soft)]">
            <span className="text-faint">engine</span>
            <span className="mono text-right">mplc</span>
            <span className="text-faint">version</span>
            <span className="mono text-right">{compilerVersion}</span>
            <span className="text-faint">mode</span>
            <span className="text-right">deterministic</span>
          </div>
        </div>
      </div>

      <div className="border-t border-[var(--line)] p-3">
        <button
          type="button"
          onClick={onSettings}
          className="flex w-full items-center gap-2 rounded-md px-2 py-2 text-left text-[10px] text-[var(--text-soft)] transition hover:bg-[color-mix(in_srgb,var(--text)_5%,transparent)] hover:text-[var(--text)]"
        >
          <SettingsIcon size={13} />
          Settings
          <span className="mono ml-auto text-[8px] text-faint">⌘,</span>
        </button>
      </div>
    </aside>
  );
}
