import {
  forwardRef,
  useEffect,
  useImperativeHandle,
  useRef,
} from 'react';
import { basicSetup } from 'codemirror';
import { Compartment, EditorState } from '@codemirror/state';
import { EditorView, keymap } from '@codemirror/view';
import { defaultKeymap, historyKeymap, indentWithTab } from '@codemirror/commands';
import { searchKeymap } from '@codemirror/search';
import {
  autocompletion,
  type CompletionContext,
  type CompletionResult,
} from '@codemirror/autocomplete';
import { lintGutter, setDiagnostics, type Diagnostic as CmDiagnostic } from '@codemirror/lint';
import { HighlightStyle, syntaxHighlighting } from '@codemirror/language';
import { tags } from '@lezer/highlight';
import { mpl } from '../../lib/mplLanguage';
import type { Diagnostic } from '../../types';

export interface EditorHandle {
  gotoLine: (line: number, column?: number) => void;
  focus: () => void;
  getView: () => EditorView | null;
}

interface EditorProps {
  value: string;
  documentId: string;
  diagnostics: Diagnostic[];
  wordWrap: boolean;
  onChange: (value: string) => void;
  onCursor?: (line: number, column: number) => void;
}

const editorTheme = EditorView.theme(
  {
    '&': {
      height: '100%',
      backgroundColor: 'transparent',
      color: 'var(--text)',
      fontSize: 'var(--editor-font-size)',
    },
    '.cm-content': {
      fontFamily: "'JetBrains Mono', ui-monospace, SFMono-Regular, monospace",
      padding: '18px 0 120px',
      caretColor: 'var(--accent)',
      lineHeight: '1.8',
    },
    '.cm-line': { padding: '0 24px 0 8px' },
    '.cm-gutters': {
      backgroundColor: 'transparent',
      color: 'var(--text-faint)',
      border: 'none',
      paddingRight: '8px',
      paddingLeft: '16px',
      fontSize: '11px',
    },
    '.cm-lineNumbers .cm-gutterElement': { padding: '0 8px 0 0' },
    '.cm-activeLineGutter': { color: 'var(--accent)', backgroundColor: 'transparent' },
    '.cm-activeLine': { backgroundColor: 'var(--accent-soft)' },
    '.cm-cursor, .cm-dropCursor': { borderLeftColor: 'var(--accent)', borderLeftWidth: '2px' },
    '.cm-selectionBackground, ::selection': {
      backgroundColor: 'color-mix(in srgb, var(--accent) 20%, transparent)',
    },
    '&.cm-focused .cm-selectionBackground': {
      backgroundColor: 'color-mix(in srgb, var(--accent) 20%, transparent)',
    },
    '.cm-matchingBracket': {
      backgroundColor: 'var(--accent-soft)',
      outline: '1px solid color-mix(in srgb, var(--accent) 35%, transparent)',
    },
    '.cm-foldPlaceholder': {
      backgroundColor: 'var(--surface-soft)',
      border: 'none',
      color: 'var(--text-soft)',
    },
    '.cm-tooltip': {
      border: '1px solid var(--line)',
      backgroundColor: 'var(--surface-raised)',
      color: 'var(--text)',
    },
  },
  { dark: false },
);

const highlight = HighlightStyle.define([
  { tag: tags.keyword, color: 'var(--syntax-keyword)', fontWeight: '600' },
  { tag: tags.typeName, color: 'var(--syntax-type)' },
  { tag: tags.function(tags.variableName), color: 'var(--syntax-function)' },
  { tag: tags.variableName, color: 'var(--syntax-variable)' },
  { tag: tags.atom, color: 'var(--syntax-atom)' },
  { tag: tags.number, color: 'var(--syntax-number)' },
  { tag: tags.string, color: 'var(--syntax-string)' },
  { tag: tags.comment, color: 'var(--syntax-comment)', fontStyle: 'italic' },
  { tag: tags.operator, color: 'var(--syntax-operator)' },
  { tag: tags.bracket, color: 'var(--syntax-bracket)' },
  { tag: tags.bool, color: 'var(--syntax-keyword)' },
  { tag: tags.invalid, color: 'var(--danger)' },
]);

const completions = [
  'note', 'rest', 'sequence', 'parallel', 'tie', 'transpose', 'diatonicTranspose',
  'stretch', 'reverse', 'repeat', 'rotate', 'syncopate', 'chord', 'harmony', 'voicing',
  'voicingNotes', 'voiceLead', 'chordAtBar', 'rhythm', 'cycle', 'zip', 'length', 'range',
  'pitchClass', 'interval', 'section', 'track', 'clip', 'export', 'project', 'asset',
  'let', 'fn', 'return', 'context', 'if', 'for', 'in',
];

