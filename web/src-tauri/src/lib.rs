use base64::Engine as _;
use serde::Serialize;
use serde_json::{json, Value};
use std::fs;
use std::io::Write;
use std::path::{Path, PathBuf};
use std::process::{Command, Output};
use std::time::{SystemTime, UNIX_EPOCH};
use tauri::{AppHandle, Manager};

#[derive(Serialize)]
#[serde(rename_all = "camelCase")]
struct Example {
    id: String,
    name: String,
    label: String,
    content: String,
    bytes: usize,
}

#[derive(Serialize)]
#[serde(rename_all = "camelCase")]
struct Artifact {
    name: String,
    bytes: u64,
}

#[derive(Serialize)]
#[serde(rename_all = "camelCase")]
struct CheckResult {
    ok: bool,
    diagnostics: Value,
    stdout: String,
    stderr: String,
}

#[derive(Serialize)]
#[serde(rename_all = "camelCase")]
struct BuildResult {
    ok: bool,
    audio_base64: String,
    audio_name: String,
    analysis: Value,
    report: Value,
    artifacts: Vec<Artifact>,
    diagnostics: Value,
    stdout: String,
    stderr: String,
    error: String,
}

#[derive(Serialize)]
#[serde(rename_all = "camelCase")]
struct Health {
    ok: bool,
    compiler: String,
    version: String,
}

fn repo_root() -> PathBuf {
    PathBuf::from(env!("CARGO_MANIFEST_DIR"))
        .join("..")
        .join("..")
        .canonicalize()
        .unwrap_or_else(|_| PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("..").join(".."))
}

fn resource_candidates(app: &AppHandle, relative: &str) -> Vec<PathBuf> {
    let mut candidates = Vec::new();
    if let Ok(resource_dir) = app.path().resource_dir() {
        candidates.push(resource_dir.join("resources").join(relative));
        candidates.push(resource_dir.join(relative));
    }
    candidates.push(repo_root().join(relative));
    candidates
}

fn first_existing(app: &AppHandle, relative: &str, is_file: bool) -> Option<PathBuf> {
    resource_candidates(app, relative)
        .into_iter()
        .find(|path| if is_file { path.is_file() } else { path.is_dir() })
}

fn compiler_path(app: &AppHandle) -> Result<PathBuf, String> {
    if let Ok(value) = std::env::var("MPL_STUDIO_COMPILER") {
        let path = PathBuf::from(value);
        if path.is_file() {
            return Ok(path);
        }
    }
    let candidates = [
        "resources/bin/mplc.exe",
        "bin/mplc.exe",
        "mplc.exe",
    ];
    for candidate in candidates {
        if let Some(path) = first_existing(app, candidate, true) {
            return Ok(path);
        }
    }
    let dev = repo_root().join("build").join("Release").join("mplc.exe");
    if dev.is_file() {
        return Ok(dev);
    }
    let debug = repo_root().join("build").join("Debug").join("mplc.exe");
    if debug.is_file() {
        return Ok(debug);
    }
    Err("mplc compiler was not found in bundled resources or build/Release".into())
}

fn stdlib_path(app: &AppHandle) -> Result<PathBuf, String> {
    if let Ok(value) = std::env::var("MPL_STUDIO_STDLIB") {
        let path = PathBuf::from(value);
        if path.is_dir() {
            return Ok(path);
        }
    }
    first_existing(app, "resources/stdlib", false)
        .or_else(|| first_existing(app, "stdlib", false))
        .or_else(|| {
            let path = repo_root().join("stdlib");
            path.is_dir().then_some(path)
        })
        .ok_or_else(|| "bundled MPL standard library was not found".into())
}

fn examples_path(app: &AppHandle) -> Result<PathBuf, String> {
    first_existing(app, "resources/examples", false)
        .or_else(|| first_existing(app, "examples", false))
        .or_else(|| {
            let path = repo_root().join("examples");
            path.is_dir().then_some(path)
        })
        .ok_or_else(|| "bundled MPL examples were not found".into())
}

fn unique_dir(prefix: &str) -> Result<PathBuf, String> {
    let nanos = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map_err(|error| error.to_string())?
        .as_nanos();
    let path = std::env::temp_dir().join(format!("{prefix}-{}-{nanos}", std::process::id()));
    fs::create_dir_all(&path).map_err(|error| error.to_string())?;
    Ok(path)
}

fn debug_log(message: &str) {
    let path = std::env::temp_dir().join("mpl-studio-command.log");
    if let Ok(mut file) = fs::OpenOptions::new().create(true).append(true).open(path) {
        let _ = writeln!(file, "{message}");
    }
}

