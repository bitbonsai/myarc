#![forbid(unsafe_code)]

//! `myarcctl` runs myarc control commands from a shell (`commands`, `run`) or
//! serves them to MCP clients over stdio (`mcp`). `run` and `mcp` send requests
//! to the running browser through `myarc_control::ipc::Client`, or with
//! `--local` to an in-memory `myarc_control::ControlService`. `commands` and
//! the MCP tool list read `myarc_control::registry`. `serve-local` is a
//! development stub that hosts an in-memory service behind an `ipc::Server`.

#[cfg(not(unix))]
compile_error!("myarcctl needs a Unix platform for its control IPC");

mod mcp;

use std::io::{self, ErrorKind, Write};
use std::path::PathBuf;
use std::process::ExitCode;
use std::sync::atomic::AtomicBool;
use std::time::{SystemTime, UNIX_EPOCH};

use myarc_control::ipc::{self, Client, ConnectError, Server};
use myarc_control::{ControlService, Outcome, PROTOCOL_VERSION, Request, Response, registry};
use serde_json::{Value, json};

pub const VERSION: &str = env!("CARGO_PKG_VERSION");
/// Longest params argument `run` accepts, the same bound as one MCP message.
const MAX_PARAMS_BYTES: usize = mcp::MAX_LINE_BYTES;

/// Exit code when `run` can't reach myArc.
const EXIT_NOT_CONNECTED: u8 = 3;

const HELP: &str = "\
myarcctl: run myarc browser control commands

Usage:
  myarcctl commands [--json]
      List the command registry: ID, area, whether it runs, summary.
  myarcctl run <command> [params-json] [--json] [--local]
      Run one command in the running myArc, for example
      myarcctl run space.rename '{\"space_id\":1,\"name\":\"Work\"}'
      Prints one line, or the full response as JSON with --json.
  myarcctl mcp [--local]
      Serve the commands as MCP tools over stdio.
  myarcctl --version
  myarcctl --help

run and mcp connect to the running myArc through the control endpoint in
$MYARC_CONTROL_DIR, or else ~/Library/Application Support/myarc/control.
With --local they use a fresh in-memory workspace instead, so changes are
gone when the command exits. An mcp session keeps one workspace until the
client disconnects.

Development stub:
  myarcctl serve-local [--dir <dir>]
      Stand in for the browser: serve an in-memory workspace at the control
      endpoint in <dir> or the default directory, so run and mcp can connect
      without myArc. Stop it with Ctrl-C.

Exit codes: 0 success, 1 the command returned an error, 2 usage error,
3 myArc couldn't be reached.
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
        local: bool,
    },
    Mcp {
        local: bool,
    },
    ServeLocal {
        dir: Option<PathBuf>,
    },
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
            local,
        }) => run(command, params, json, local),
        Ok(Cli::Mcp { local }) => {
            let backend = if local {
                mcp::Backend::Local(ControlService::new())
            } else {
                mcp::Backend::remote()
            };
            match mcp::serve(io::stdin().lock(), io::stdout().lock(), backend) {
                Ok(()) => ExitCode::SUCCESS,
                Err(error) => {
                    mcp::diagnostic(&format!("stopped: {error}"));
                    ExitCode::FAILURE
                }
            }
        }
        Ok(Cli::ServeLocal { dir }) => serve_local(dir),
        Err(message) => usage_error(&message),
    }
}

