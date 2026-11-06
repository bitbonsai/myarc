#![forbid(unsafe_code)]

//! `myarcctl` runs myarc control commands from a shell (`commands`, `run`) or
//! serves them to MCP clients over stdio (`mcp`). Both paths call
//! `myarc_control::ControlService::handle` and read `myarc_control::registry`.

mod mcp;

use std::io::{self, Write};
use std::process::ExitCode;
use std::time::{SystemTime, UNIX_EPOCH};

use myarc_control::{ControlService, Outcome, PROTOCOL_VERSION, Request, Response, registry};
use serde_json::{Value, json};

pub const VERSION: &str = env!("CARGO_PKG_VERSION");
/// Longest params argument `run` accepts, the same bound as one MCP message.
const MAX_PARAMS_BYTES: usize = mcp::MAX_LINE_BYTES;

const HELP: &str = "\
myarcctl: run myarc browser control commands

Usage:
  myarcctl commands [--json]
      List the command registry: ID, area, whether it runs, summary.
  myarcctl run <command> [params-json] [--json]
      Run one command, for example
      myarcctl run space.rename '{\"space_id\":1,\"name\":\"Work\"}'
      Prints one line, or the full response as JSON with --json.
  myarcctl mcp
      Serve the commands as MCP tools over stdio.
  myarcctl --version
  myarcctl --help

This build has no browser attached. Each run starts from a fresh in-memory
workspace, so changes are gone when the command exits. An mcp session keeps
one workspace until the client disconnects.

Exit codes: 0 success, 1 the command returned an error, 2 usage error.
";

enum Cli {
    Help,
    Version,
    Commands {
        json: bool,
    },
    Run {
        command: String,
        params: Value,
        json: bool,
    },
    Mcp,
}

fn main() -> ExitCode {
    let mut args = Vec::new();
    for arg in std::env::args_os().skip(1) {
        match arg.into_string() {
            Ok(arg) => args.push(arg),
            Err(_) => return usage_error("arguments must be valid UTF-8"),
        }
    }
    match parse(&args) {
        Ok(Cli::Help) => print(HELP.trim_end()),
        Ok(Cli::Version) => print(&format!(
            "myarcctl {VERSION} (control protocol {PROTOCOL_VERSION})"
        )),
        Ok(Cli::Commands { json }) => print(&commands(json)),
        Ok(Cli::Run {
            command,
            params,
            json,
        }) => run(command, params, json),
        Ok(Cli::Mcp) => match mcp::serve(io::stdin().lock(), io::stdout().lock()) {
            Ok(()) => ExitCode::SUCCESS,
            Err(error) => {
                mcp::diagnostic(&format!("stopped: {error}"));
                ExitCode::FAILURE
            }
        },
        Err(message) => usage_error(&message),
    }
}

fn parse(args: &[String]) -> Result<Cli, String> {
    let mut json = false;
    let mut help = false;
    let mut version = false;
    let mut positional = Vec::new();
    for arg in args {
        match arg.as_str() {
            "--json" => json = true,
            "-h" | "--help" => help = true,
            "-V" | "--version" => version = true,
            flag if flag.starts_with("--") => {
                return Err(format!("unknown option {}", mcp::bounded(flag)));
            }
            other => positional.push(other),
        }
    }
    if help {
        return Ok(Cli::Help);
    }
    if version {
        return if positional.is_empty() && !json {
            Ok(Cli::Version)
        } else {
            Err(String::from("--version takes no other arguments"))
        };
    }
    match positional.as_slice() {
        [] => Err(String::from("missing subcommand")),
        ["commands"] => Ok(Cli::Commands { json }),
        ["commands", ..] => Err(String::from("commands takes no arguments")),
        ["run"] => Err(String::from(
            "run needs a command ID; myarcctl commands lists them",
        )),
        ["run", command] => Ok(Cli::Run {
            command: (*command).to_owned(),
            params: Value::Null,
            json,
        }),
        ["run", command, params] => Ok(Cli::Run {
            command: (*command).to_owned(),
            params: parse_params(params)?,
            json,
        }),
        ["run", ..] => Err(String::from(
            "run takes a command ID and one params argument; quote the JSON",
        )),
        ["mcp"] if !json => Ok(Cli::Mcp),
        ["mcp", ..] => Err(String::from("mcp takes no arguments")),
        [other, ..] => Err(format!("unknown subcommand {:?}", mcp::bounded(other))),
    }
}