fn run_compiler(app: &AppHandle, args: &[String], cwd: &Path) -> Result<Output, String> {
    let compiler = compiler_path(app)?;
    let stdlib = stdlib_path(app)?;
    let output = Command::new(&compiler)
        .args(args)
        .current_dir(cwd)
        .env("MPL_STDLIB_DIR", &stdlib)
        .output()
        .map_err(|error| {
            let message = format!(
                "failed to run {} (cwd={}, stdlib={}): {error}",
                compiler.display(),
                cwd.display(),
                stdlib.display()
            );
            debug_log(&message);
            message
        })?;
    debug_log(&format!(
        "compiler={} cwd={} code={:?}",
        compiler.display(),
        cwd.display(),
        output.status.code()
    ));
    Ok(output)
}

fn parse_diagnostics(output: &Output) -> Value {
    let text = String::from_utf8_lossy(&output.stdout);
    serde_json::from_str(text.trim()).unwrap_or_else(|_| json!([]))
}

fn sanitize_name(name: &str) -> String {
    let base = name
        .rsplit(['/', '\\'])
        .next()
        .unwrap_or("untitled")
        .trim_end_matches(".mpl");
    let cleaned: String = base
        .chars()
        .map(|ch| if ch.is_ascii_alphanumeric() || ch == '_' || ch == '-' || ch == '.' { ch } else { '-' })
        .collect();
    if cleaned.is_empty() { "untitled".into() } else { cleaned }
}

fn find_wav(dir: &Path) -> Option<PathBuf> {
    let entries = fs::read_dir(dir).ok()?;
    for entry in entries.flatten() {
        let path = entry.path();
        if path.is_dir() {
            if let Some(found) = find_wav(&path) {
                return Some(found);
            }
        } else if path.extension().and_then(|ext| ext.to_str()).map(|ext| ext.eq_ignore_ascii_case("wav")) == Some(true) {
            return Some(path);
        }
    }
    None
}

fn collect_artifacts(dir: &Path, output: &mut Vec<Artifact>) {
    let Ok(entries) = fs::read_dir(dir) else { return };
    for entry in entries.flatten() {
        let path = entry.path();
        if path.is_dir() {
            collect_artifacts(&path, output);
        } else if let Ok(metadata) = entry.metadata() {
            output.push(Artifact {
                name: entry.file_name().to_string_lossy().into_owned(),
                bytes: metadata.len(),
            });
        }
    }
    output.sort_by(|left, right| left.name.cmp(&right.name));
}

fn json_or_null(text: &[u8]) -> Value {
    serde_json::from_str(String::from_utf8_lossy(text).trim()).unwrap_or(Value::Null)
}

#[tauri::command]
fn list_examples(app: AppHandle) -> Result<Vec<Example>, String> {
    let dir = examples_path(&app)?;
    let mut examples = Vec::new();
    let entries = fs::read_dir(&dir).map_err(|error| error.to_string())?;
    for entry in entries.flatten() {
        let path = entry.path();
        if path.extension().and_then(|value| value.to_str()) != Some("mpl") {
            continue;
        }
        let content = fs::read_to_string(&path).map_err(|error| error.to_string())?;
        let name = entry.file_name().to_string_lossy().into_owned();
        examples.push(Example {
            id: format!("example:{name}"),
            label: name.trim_end_matches(".mpl").to_string(),
            name,
            bytes: content.len(),
            content,
        });
    }
    examples.sort_by(|left, right| {
        let preferred = [
            "math-rock-4min.mpl",
            "8bit-2min.mpl",
            "math-rock.mpl",
            "core.mpl",
            "production.mpl",
            "temporal.mpl",
            "audio.mpl",
            "reference.mpl",
            "suite.mpl",
        ];
        let left_rank = preferred.iter().position(|name| *name == left.name).unwrap_or(usize::MAX);
        let right_rank = preferred.iter().position(|name| *name == right.name).unwrap_or(usize::MAX);
        left_rank.cmp(&right_rank).then_with(|| left.name.cmp(&right.name))
    });
    Ok(examples)
}

#[tauri::command]
fn check_source(app: AppHandle, source: String, name: String) -> Result<CheckResult, String> {
    if source.trim().is_empty() {
        return Ok(CheckResult { ok: true, diagnostics: json!([]), stdout: String::new(), stderr: String::new() });
    }
    let dir = unique_dir("mpl-studio-check")?;
    let file = dir.join(format!("{}.mpl", sanitize_name(&name)));
    fs::write(&file, source).map_err(|error| error.to_string())?;
    let args = vec![
        "check".to_string(),
        file.to_string_lossy().into_owned(),
        "--json".to_string(),
    ];
    let output = run_compiler(&app, &args, &dir)?;
    let result = CheckResult {
        ok: output.status.success(),
        diagnostics: parse_diagnostics(&output),
        stdout: String::from_utf8_lossy(&output.stdout).into_owned(),
        stderr: String::from_utf8_lossy(&output.stderr).into_owned(),
    };
    let _ = fs::remove_dir_all(&dir);
    Ok(result)
}

