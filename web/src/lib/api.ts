import { invoke } from '@tauri-apps/api/core';
import type { CheckResult, Diagnostic, ExampleFile } from '../types';

export const isTauri = () =>
  typeof window !== 'undefined' && '__TAURI_INTERNALS__' in window;

async function jsonFetch<T>(url: string, init?: RequestInit): Promise<T> {
  const response = await fetch(url, init);
  if (!response.ok) {
    const text = await response.text();
    throw new Error(text || `${response.status} ${response.statusText}`);
  }
  return response.json() as Promise<T>;
}

function base64ToBlobUrl(base64: string, mime = 'audio/wav'): string {
  const binary = atob(base64);
  const bytes = new Uint8Array(binary.length);
  for (let index = 0; index < binary.length; index += 1) {
    bytes[index] = binary.charCodeAt(index);
  }
  return URL.createObjectURL(new Blob([bytes], { type: mime }));
}

export async function fetchExamples(): Promise<ExampleFile[]> {
  if (isTauri()) {
    const examples = await invoke<ExampleFile[]>('list_examples');
    return examples;
  }
  const data = await jsonFetch<{ ok: boolean; examples: ExampleFile[] }>('/api/examples');
  return data.examples;
}

export async function checkSource(source: string, name: string): Promise<CheckResult> {
  if (isTauri()) {
    const result = await invoke<CheckResult>('check_source', { source, name });
    return { ...result, diagnostics: result.diagnostics ?? [] };
  }
  const data = await jsonFetch<CheckResult>('/api/check', {
    method: 'POST',
    headers: { 'content-type': 'application/json' },
    body: JSON.stringify({ source, name }),
  });
  return { ...data, diagnostics: data.diagnostics ?? [] };
}

type WebBuild =
  | {
      ok: true;
      audioUrl: string;
      audioName: string;
      analysis: Record<string, unknown> | null;
      report: Record<string, unknown> | null;
      artifacts: Array<{ name: string; bytes: number }>;
      diagnostics: Diagnostic[];
      stdout?: string;
      stderr?: string;
    }
  | { ok: false; diagnostics?: Diagnostic[]; stdout?: string; stderr?: string; error?: string };

type TauriBuild = {
  ok: boolean;
  audioBase64: string;
  audioName: string;
  analysis: Record<string, unknown> | null;
  report: Record<string, unknown> | null;
  artifacts: Array<{ name: string; bytes: number }>;
  diagnostics: Diagnostic[];
  stdout?: string;
  stderr?: string;
  error?: string;
};

export async function buildSource(source: string, name: string): Promise<WebBuild> {
  if (isTauri()) {
    const result = await invoke<TauriBuild>('build_source', { source, name });
    if (!result.ok) {
      return {
        ok: false,
        diagnostics: result.diagnostics ?? [],
        stdout: result.stdout,
        stderr: result.stderr,
        error: result.error,
      };
    }
    return {
      ok: true,
      audioUrl: base64ToBlobUrl(result.audioBase64),
      audioName: result.audioName,
      analysis: result.analysis,
      report: result.report,
      artifacts: result.artifacts ?? [],
      diagnostics: result.diagnostics ?? [],
      stdout: result.stdout,
      stderr: result.stderr,
    };
  }
  return jsonFetch<WebBuild>('/api/build', {
    method: 'POST',
    headers: { 'content-type': 'application/json' },
    body: JSON.stringify({ source, name }),
  });
}

export async function fetchReport(source: string, name: string) {
  if (isTauri()) {
    const report = await invoke<Record<string, unknown> | null>('report_source', { source, name });
    return { ok: report != null, report };
  }
  return jsonFetch<{ ok: boolean; report: Record<string, unknown> | null; stdout?: string; stderr?: string }>(
    '/api/report',
    {
      method: 'POST',
      headers: { 'content-type': 'application/json' },
      body: JSON.stringify({ source, name }),
    },
  );
}

export async function fetchHealth(): Promise<{ ok: boolean; compiler: string; version: string }> {
  if (isTauri()) {
    return invoke('health');
  }
  return jsonFetch('/api/health');
}