fn parse_params(text: &str) -> Result<Value, String> {
    if text.len() > MAX_PARAMS_BYTES {
        return Err(format!("params exceed {MAX_PARAMS_BYTES} bytes"));
    }
    serde_json::from_str(text).map_err(|error| format!("params are not valid JSON: {error}"))
}

fn commands(json: bool) -> String {
    if json {
        let list: Vec<Value> = registry()
            .iter()
            .map(|spec| {
                json!({
                    "id": spec.id,
                    "area": spec.area,
                    "implemented": spec.implemented,
                    "summary": spec.summary,
                })
            })
            .collect();
        return Value::Array(list).to_string();
    }
    let width = registry()
        .iter()
        .map(|spec| spec.id.len())
        .max()
        .unwrap_or(0);
    let mut lines = vec![format!(
        "{:<width$}  {:<12}  {:<11}  SUMMARY",
        "ID", "AREA", "IMPLEMENTED"
    )];
    for spec in registry() {
        let area = json!(spec.area);
        lines.push(format!(
            "{:<width$}  {:<12}  {:<11}  {}",
            spec.id,
            area.as_str().unwrap_or_default(),
            if spec.implemented { "yes" } else { "no" },
            spec.summary,
        ));
    }
    lines.join("\n")
}

fn run(command: String, params: Value, json: bool) -> ExitCode {
    let mut service = ControlService::new();
    let response = service.handle(Request {
        id: request_id(),
        command,
        params,
    });
    let success = matches!(response.outcome, Outcome::Success { .. });
    let printed = if json {
        match serde_json::to_string(&response) {
            Ok(text) => write_line(&mut io::stdout(), &text),
            Err(error) => write_line(&mut io::stderr(), &format!("myarcctl: {error}")),
        }
    } else if success {
        write_line(&mut io::stdout(), &readable(&response))
    } else {
        write_line(&mut io::stderr(), &readable(&response))
    };
    if success && printed {
        ExitCode::SUCCESS
    } else {
        ExitCode::FAILURE
    }
}

fn request_id() -> String {
    let millis = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map(|elapsed| elapsed.as_millis())
        .unwrap_or_default();
    format!("cli-{}-{millis}", std::process::id())
}

fn readable(response: &Response) -> String {
    let command = &response.command;
    match &response.outcome {
        Outcome::Success { data } => format!("{command}: ok {data}"),
        Outcome::Denied { reason } => format!("{command}: denied: {reason}"),
        Outcome::Unsupported { reason, .. } => format!("{command}: unsupported: {reason}"),
        Outcome::Stale { reason } => format!("{command}: stale: {reason}"),
        Outcome::Cancelled => format!("{command}: cancelled"),
        Outcome::Timeout => format!("{command}: timeout"),
        Outcome::OutcomeUnknown { reason } => format!("{command}: outcome unknown: {reason}"),
        Outcome::Invalid { reason } => format!("{command}: invalid: {reason}"),
    }
}

fn print(text: &str) -> ExitCode {
    if write_line(&mut io::stdout(), text) {
        ExitCode::SUCCESS
    } else {
        ExitCode::FAILURE
    }
}

fn usage_error(message: &str) -> ExitCode {
    write_line(
        &mut io::stderr(),
        &format!("myarcctl: {message}\n\n{}", HELP.trim_end()),
    );
    ExitCode::from(2)
}

/// Writes one line without panicking when the reader has gone away.
fn write_line(out: &mut impl Write, text: &str) -> bool {
    writeln!(out, "{text}").and_then(|()| out.flush()).is_ok()
}