function mplCompletions(context: CompletionContext): CompletionResult | null {
  const word = context.matchBefore(/[A-Za-z_][A-Za-z0-9_]*/);
  if (!word || (word.from === word.to && !context.explicit)) return null;
  return {
    from: word.from,
    options: completions.map((label) => ({
      label,
      type: ['section', 'track', 'clip'].includes(label) ? 'keyword' : 'function',
    })),
  };
}

export const CodeEditor = forwardRef<EditorHandle, EditorProps>(function CodeEditor(
  { value, documentId, diagnostics, wordWrap, onChange, onCursor },
  ref,
) {
  const hostRef = useRef<HTMLDivElement>(null);
  const viewRef = useRef<EditorView | null>(null);
  const valueRef = useRef(value);
  const documentIdRef = useRef(documentId);
  const onChangeRef = useRef(onChange);
  const onCursorRef = useRef(onCursor);
  const wrapCompartment = useRef(new Compartment());
  valueRef.current = value;
  documentIdRef.current = documentId;
  onChangeRef.current = onChange;
  onCursorRef.current = onCursor;

  useImperativeHandle(ref, () => ({
    gotoLine: (line, column = 1) => {
      const view = viewRef.current;
      if (!view) return;
      const doc = view.state.doc;
      const safeLine = Math.max(1, Math.min(line, doc.lines));
      const info = doc.line(safeLine);
      const pos = Math.min(info.to, info.from + Math.max(0, column - 1));
      view.dispatch({
        selection: { anchor: pos },
        effects: EditorView.scrollIntoView(pos, { y: 'center' }),
      });
      view.focus();
    },
    focus: () => viewRef.current?.focus(),
    getView: () => viewRef.current,
  }));

  useEffect(() => {
    if (!hostRef.current) return;
    const view = new EditorView({
      state: EditorState.create({
        doc: valueRef.current,
        extensions: [
          basicSetup,
          mpl(),
          syntaxHighlighting(highlight),
          editorTheme,
          wrapCompartment.current.of(wordWrap ? EditorView.lineWrapping : []),
          lintGutter(),
          autocompletion({ override: [mplCompletions], activateOnTyping: true }),
          keymap.of([...defaultKeymap, ...historyKeymap, ...searchKeymap, indentWithTab]),
          EditorView.updateListener.of((update) => {
            if (update.docChanged) {
              const next = update.state.doc.toString();
              valueRef.current = next;
              onChangeRef.current(next);
            }
            if (update.selectionSet || update.docChanged) {
              const head = update.state.selection.main.head;
              const line = update.state.doc.lineAt(head);
              onCursorRef.current?.(line.number, head - line.from + 1);
            }
          }),
        ],
      }),
      parent: hostRef.current,
    });
    viewRef.current = view;
    return () => {
      view.destroy();
      viewRef.current = null;
    };
    // wordWrap is applied through the compartment below; the view is created once.
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  useEffect(() => {
    const view = viewRef.current;
    if (!view) return;
    const current = view.state.doc.toString();
    if (current === value) return;
    valueRef.current = value;
    view.dispatch({ changes: { from: 0, to: current.length, insert: value } });
  }, [value]);

  useEffect(() => {
    const view = viewRef.current;
    if (!view) return;
    const timer = window.setTimeout(() => {
      view.dispatch({
        selection: { anchor: 0 },
        effects: EditorView.scrollIntoView(0, { y: 'start' }),
      });
      view.scrollDOM.scrollTop = 0;
    }, 0);
    return () => window.clearTimeout(timer);
  }, [documentId]);

  useEffect(() => {
    const view = viewRef.current;
    if (!view) return;
    view.dispatch({
      effects: wrapCompartment.current.reconfigure(wordWrap ? EditorView.lineWrapping : []),
    });
  }, [wordWrap]);

  useEffect(() => {
    const view = viewRef.current;
    if (!view) return;
    const lineCount = view.state.doc.lines;
    const mapped: CmDiagnostic[] = diagnostics.map((diagnostic) => {
      const line = Math.max(1, Math.min(diagnostic.line || 1, lineCount));
      const info = view.state.doc.line(line);
      const start = Math.min(info.to, info.from + Math.max(0, (diagnostic.column || 1) - 1));
      const end = Math.min(
        info.to,
        Math.max(start + 1, start + Math.max(1, (diagnostic.endColumn || (diagnostic.column || 1) + 1) - (diagnostic.column || 1))),
      );
      const severity =
        diagnostic.severity === 'error'
          ? 'error'
          : diagnostic.severity === 'warning'
            ? 'warning'
            : diagnostic.severity === 'info'
              ? 'info'
              : 'hint';
      return {
        from: start,
        to: end,
        severity,
        message: `${diagnostic.code ? `[${diagnostic.code}] ` : ''}${diagnostic.message}`,
        source: 'mplc',
      };
    });
    view.dispatch(setDiagnostics(view.state, mapped));
  }, [diagnostics]);

  return <div ref={hostRef} className="editor-host h-full w-full overflow-hidden" />;
});
