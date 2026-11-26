import express from 'express';
import path from 'node:path';
import fs from 'node:fs';
import fsp from 'node:fs/promises';
import os from 'node:os';
import crypto from 'node:crypto';
import { fileURLToPath } from 'node:url';
import { spawn } from 'node:child_process';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const webRoot = path.resolve(__dirname, '..');
const repoRoot = path.resolve(webRoot, '..');
const runtimeRoot = path.join(webRoot, '.runtime');
const port = Number(process.env.PORT || 4317);

await fsp.mkdir(runtimeRoot, { recursive: true });

function findCompiler() {
  const candidates = [
    process.env.MPLC,
    path.join(repoRoot, 'build', 'Release', 'mplc.exe'),
    path.join(repoRoot, 'build', 'Debug', 'mplc.exe'),
    path.join(repoRoot, 'build', 'Release', 'mplc'),
    path.join(repoRoot, 'build', 'Debug', 'mplc'),
  ].filter(Boolean);
  return candidates.find((candidate) => fs.existsSync(candidate)) || null;
}

const compilerPath = findCompiler();
if (!compilerPath) {
  console.error('MPL compiler not found. Build mplc or set MPLC=/path/to/mplc.');
  process.exit(1);
}

function runCompiler(args, timeoutMs = 120000) {
  return new Promise((resolve) => {
    const child = spawn(compilerPath, args, {
      cwd: repoRoot,
      windowsHide: true,
      env: { ...process.env, MPL_STUDIO: '1' },
    });
    let stdout = '';
    let stderr = '';
    let finished = false;
    const timer = setTimeout(() => {
      if (!finished) {
        child.kill();
        finished = true;
        resolve({ code: -1, stdout, stderr: `${stderr}\ncompiler timeout`, timedOut: true });
      }
    }, timeoutMs);
    child.stdout.on('data', (chunk) => { stdout += chunk.toString('utf8'); });
    child.stderr.on('data', (chunk) => { stderr += chunk.toString('utf8'); });
    child.on('error', (error) => {
      if (finished) return;
      finished = true;
      clearTimeout(timer);
      resolve({ code: -1, stdout, stderr: `${stderr}\n${error.message}`, spawnError: true });
    });
    child.on('close', (code) => {
      if (finished) return;
      finished = true;
      clearTimeout(timer);
      resolve({ code: code ?? -1, stdout, stderr });
    });
  });
}

function parseJson(text) {
  const trimmed = text.trim();
  if (!trimmed) return null;
  try {
    return JSON.parse(trimmed);
  } catch {
    const start = trimmed.indexOf('[');
    const end = trimmed.lastIndexOf(']');
    if (start >= 0 && end > start) {
      try { return JSON.parse(trimmed.slice(start, end + 1)); } catch { /* ignore */ }
    }
    const objStart = trimmed.indexOf('{');
    const objEnd = trimmed.lastIndexOf('}');
    if (objStart >= 0 && objEnd > objStart) {
      try { return JSON.parse(trimmed.slice(objStart, objEnd + 1)); } catch { /* ignore */ }
    }
    return null;
  }
}

function diagnosticsFrom(result) {
  const parsed = parseJson(result.stdout);
  if (Array.isArray(parsed)) return parsed;
  if (parsed && Array.isArray(parsed.diagnostics)) return parsed.diagnostics;
  return [];
}

async function writeSource(source, label = 'untitled') {
  const id = crypto.randomUUID().slice(0, 12);
  const dir = await fsp.mkdtemp(path.join(runtimeRoot, 'session-'));
  const safe = String(label).replace(/[^a-zA-Z0-9_.-]/g, '-').replace(/\.mpl$/i, '') || 'untitled';
  const file = path.join(dir, `${safe}.mpl`);
  await fsp.writeFile(file, source, 'utf8');
  return { id, dir, file };
}

function findWav(dir) {
  const entries = fs.readdirSync(dir, { withFileTypes: true });
  for (const entry of entries) {
    const full = path.join(dir, entry.name);
    if (entry.isDirectory()) {
      const nested = findWav(full);
      if (nested) return nested;
    } else if (entry.name.toLowerCase().endsWith('.wav')) {
      return full;
    }
  }
  return null;
}

function listExamples() {
  const dir = path.join(repoRoot, 'examples');
  const preferred = ['math-rock-4min.mpl', '8bit-2min.mpl', 'math-rock.mpl', 'core.mpl', 'production.mpl', 'temporal.mpl', 'audio.mpl', 'reference.mpl', 'suite.mpl'];
  const all = fs.readdirSync(dir).filter((name) => name.endsWith('.mpl'));
  all.sort((a, b) => {
    const ai = preferred.indexOf(a);
    const bi = preferred.indexOf(b);
    if (ai >= 0 && bi >= 0) return ai - bi;
    if (ai >= 0) return -1;
    if (bi >= 0) return 1;
    return a.localeCompare(b);
  });
  return all.map((name) => {
    const content = fs.readFileSync(path.join(dir, name), 'utf8');
    return {
      id: `example:${name}`,
      name,
      label: name.replace(/\.mpl$/, ''),
      content,
      bytes: Buffer.byteLength(content, 'utf8'),
      group: 'Examples',
    };
  });
}

