import { X } from 'lucide-react';

export function AboutDialog({ onClose, compilerVersion }: { onClose: () => void; compilerVersion: string }) {
  return (
    <div className="dialog-backdrop fade-in fixed inset-0 z-[80] grid place-items-center p-6" onMouseDown={onClose}>
      <div
        className="dialog-panel relative w-[440px] max-w-full overflow-hidden rounded-xl p-7 text-center"
        onMouseDown={(event) => event.stopPropagation()}
      >
        <button type="button" onClick={onClose} className="icon-btn absolute right-3 top-3 h-8 w-8" aria-label="Close about">
          <X size={14} />
        </button>
        <img src="/brand/logo.png" alt="MPL Studio" className="brand-logo mx-auto h-16 w-16 object-contain" />
        <div className="mt-5 text-[22px] font-semibold tracking-[-0.04em] text-strong">MPL Studio</div>
        <div className="mono mt-2 text-[9px] uppercase tracking-[0.2em] text-faint">music, typed</div>
        <p className="mx-auto mt-5 max-w-[300px] text-[11px] leading-relaxed text-soft">
          A local-first editor for the Music Production Language. Compiler, standard library and
          examples are bundled; no model or network is required.
        </p>
        <div className="mt-6 flex items-center justify-center gap-4 border-t border-[var(--line)] pt-4">
          <span className="mono text-[9px] text-faint">mplc {compilerVersion}</span>
          <span className="h-3 w-px bg-[var(--line-strong)]" />
          <span className="mono text-[9px] text-faint">Tauri 2</span>
          <span className="h-3 w-px bg-[var(--line-strong)]" />
          <span className="mono text-[9px] text-faint">local only</span>
        </div>
      </div>
    </div>
  );
}
