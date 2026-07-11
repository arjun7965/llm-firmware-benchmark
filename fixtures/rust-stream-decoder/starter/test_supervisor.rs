use std::fs::File;
use std::io::Read;
use std::path::PathBuf;
use std::process::{Command, ExitCode};

const COMPLETION_ENVIRONMENT: &str = "FIXTURE_COMPLETION_TOKEN";
const COMPLETION_PREFIX: &str = "fixture-complete:";

fn completion_token() -> Result<String, String> {
    let mut bytes = [0_u8; 24];
    File::open("/dev/urandom")
        .and_then(|mut source| source.read_exact(&mut bytes))
        .map_err(|error| format!("create completion token: {error}"))?;
    Ok(bytes.iter().map(|byte| format!("{byte:02x}")).collect())
}

fn run() -> Result<(), String> {
    let token = completion_token()?;
    let current_executable = std::env::current_exe()
        .map_err(|error| format!("resolve test supervisor: {error}"))?;
    let test_binary: PathBuf = current_executable
        .parent()
        .ok_or_else(|| "test supervisor has no parent directory".to_owned())?
        .join("candidate-tests");
    let output = Command::new(test_binary)
        .args(["--test-threads=1", "--nocapture"])
        .env(COMPLETION_ENVIRONMENT, &token)
        .output()
        .map_err(|error| format!("run candidate tests: {error}"))?;

    print!("{}", String::from_utf8_lossy(&output.stdout));
    eprint!("{}", String::from_utf8_lossy(&output.stderr));
    if !output.status.success() {
        return Err(format!("candidate tests failed with {}", output.status));
    }

    let expected = format!("{COMPLETION_PREFIX}{token}");
    if !output.stdout.split(|byte| *byte == b'\n').any(|line| {
        line.strip_suffix(b"\r").unwrap_or(line) == expected.as_bytes()
    }) {
        return Err("candidate tests exited without their completion token".to_owned());
    }
    Ok(())
}

fn main() -> ExitCode {
    match run() {
        Ok(()) => ExitCode::SUCCESS,
        Err(error) => {
            eprintln!("{error}");
            ExitCode::FAILURE
        }
    }
}
