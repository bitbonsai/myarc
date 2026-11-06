//! MCP server over stdio. Messages are newline-delimited JSON-RPC 2.0: one per
//! line on stdin, replies one per line on stdout, diagnostics on stderr. Tools
//! come from `registry()` and calls go through one `ControlService` that lives
//! for the whole session.

use std::io::{self, BufRead, Read, Write};

use myarc_control::{ControlService, Outcome, Request, registry};
use serde_json::{Map, Value, json};

use crate::VERSION;

/// Longest message line accepted; longer lines get an error reply.
pub const MAX_LINE_BYTES: usize = 1024 * 1024;
/// Revision sent when the client asks for one this server does not list.
const LATEST_PROTOCOL_VERSION: &str = "2025-06-18";
const SUPPORTED_PROTOCOL_VERSIONS: &[&str] = &["2025-06-18", "2025-03-26", "2024-11-05"];
/// Longest piece of client input quoted in an error message.
const MAX_QUOTED_BYTES: usize = 64;

const PARSE_ERROR: i64 = -32700;
const INVALID_REQUEST: i64 = -32600;
const METHOD_NOT_FOUND: i64 = -32601;
const INVALID_PARAMS: i64 = -32602;
const INTERNAL_ERROR: i64 = -32603;

type RpcError = (i64, String);

/// Reads messages until EOF and writes one reply per request.
pub fn serve(mut input: impl BufRead, mut output: impl Write) -> io::Result<()> {
    let mut server = Server::default();
    let mut line = Vec::new();
    loop {
        let reply = match read_line(&mut input, &mut line)? {
            Line::Eof => return Ok(()),
            Line::TooLong => {
                diagnostic(&format!("rejected a message over {MAX_LINE_BYTES} bytes"));
                Some(error_reply(
                    Value::Null,
                    INVALID_REQUEST,
                    format!("Message exceeds {MAX_LINE_BYTES} bytes; send a smaller one"),
                ))
            }
            Line::Read => server.handle_line(&line),
        };
        if let Some(reply) = reply {
            serde_json::to_writer(&mut output, &reply)?;
            output.write_all(b"\n")?;
            output.flush()?;
        }
    }
}

/// Writes to stderr, ignoring failures so a closed stderr can't stop the
/// server.
pub fn diagnostic(message: &str) {
    let _ = writeln!(io::stderr(), "myarcctl mcp: {message}");
}

/// Prefix of untrusted text short enough to quote in an error.
pub fn bounded(text: &str) -> &str {
    let mut end = text.len().min(MAX_QUOTED_BYTES);
    while !text.is_char_boundary(end) {
        end -= 1;
    }
    &text[..end]
}

enum Line {
    Read,
    TooLong,
    Eof,
}

/// Reads one line into `buf` without its line ending, holding at most
/// `MAX_LINE_BYTES` plus one in memory. The rest of a longer line is skipped.
fn read_line(input: &mut impl BufRead, buf: &mut Vec<u8>) -> io::Result<Line> {
    buf.clear();
    let limit = MAX_LINE_BYTES as u64 + 1;
    if input.by_ref().take(limit).read_until(b'\n', buf)? == 0 {
        return Ok(Line::Eof);
    }
    if buf.last() == Some(&b'\n') {
        buf.pop();
        if buf.last() == Some(&b'\r') {
            buf.pop();
        }
        return Ok(Line::Read);
    }
    if buf.len() <= MAX_LINE_BYTES {
        // Last line, ended by EOF instead of a newline.
        return Ok(Line::Read);
    }
    loop {
        buf.clear();
        let read = input.by_ref().take(64 * 1024).read_until(b'\n', buf)?;
        if read == 0 || buf.last() == Some(&b'\n') {
            break;
        }
    }
    buf.clear();
    Ok(Line::TooLong)
}

#[derive(Default)]
struct Server {
    service: ControlService,
    calls: u64,
}

impl Server {
    fn handle_line(&mut self, line: &[u8]) -> Option<Value> {
        let Ok(text) = std::str::from_utf8(line) else {
            return Some(error_reply(
                Value::Null,
                PARSE_ERROR,
                String::from("Parse error: message is not valid UTF-8"),
            ));
        };
        if text.trim().is_empty() {
            return None;
        }
        match serde_json::from_str(text) {
            Ok(message) => self.handle_message(message),
            Err(error) => Some(error_reply(
                Value::Null,
                PARSE_ERROR,
                format!("Parse error: {error}"),
            )),
        }
    }