const app = express();
app.use(express.json({ limit: '2mb' }));

app.get('/api/health', (_req, res) => {
  res.json({ ok: true, compiler: compilerPath, version: '0.5.0-beta', port });
});

app.get('/api/examples', async (_req, res) => {
  try {
    res.json({ ok: true, examples: listExamples() });
  } catch (error) {
    res.status(500).json({ ok: false, error: String(error) });
  }
});

app.post('/api/check', async (req, res) => {
  const source = typeof req.body?.source === 'string' ? req.body.source : '';
  if (!source.trim()) return res.json({ ok: true, diagnostics: [] });
  try {
    const { file } = await writeSource(source, req.body?.name || 'untitled');
    const result = await runCompiler(['check', file, '--json'], 45000);
    const diagnostics = diagnosticsFrom(result);
    res.json({
      ok: result.code === 0,
      diagnostics,
      stdout: result.stdout,
      stderr: result.stderr,
    });
  } catch (error) {
    res.status(500).json({ ok: false, error: String(error) });
  }
});

app.post('/api/report', async (req, res) => {
  const source = typeof req.body?.source === 'string' ? req.body.source : '';
  try {
    const { file } = await writeSource(source, req.body?.name || 'report');
    const result = await runCompiler(['report', file], 45000);
    res.json({ ok: result.code === 0, report: parseJson(result.stdout), stdout: result.stdout, stderr: result.stderr });
  } catch (error) {
    res.status(500).json({ ok: false, error: String(error) });
  }
});

app.post('/api/build', async (req, res) => {
  const source = typeof req.body?.source === 'string' ? req.body.source : '';
  if (!source.trim()) return res.status(400).json({ ok: false, error: 'empty source' });
  try {
    const { id, dir, file } = await writeSource(source, req.body?.name || 'build');
    const outDir = path.join(dir, 'out');
    await fsp.mkdir(outDir, { recursive: true });
    const result = await runCompiler(['build', file, '-o', outDir, '--tail', '0.75'], 120000);
    const wav = findWav(outDir);
    if (result.code !== 0 || !wav) {
      return res.json({
        ok: false,
        diagnostics: diagnosticsFrom(result),
        stdout: result.stdout,
        stderr: result.stderr,
        error: 'compiler build failed or produced no WAV; add export wav("...") to the source',
      });
    }
    const audioId = crypto.randomUUID().replace(/-/g, '').slice(0, 12);
    const audioDir = path.join(runtimeRoot, 'audio', audioId);
    await fsp.mkdir(audioDir, { recursive: true });
    const audioName = path.basename(wav);
    const audioPath = path.join(audioDir, audioName);
    await fsp.copyFile(wav, audioPath);
    const analysis = await runCompiler(['analyze', wav, '--json'], 45000);
    const report = await runCompiler(['report', file], 45000);
    const artifacts = fs.readdirSync(outDir).map((name) => ({
      name,
      bytes: fs.statSync(path.join(outDir, name)).size,
    }));
    res.json({
      ok: true,
      audioUrl: `/api/audio/${audioId}/${encodeURIComponent(audioName)}`,
      audioName,
      analysis: parseJson(analysis.stdout),
      report: parseJson(report.stdout),
      artifacts,
      diagnostics: diagnosticsFrom(result),
      stdout: result.stdout,
      stderr: result.stderr,
    });
  } catch (error) {
    res.status(500).json({ ok: false, error: String(error) });
  }
});

app.get('/api/audio/:id/:file', (req, res) => {
  const id = String(req.params.id).replace(/[^a-zA-Z0-9]/g, '');
  const file = path.basename(String(req.params.file));
  const audioPath = path.join(runtimeRoot, 'audio', id, file);
  if (!audioPath.startsWith(path.join(runtimeRoot, 'audio'))) return res.status(400).end();
  if (!fs.existsSync(audioPath)) return res.status(404).end();
  res.type('audio/wav');
  res.sendFile(audioPath);
});

const dist = path.join(webRoot, 'dist');
if (fs.existsSync(dist)) {
  app.use(express.static(dist));
  app.get('*', (req, res, next) => {
    if (req.path.startsWith('/api/')) return next();
    res.sendFile(path.join(dist, 'index.html'));
  });
}

app.listen(port, '127.0.0.1', () => {
  console.log(`MPL Studio API on http://127.0.0.1:${port}`);
  console.log(`compiler: ${compilerPath}`);
});