#[tauri::command]
fn report_source(app: AppHandle, source: String, name: String) -> Result<Value, String> {
    let dir = unique_dir("mpl-studio-report")?;
    let file = dir.join(format!("{}.mpl", sanitize_name(&name)));
    fs::write(&file, source).map_err(|error| error.to_string())?;
    let output = run_compiler(
        &app,
        &["report".to_string(), file.to_string_lossy().into_owned()],
        &dir,
    )?;
    let report = json_or_null(&output.stdout);
    let _ = fs::remove_dir_all(&dir);
    Ok(report)
}

#[tauri::command]
fn build_source(app: AppHandle, source: String, name: String) -> Result<BuildResult, String> {
    if source.trim().is_empty() {
        return Ok(BuildResult {
            ok: false,
            audio_base64: String::new(),
            audio_name: String::new(),
            analysis: Value::Null,
            report: Value::Null,
            artifacts: Vec::new(),
            diagnostics: json!([]),
            stdout: String::new(),
            stderr: String::new(),
            error: "empty source".into(),
        });
    }
    let dir = unique_dir("mpl-studio-build")?;
    let file = dir.join(format!("{}.mpl", sanitize_name(&name)));
    fs::write(&file, source).map_err(|error| error.to_string())?;
    let out_dir = dir.join("out");
    fs::create_dir_all(&out_dir).map_err(|error| error.to_string())?;
    let output = run_compiler(
        &app,
        &[
            "build".to_string(),
            file.to_string_lossy().into_owned(),
            "-o".to_string(),
            out_dir.to_string_lossy().into_owned(),
            "--tail".to_string(),
            "0.75".to_string(),
        ],
        &dir,
    )?;
    let diagnostics = parse_diagnostics(&output);
    let wav = find_wav(&out_dir);
    if !output.status.success() || wav.is_none() {
        let result = BuildResult {
            ok: false,
            audio_base64: String::new(),
            audio_name: String::new(),
            analysis: Value::Null,
            report: Value::Null,
            artifacts: Vec::new(),
            diagnostics,
            stdout: String::from_utf8_lossy(&output.stdout).into_owned(),
            stderr: String::from_utf8_lossy(&output.stderr).into_owned(),
            error: "compiler build failed or produced no WAV; add export wav(\"...\") to the source".into(),
        };
        let _ = fs::remove_dir_all(&dir);
        return Ok(result);
    }
    let wav = wav.unwrap();
    let bytes = fs::read(&wav).map_err(|error| error.to_string())?;
    let audio_base64 = base64::engine::general_purpose::STANDARD.encode(&bytes);
    let audio_name = wav.file_name().and_then(|value| value.to_str()).unwrap_or("render.wav").to_string();
    let analysis = run_compiler(
        &app,
        &[
            "analyze".to_string(),
            wav.to_string_lossy().into_owned(),
            "--json".to_string(),
        ],
        &dir,
    )
    .ok()
    .map(|result| json_or_null(&result.stdout))
    .unwrap_or(Value::Null);
    let report = run_compiler(
        &app,
        &["report".to_string(), file.to_string_lossy().into_owned()],
        &dir,
    )
    .ok()
    .map(|result| json_or_null(&result.stdout))
    .unwrap_or(Value::Null);
    let mut artifacts = Vec::new();
    collect_artifacts(&out_dir, &mut artifacts);
    let result = BuildResult {
        ok: true,
        audio_base64,
        audio_name,
        analysis,
        report,
        artifacts,
        diagnostics,
        stdout: String::from_utf8_lossy(&output.stdout).into_owned(),
        stderr: String::from_utf8_lossy(&output.stderr).into_owned(),
        error: String::new(),
    };
    let _ = fs::remove_dir_all(&dir);
    Ok(result)
}

#[tauri::command]
fn health(app: AppHandle) -> Health {
    match compiler_path(&app) {
        Ok(path) => Health {
            ok: true,
            compiler: path.display().to_string(),
            version: "0.5.0-beta".into(),
        },
        Err(error) => Health {
            ok: false,
            compiler: error,
            version: "0.5.0-beta".into(),
        },
    }
}

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    tauri::Builder::default()
        .invoke_handler(tauri::generate_handler![
            list_examples,
            check_source,
            report_source,
            build_source,
            health
        ])
        .run(tauri::generate_context!())
        .expect("error while running MPL Studio");
}