fn parse(args: &[String]) -> Result<Cli, String> {
    let mut json = false;
    let mut local = false;
    let mut dir = None;
    let mut help = false;
    let mut version = false;
    let mut positional = Vec::new();
    let mut args = args.iter();
    while let Some(arg) = args.next() {
        match arg.as_str() {
            "--json" => json = true,
            "--local" => local = true,
            "--dir" => match args.next() {
                Some(path) => dir = Some(PathBuf::from(path)),
                None => return Err(String::from("--dir needs a directory")),
            },
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
        return if positional.is_empty() && !json && !local && dir.is_none() {
            Ok(Cli::Version)
        } else {
            Err(String::from("--version takes no other arguments"))
        };
    }
    let subcommand = positional.first().copied();
    if local && !matches!(subcommand, Some("run" | "mcp")) {
        return Err(String::from("--local only applies to run and mcp"));
    }
    if dir.is_some() && subcommand != Some("serve-local") {
        return Err(String::from("--dir only applies to serve-local"));
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
            local,
        }),
        ["run", command, params] => Ok(Cli::Run {
            command: (*command).to_owned(),
            params: parse_params(params)?,
            json,
            local,
        }),
        ["run", ..] => Err(String::from(
            "run takes a command ID and one params argument; quote the JSON",
        )),
        ["mcp"] if !json => Ok(Cli::Mcp { local }),
        ["mcp", ..] => Err(String::from("mcp takes no arguments besides --local")),
        ["serve-local"] if !json => Ok(Cli::ServeLocal { dir }),
        ["serve-local", ..] => Err(String::from("serve-local takes no arguments besides --dir")),
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

fn run(command: String, params: Value, json: bool, local: bool) -> ExitCode {
    let request = Request {
        id: request_id(),
        command,
        params,
    };
    let response = if local {
        ControlService::new().handle(request)
    } else {
        match connect() {
            Ok(mut client) => call(&mut client, request).unwrap_or_else(|response| response),
            Err(message) => {
                write_line(&mut io::stderr(), &format!("myarcctl: {message}"));
                return ExitCode::from(EXIT_NOT_CONNECTED);
            }
        }
    };
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

/// Connects to the running myArc through the default control directory.
fn connect() -> Result<Client, String> {
    let Some(dir) = ipc::default_dir() else {
        return Err(format!(
            "can't find the control endpoint; set {} or HOME",
            ipc::DIR_ENV
        ));
    };
    Client::connect_as(&dir, "myarcctl", VERSION).map_err(|error| match error {
        ConnectError::NotRunning { .. } | ConnectError::Stale { .. } => {
            format!("{error}, or pass --local to use an in-memory workspace")
        }
        other => other.to_string(),
    })
}

/// Sends one request. `Err` carries a response describing a lost connection,
/// after which the client is closed.
fn call(client: &mut Client, request: Request) -> Result<Response, Response> {
    let id = request.id.clone();
    let command = request.command.clone();
    client.call(request).map_err(|error| {
        let outcome = if error.kind() == ErrorKind::InvalidInput {
            Outcome::Invalid {
                reason: error.to_string(),
            }
        } else {
            Outcome::OutcomeUnknown {
                reason: format!(
                    "lost the connection to myArc ({error}); check the result before retrying"
                ),
                data: None,
            }
        };
        Response {
            id,
            command,
            outcome,
        }
    })
}

fn serve_local(dir: Option<PathBuf>) -> ExitCode {
    let Some(dir) = dir.or_else(ipc::default_dir) else {
        return usage_error(&format!(
            "serve-local needs --dir, {} or HOME",
            ipc::DIR_ENV
        ));
    };
    let server = match Server::bind(&dir) {
        Ok(server) => server,
        Err(error) => {
            write_line(
                &mut io::stderr(),
                &format!(
                    "myarcctl serve-local: can't serve at {}: {error}",
                    dir.display()
                ),
            );
            return ExitCode::FAILURE;
        }
    };
    // This line tells scripts the endpoint is ready.
    print(&format!(
        "myarcctl serve-local: serving an in-memory workspace at {}",
        dir.display()
    ));
    let mut service = ControlService::new();
    let stop = AtomicBool::new(false);
    match server.serve(|request| service.handle(request), &stop) {
        Ok(()) => ExitCode::SUCCESS,
        Err(error) => {
            write_line(
                &mut io::stderr(),
                &format!("myarcctl serve-local: stopped: {error}"),
            );
            ExitCode::FAILURE
        }
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
        Outcome::OutcomeUnknown { reason, data } => match data {
            Some(data) => format!("{command}: outcome unknown: {reason} {data}"),
            None => format!("{command}: outcome unknown: {reason}"),
        },
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
