export type Severity = 'error' | 'warning' | 'info' | 'hint';

export interface Diagnostic {
  code: string;
  severity: Severity | string;
  file?: string;
  line: number;
  column: number;
  endLine?: number;
  endColumn?: number;
  offset?: number;
  length?: number;
  message: string;
  expected?: string;
  actual?: string;
  related?: string;
  suggestions?: string[];
}

export interface ExampleFile {
  id: string;
  name: string;
  label: string;
  content: string;
  bytes: number;
  group?: string;
}

export interface SourceFile {
  id: string;
  name: string;
  content: string;
  dirty: boolean;
  kind: 'example' | 'untitled';
}

export interface AudioAnalysis {
  sampleRate?: number;
  channels?: number;
  frames?: number;
  samplePeakDbfs?: number;
  rmsDbfs?: number;
  crestFactorDb?: number;
  truePeakDbtp?: number;
  integratedLoudnessLufs?: number;
  maxShortTermLoudnessLufs?: number;
  clipped?: boolean;
  nonFinite?: boolean;
}

export interface CompilerReport {
  compilerVersion?: string;
  languageVersion?: string;
  stdlibVersion?: string;
  benchmarkVersion?: string;
  observationVersion?: string;
  phases?: Record<string, unknown>;
  diagnostics?: Diagnostic[];
  modules?: Array<{ name: string; path: string; contentHash?: string; resolvedHash?: string }>;
  counts?: {
    musicObjects?: number;
    tracks?: number;
    clips?: number;
    notes?: number;
  };
  [key: string]: unknown;
}

export interface BuildArtifact {
  name: string;
  bytes: number;
}

export interface BuildResult {
  audioUrl: string;
  audioName: string;
  analysis: AudioAnalysis | null;
  report: CompilerReport | null;
  artifacts: BuildArtifact[];
  diagnostics: Diagnostic[];
  stdout?: string;
  stderr?: string;
}

export interface CheckResult {
  ok: boolean;
  diagnostics: Diagnostic[];
  stdout?: string;
  stderr?: string;
}
