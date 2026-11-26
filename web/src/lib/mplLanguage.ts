import { LanguageSupport, StreamLanguage } from '@codemirror/language';

const KEYWORDS = new Set([
  'project', 'asset', 'let', 'fn', 'return', 'context', 'track', 'section', 'clip',
  'arrangement', 'export', 'module', 'import', 'if', 'else', 'for', 'in', 'true',
  'false', 'bars', 'beats', 'whole', 'meter', 'key', 'tempo', 'role', 'asset',
  'play', 'span', 'at', 'for', 'vel', 'gain', 'pan', 'send', 'insert', 'processor',
  'bus', 'master', 'automation', 'curve', 'keyframes', 'linear', 'exponential',
]);

const TYPES = new Set([
  'Pitch', 'Duration', 'Position', 'Interval', 'Key', 'Chord', 'Harmony', 'Voicing',
  'Rhythm', 'Fragment', 'Motif', 'Phrase', 'Number', 'Int', 'Bool', 'Boolean',
  'String', 'Track', 'Section', 'Processor', 'Bus', 'Meter', 'Scalar', 'Audio',
  'Note', 'Curve',
]);

const BUILTINS = new Set([
  'note', 'rest', 'sequence', 'parallel', 'repeat', 'transpose', 'diatonicTranspose',
  'stretch', 'reverse', 'rotate', 'syncopate', 'harmonize', 'random', 'ratio',
  'chord', 'chordAt', 'chordAtBar', 'chordNotes', 'voicing', 'voicingNotes',
  'voicingStatus', 'voicingMovement', 'voicingCrossings', 'voicingComplete',
  'voiceLead', 'commonTones', 'voiceDistance', 'pitchRange', 'fragmentRange',
  'pitchClass', 'interval', 'parallelPerfects', 'zip', 'cycle', 'overlay',
  'subdivide', 'accent', 'realign', 'quantize', 'period', 'commonPeriod',
  'metricAccent', 'metricWeight', 'onsetSet', 'intersect', 'union', 'difference',
  'complement', 'accentFollow', 'retrigger', 'length', 'range', 'round', 'tie',
  'true', 'false',
]);

function token(stream: any): string | null {
  if (stream.eatSpace()) return null;
  if (stream.match('//')) {
    stream.skipToEnd();
    return 'comment';
  }
  if (stream.match('/*')) {
    while (!stream.eol()) {
      if (stream.match('*/')) break;
      stream.next();
    }
    return 'comment';
  }
  if (stream.match(/^"(?:[^"\\]|\\.)*"/)) return 'string';
  if (stream.match(/^[A-G](?:#|b)?-?\d+/)) return 'atom';
  if (stream.match(/^\d+\/\d+/)) return 'number';
  if (stream.match(/^\d+(?:\.\d+)?/)) {
    stream.eatWhile(/[a-zA-Z]+/);
    return 'number';
  }
  if (stream.match(/^[a-zA-Z_][a-zA-Z0-9_]*/)) {
    const word = stream.current();
    if (KEYWORDS.has(word)) return 'keyword';
    if (TYPES.has(word)) return 'typeName';
    if (BUILTINS.has(word)) return 'function';
    return 'variableName';
  }
  if (stream.match(/^[{}()[\]]/)) return 'bracket';
  if (stream.match(/^[=+\-*/%<>!&|:;,.]/)) return 'operator';
  stream.next();
  return null;
}

export const mplLanguage = StreamLanguage.define({
  name: 'mpl',
  token,
});

export function mpl(): LanguageSupport {
  return new LanguageSupport(mplLanguage);
}