    fn handle_message(&mut self, message: Value) -> Option<Value> {
        let Value::Object(mut message) = message else {
            return Some(invalid_request(
                "Expected one JSON-RPC 2.0 message object; batches are not supported",
            ));
        };
        let id = message.remove("id");
        if message.get("jsonrpc").and_then(Value::as_str) != Some("2.0") {
            return Some(invalid_request("Message must set \"jsonrpc\": \"2.0\""));
        }
        let method = match message.get("method") {
            Some(Value::String(method)) => method.clone(),
            Some(_) => return Some(invalid_request("Method must be a string")),
            // A reply to a server request. This server sends none, so drop it.
            None if message.contains_key("result") || message.contains_key("error") => {
                return None;
            }
            None => return Some(invalid_request("Message has no method")),
        };
        let id = match id {
            // Notifications, including notifications/initialized, get no reply.
            None => return None,
            Some(id @ (Value::String(_) | Value::Number(_))) => id,
            Some(_) => return Some(invalid_request("Request id must be a string or number")),
        };
        Some(match self.call(&method, message.remove("params")) {
            Ok(result) => json!({ "jsonrpc": "2.0", "id": id, "result": result }),
            Err((code, reason)) => error_reply(id, code, reason),
        })
    }

    fn call(&mut self, method: &str, params: Option<Value>) -> Result<Value, RpcError> {
        let params = object_params(params)?;
        match method {
            "initialize" => initialize(&params),
            "ping" => Ok(json!({})),
            "tools/list" => Ok(json!({ "tools": tools() })),
            "tools/call" => self.call_tool(params),
            other => Err((
                METHOD_NOT_FOUND,
                format!("Method not found: {:?}", bounded(other)),
            )),
        }
    }

    fn call_tool(&mut self, mut params: Map<String, Value>) -> Result<Value, RpcError> {
        let Some(Value::String(name)) = params.get("name") else {
            return Err(invalid_params("tools/call needs a tool name string"));
        };
        let Some(spec) = registry().iter().find(|spec| tool_name(spec.id) == *name) else {
            return Err(invalid_params(&format!(
                "Unknown tool {:?}; tools/list shows the tool names",
                bounded(name)
            )));
        };
        let arguments = match params.remove("arguments") {
            None | Some(Value::Null) => Value::Null,
            Some(arguments @ Value::Object(_)) => arguments,
            Some(_) => return Err(invalid_params("Tool arguments must be an object")),
        };
        self.calls += 1;
        let response = self.service.handle(Request {
            id: format!("mcp-{}", self.calls),
            command: spec.id.to_owned(),
            params: arguments,
        });
        let is_error = !matches!(response.outcome, Outcome::Success { .. });
        let structured = serde_json::to_value(&response).map_err(|error| {
            (
                INTERNAL_ERROR,
                format!("Could not encode response: {error}"),
            )
        })?;
        Ok(json!({
            "content": [{ "type": "text", "text": structured.to_string() }],
            "structuredContent": structured,
            "isError": is_error,
        }))
    }
}

fn initialize(params: &Map<String, Value>) -> Result<Value, RpcError> {
    let Some(requested) = params.get("protocolVersion").and_then(Value::as_str) else {
        return Err(invalid_params("initialize needs a protocolVersion string"));
    };
    let version = SUPPORTED_PROTOCOL_VERSIONS
        .iter()
        .find(|version| **version == requested)
        .copied()
        .unwrap_or(LATEST_PROTOCOL_VERSION);
    Ok(json!({
        "protocolVersion": version,
        "capabilities": { "tools": {} },
        "serverInfo": { "name": "myarcctl", "version": VERSION },
    }))
}

fn tools() -> Vec<Value> {
    registry()
        .iter()
        .map(|spec| {
            let mut description = spec.summary.to_owned();
            if !spec.implemented {
                description.push_str(" (not yet implemented)");
            }
            json!({
                "name": tool_name(spec.id),
                "description": description,
                "inputSchema": spec.params,
            })
        })
        .collect()
}

/// MCP tool names avoid dots, so `space.create` becomes `space_create`.
fn tool_name(command: &str) -> String {
    command.replace('.', "_")
}

fn object_params(params: Option<Value>) -> Result<Map<String, Value>, RpcError> {
    match params {
        None | Some(Value::Null) => Ok(Map::new()),
        Some(Value::Object(map)) => Ok(map),
        Some(_) => Err(invalid_params("Params must be an object")),
    }
}

fn invalid_params(reason: &str) -> RpcError {
    (INVALID_PARAMS, reason.to_owned())
}

fn invalid_request(reason: &str) -> Value {
    error_reply(Value::Null, INVALID_REQUEST, reason.to_owned())
}

fn error_reply(id: Value, code: i64, message: String) -> Value {
    json!({
        "jsonrpc": "2.0",
        "id": id,
        "error": { "code": code, "message": message },
    })
}
