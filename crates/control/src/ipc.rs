//! Authenticated same-user local IPC between control clients such as
//! `myarcctl` and a running browser's control service.
//!
//! The browser hosts a [`Server`] in a private directory. Binding writes
//! `control.json` there with the Unix socket path, a fresh random token, the
//! server's pid and its protocol version. A [`Client`] reads that file, checks
//! that the directory, the file and the socket belong to the current user and
//! that group and others can't reach them, then connects.
//!
//! Every message is a frame: a 4-byte big-endian length and that many bytes of
//! UTF-8 JSON, at most [`MAX_FRAME_BYTES`]. The client's first frame is a hello
//! carrying the token. The server answers with a welcome or an error and then
//! closes on error. After a welcome the client sends [`Request`] frames and gets
//! one [`Response`] frame back for each.
//!
//! The token never appears in logs, errors or `Debug` output.

use std::fmt;
use std::fs::{self, DirBuilder, File, OpenOptions, Permissions};
use std::io::{self, ErrorKind, Read, Write};
use std::os::fd::OwnedFd;
use std::os::unix::fs::{DirBuilderExt, FileTypeExt, MetadataExt, OpenOptionsExt, PermissionsExt};
use std::os::unix::net::{UnixListener, UnixStream};
use std::panic::{self, AssertUnwindSafe};
use std::path::{Path, PathBuf};
use std::sync::atomic::{AtomicBool, AtomicU64, AtomicUsize, Ordering};
use std::sync::{Mutex, OnceLock, PoisonError};
use std::thread;
use std::time::{Duration, Instant};

use serde::{Deserialize, Serialize};

use crate::{Outcome, PROTOCOL_VERSION, Request, Response, bounded};

/// Environment variable that overrides the default control directory.
pub const DIR_ENV: &str = "MYARC_CONTROL_DIR";
/// Endpoint file the server writes in the control directory.
pub const ENDPOINT_FILE: &str = "control.json";
/// Unix socket the server listens on in the control directory.
pub const SOCKET_FILE: &str = "control.sock";
/// Largest frame body either side accepts.
pub const MAX_FRAME_BYTES: usize = 1024 * 1024;
/// A connection with no incoming bytes for this long is closed.
pub const IDLE_TIMEOUT: Duration = Duration::from_secs(60);
/// Most connections a server serves at once; more get a busy error.
pub const MAX_CONNECTIONS: usize = 16;

/// Time a new connection has to send its hello.
const HELLO_TIMEOUT: Duration = Duration::from_secs(5);
/// How often blocked server reads and accepts check the stop flag.
const POLL_INTERVAL: Duration = Duration::from_millis(50);
const WRITE_TIMEOUT: Duration = Duration::from_secs(5);
/// How long a client waits for a reply. Above the longest bounded wait a
/// command can ask for (`page.wait` allows 30 seconds).
const CLIENT_READ_TIMEOUT: Duration = Duration::from_secs(60);
const MAX_ENDPOINT_FILE_BYTES: u64 = 64 * 1024;
const MAX_CLIENT_FIELD_BYTES: usize = 64;
const TOKEN_BYTES: usize = 32;

/// The control directory clients look in: `$MYARC_CONTROL_DIR`, or else
/// `~/Library/Application Support/myarc/control`. `None` when neither that
/// variable nor `HOME` is set.
pub fn default_dir() -> Option<PathBuf> {
    if let Some(dir) = std::env::var_os(DIR_ENV).filter(|dir| !dir.is_empty()) {
        return Some(PathBuf::from(dir));
    }
    std::env::var_os("HOME")
        .filter(|home| !home.is_empty())
        .map(|home| PathBuf::from(home).join("Library/Application Support/myarc/control"))
}

/// Session secret. Its `Debug` output is redacted and it has no `Display`.
#[derive(Clone, Serialize, Deserialize)]
#[serde(transparent)]
struct Token(String);

impl Token {
    fn generate() -> io::Result<Self> {
        let mut bytes = [0u8; TOKEN_BYTES];
        File::open("/dev/urandom")?.read_exact(&mut bytes)?;
        Ok(Self(
            bytes.iter().map(|byte| format!("{byte:02x}")).collect(),
        ))
    }

    /// Compares in time that depends only on the lengths.
    fn matches(&self, other: &Token) -> bool {
        let (a, b) = (self.0.as_bytes(), other.0.as_bytes());
        if a.len() != b.len() {
            return false;
        }
        let diff = a.iter().zip(b).fold(0u8, |acc, (x, y)| acc | (x ^ y));
        std::hint::black_box(diff) == 0
    }
}

impl fmt::Debug for Token {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str("Token(<redacted>)")
    }
}

/// Contents of `control.json`.
#[derive(Debug, Serialize, Deserialize)]
struct EndpointFile {
    socket: PathBuf,
    token: Token,
    pid: u32,
    protocol_version: String,
}

#[derive(Serialize, Deserialize)]
struct ClientInfo {
    name: String,
    version: String,
}

#[derive(Serialize, Deserialize)]
#[serde(tag = "type", rename_all = "snake_case")]
enum ClientMessage {
    Hello {
        token: Token,
        client: ClientInfo,
        protocol_version: String,
    },
}

#[derive(Serialize, Deserialize)]
#[serde(tag = "type", rename_all = "snake_case")]
enum ServerMessage {
    Welcome {
        session_id: String,
    },
    Error {
        code: String,
        message: String,
        /// Set on `protocol_mismatch` so the client can name both versions.
        #[serde(default, skip_serializing_if = "Option::is_none")]
        protocol_version: Option<String>,
    },
}

const BAD_HELLO: &str = "bad_hello";
const TOKEN_REJECTED: &str = "token_rejected";
const PROTOCOL_MISMATCH: &str = "protocol_mismatch";
const FRAME_TOO_LARGE: &str = "frame_too_large";
const BUSY: &str = "busy";

/// Control endpoint hosted by the browser. Bind it, then call [`Server::serve`]
/// on a thread the browser owns. Dropping it removes the socket and
/// `control.json`.
#[derive(Debug)]
pub struct Server {
    listener: UnixListener,
    dir: PathBuf,
    socket: PathBuf,
    endpoint: PathBuf,
    token: Token,
    idle_timeout: Duration,
}

impl Server {
    /// Creates `dir` with mode 0700 if needed, replaces a stale endpoint left
    /// by a server that is gone, listens on `dir/control.sock` (mode 0600) and
    /// writes `dir/control.json` (mode 0600) with a fresh token. Fails with
    /// `AddrInUse` when another server is live in `dir`, and with
    /// `PermissionDenied` when `dir` isn't a directory owned by this user.
    pub fn bind(dir: &Path) -> io::Result<Server> {
        DirBuilder::new().recursive(true).mode(0o700).create(dir)?;
        let meta = fs::symlink_metadata(dir)?;
        if !meta.is_dir() || meta.uid() != current_uid()? {
            return Err(io::Error::new(
                ErrorKind::PermissionDenied,
                format!(
                    "{} must be a directory owned by you to hold the control endpoint",
                    dir.display()
                ),
            ));
        }
        if meta.mode() & 0o777 != 0o700 {
            fs::set_permissions(dir, Permissions::from_mode(0o700))?;
        }
        let socket = dir.join(SOCKET_FILE);
        let endpoint = dir.join(ENDPOINT_FILE);
        clear_stale(&socket, &endpoint)?;
        let token = Token::generate()?;
        let listener = UnixListener::bind(&socket)?;
        let server = Server {
            listener,
            dir: dir.to_owned(),
            socket,
            endpoint,
            token,
            idle_timeout: IDLE_TIMEOUT,
        };
        let published = fs::set_permissions(&server.socket, Permissions::from_mode(0o600))
            .and_then(|()| server.listener.set_nonblocking(true))
            .and_then(|()| server.write_endpoint());
        if let Err(error) = published {
            let _ = fs::remove_file(&server.socket);
            return Err(error);
        }
        Ok(server)
    }

    /// The control directory this server publishes in.
    pub fn dir(&self) -> &Path {
        &self.dir
    }

    /// Closes connections that send nothing for `timeout`. Defaults to
    /// [`IDLE_TIMEOUT`].
    pub fn set_idle_timeout(&mut self, timeout: Duration) {
        self.idle_timeout = timeout;
    }

    /// Accepts clients until `stop` becomes true, then waits for open
    /// connections to notice and removes the endpoint. Each connection runs on
    /// its own thread, and `handler` runs one request at a time across all of
    /// them, so the browser can post each request to its UI thread and wait.
    /// A panic in `handler` answers that request with `outcome_unknown`.
    pub fn serve<F>(self, handler: F, stop: &AtomicBool) -> io::Result<()>
    where
        F: FnMut(Request) -> Response + Send,
    {
        let shared = Shared {
            handler: Mutex::new(handler),
            token: &self.token,
            stop,
            idle_timeout: self.idle_timeout,
            active: AtomicUsize::new(0),
            sessions: AtomicU64::new(0),
        };
        thread::scope(|scope| {
            while !stop.load(Ordering::Acquire) {
                match self.listener.accept() {
                    Ok((stream, _)) => {
                        if shared.active.fetch_add(1, Ordering::AcqRel) >= MAX_CONNECTIONS {
                            shared.active.fetch_sub(1, Ordering::AcqRel);
                            let _ = stream.set_nonblocking(false);
                            let _ = stream.set_write_timeout(Some(WRITE_TIMEOUT));
                            reply_error(
                                &mut &stream,
                                BUSY,
                                "myArc is serving too many control connections; try again shortly",
                            );
                            continue;
                        }
                        let shared = &shared;
                        let spawned = thread::Builder::new()
                            .name(String::from("myarc-control-ipc"))
                            .spawn_scoped(scope, move || {
                                let _slot = Slot(&shared.active);
                                shared.connection(stream);
                            });
                        if spawned.is_err() {
                            // The closure, and with it the slot guard, never
                            // ran; the stream was dropped with it.
                            shared.active.fetch_sub(1, Ordering::AcqRel);
                        }
                    }
                    Err(error) if error.kind() == ErrorKind::Interrupted => {}
                    Err(_) => thread::sleep(POLL_INTERVAL),
                }
            }
        });
        Ok(())
    }

    fn write_endpoint(&self) -> io::Result<()> {
        let file = EndpointFile {
            socket: self.socket.clone(),
            token: self.token.clone(),
            pid: std::process::id(),
            protocol_version: PROTOCOL_VERSION.to_owned(),
        };
        let bytes = serde_json::to_vec(&file).map_err(io::Error::other)?;
        let temporary = self.dir.join(format!("{ENDPOINT_FILE}.tmp"));
        let _ = fs::remove_file(&temporary);
        let written = (|| -> io::Result<()> {
            let mut out = OpenOptions::new()
                .write(true)
                .create_new(true)
                .mode(0o600)
                .open(&temporary)?;
            out.write_all(&bytes)?;
            out.sync_all()?;
            fs::set_permissions(&temporary, Permissions::from_mode(0o600))?;
            fs::rename(&temporary, &self.endpoint)
        })();
        if written.is_err() {
            let _ = fs::remove_file(&temporary);
        }
        written
    }
}

impl Drop for Server {
    fn drop(&mut self) {
        // Remove only an endpoint this server wrote, never a newer one.
        let ours = read_endpoint(&self.endpoint)
            .is_ok_and(|file| file.token.matches(&self.token) && file.socket == self.socket);
        if ours {
            let _ = fs::remove_file(&self.endpoint);
            let _ = fs::remove_file(&self.socket);
        }
    }
}

/// Removes a socket nothing listens on, and the endpoint file with it.
fn clear_stale(socket: &Path, endpoint: &Path) -> io::Result<()> {
    if fs::symlink_metadata(socket).is_ok() {
        if UnixStream::connect(socket).is_ok() {
            return Err(io::Error::new(
                ErrorKind::AddrInUse,
                format!(
                    "another myArc is already serving control at {}",
                    socket.display()
                ),
            ));
        }
        fs::remove_file(socket)?;
    }
    match fs::remove_file(endpoint) {
        Err(error) if error.kind() != ErrorKind::NotFound => Err(error),
        _ => Ok(()),
    }
}

/// Releases a connection slot when its thread ends, even by panic.
struct Slot<'a>(&'a AtomicUsize);

impl Drop for Slot<'_> {
    fn drop(&mut self) {
        self.0.fetch_sub(1, Ordering::AcqRel);
    }
}

struct Shared<'a, F> {
    handler: Mutex<F>,
    token: &'a Token,
    stop: &'a AtomicBool,
    idle_timeout: Duration,
    active: AtomicUsize,
    sessions: AtomicU64,
}

impl<F: FnMut(Request) -> Response> Shared<'_, F> {
    fn connection(&self, stream: UnixStream) {
        // Accepted sockets inherit the listener's non-blocking mode on macOS.
        let ready = stream
            .set_nonblocking(false)
            .and_then(|()| stream.set_read_timeout(Some(POLL_INTERVAL)))
            .and_then(|()| stream.set_write_timeout(Some(WRITE_TIMEOUT)));
        if ready.is_err() {
            return;
        }
        let now = Instant::now();
        let mut reader = PollingReader {
            stream: &stream,
            stop: self.stop,
            idle_timeout: self.idle_timeout,
            last_progress: now,
            deadline: Some(now + HELLO_TIMEOUT),
        };
        let mut writer = &stream;
        let hello = match read_frame(&mut reader) {
            Ok(Frame::Message(bytes)) => bytes,
            Ok(Frame::TooLarge(len)) => {
                reply_error(&mut writer, FRAME_TOO_LARGE, &too_large(len));
                return;
            }
            Ok(Frame::Closed) | Err(_) => return,
        };
        if let Err((code, message, version)) = self.check_hello(&hello) {
            let reply = ServerMessage::Error {
                code: code.to_owned(),
                message,
                protocol_version: version,
            };
            let _ = write_json(&mut writer, &reply);
            return;
        }
        let session = self.sessions.fetch_add(1, Ordering::AcqRel) + 1;
        let welcome = ServerMessage::Welcome {
            session_id: session.to_string(),
        };
        if write_json(&mut writer, &welcome).is_err() {
            return;
        }
        reader.deadline = None;
        loop {
            let bytes = match read_frame(&mut reader) {
                Ok(Frame::Message(bytes)) => bytes,
                Ok(Frame::TooLarge(len)) => {
                    reply_error(&mut writer, FRAME_TOO_LARGE, &too_large(len));
                    return;
                }
                Ok(Frame::Closed) | Err(_) => return,
            };
            let response = match serde_json::from_slice::<Request>(&bytes) {
                Ok(request) => self.dispatch(request),
                Err(error) => Response {
                    id: String::new(),
                    command: String::new(),
                    outcome: Outcome::Invalid {
                        reason: format!(
                            "Frame is not a control request: {}",
                            bounded(&error.to_string())
                        ),
                    },
                },
            };
            if write_json(&mut writer, &response).is_err() {
                return;
            }
        }
    }

    fn check_hello(&self, bytes: &[u8]) -> Result<(), (&'static str, String, Option<String>)> {
        let Ok(ClientMessage::Hello {
            token,
            client,
            protocol_version,
        }) = serde_json::from_slice(bytes)
        else {
            return Err((
                BAD_HELLO,
                String::from(
                    "The first frame must be a hello with token, client and protocol_version",
                ),
                None,
            ));
        };
        if !self.token.matches(&token) {
            return Err((
                TOKEN_REJECTED,
                String::from("Token rejected; read control.json again"),
                None,
            ));
        }
        if client.name.len() > MAX_CLIENT_FIELD_BYTES
            || client.version.len() > MAX_CLIENT_FIELD_BYTES
        {
            return Err((
                BAD_HELLO,
                format!("Client name and version must be at most {MAX_CLIENT_FIELD_BYTES} bytes"),
                None,
            ));
        }
        if protocol_version != PROTOCOL_VERSION {
            return Err((
                PROTOCOL_MISMATCH,
                format!(
                    "This browser speaks control protocol {PROTOCOL_VERSION}, not {}",
                    bounded(&protocol_version)
                ),
                Some(PROTOCOL_VERSION.to_owned()),
            ));
        }
        Ok(())
    }

    fn dispatch(&self, request: Request) -> Response {
        let id = bounded(&request.id).to_owned();
        let command = bounded(&request.command).to_owned();
        let mut handler = self.handler.lock().unwrap_or_else(PoisonError::into_inner);
        panic::catch_unwind(AssertUnwindSafe(|| (*handler)(request))).unwrap_or_else(|_| Response {
            id,
            command,
            outcome: Outcome::OutcomeUnknown {
                reason: String::from(
                    "myArc failed while handling this request; check the result before retrying",
                ),
            },
        })
    }
}

/// Reads from a stream whose read timeout is [`POLL_INTERVAL`], retrying
/// timeouts until the server stops, the connection idles past its limit or the
/// hello deadline passes.
struct PollingReader<'a> {
    stream: &'a UnixStream,
    stop: &'a AtomicBool,
    idle_timeout: Duration,
    last_progress: Instant,
    deadline: Option<Instant>,
}

impl Read for PollingReader<'_> {
    fn read(&mut self, buf: &mut [u8]) -> io::Result<usize> {
        loop {
            let mut stream = self.stream;
            match stream.read(buf) {
                Ok(read) => {
                    self.last_progress = Instant::now();
                    return Ok(read);
                }
                Err(error)
                    if matches!(
                        error.kind(),
                        ErrorKind::WouldBlock | ErrorKind::TimedOut | ErrorKind::Interrupted
                    ) =>
                {
                    if self.stop.load(Ordering::Acquire) {
                        return Err(io::Error::new(
                            ErrorKind::ConnectionAborted,
                            "the control server is stopping",
                        ));
                    }
                    let now = Instant::now();
                    if self.deadline.is_some_and(|deadline| now >= deadline)
                        || now.duration_since(self.last_progress) >= self.idle_timeout
                    {
                        return Err(io::Error::new(
                            ErrorKind::TimedOut,
                            "the control connection was idle too long",
                        ));
                    }
                }
                Err(error) => return Err(error),
            }
        }
    }
}

enum Frame {
    Message(Vec<u8>),
    /// The header announced a body over [`MAX_FRAME_BYTES`]; it was not read.
    TooLarge(usize),
    /// The peer closed the connection between frames.
    Closed,
}

fn read_frame(reader: &mut impl Read) -> io::Result<Frame> {
    let mut header = [0u8; 4];
    let mut filled = 0;
    while filled < header.len() {
        match reader.read(&mut header[filled..]) {
            Ok(0) if filled == 0 => return Ok(Frame::Closed),
            Ok(0) => {
                return Err(io::Error::new(
                    ErrorKind::UnexpectedEof,
                    "connection closed inside a frame header",
                ));
            }
            Ok(read) => filled += read,
            Err(error) if error.kind() == ErrorKind::Interrupted => {}
            Err(error) => return Err(error),
        }
    }
    let len = u32::from_be_bytes(header) as usize;
    if len > MAX_FRAME_BYTES {
        return Ok(Frame::TooLarge(len));
    }
    let mut body = vec![0; len];
    reader.read_exact(&mut body).map_err(|error| {
        if error.kind() == ErrorKind::UnexpectedEof {
            io::Error::new(
                ErrorKind::UnexpectedEof,
                format!("connection closed inside a {len}-byte frame"),
            )
        } else {
            error
        }
    })?;
    Ok(Frame::Message(body))
}

fn write_frame(writer: &mut impl Write, body: &[u8]) -> io::Result<()> {
    let len = u32::try_from(body.len())
        .ok()
        .filter(|&len| len as usize <= MAX_FRAME_BYTES)
        .ok_or_else(|| {
            io::Error::new(
                ErrorKind::InvalidInput,
                format!(
                    "message is {} bytes, over the {MAX_FRAME_BYTES}-byte frame limit",
                    body.len()
                ),
            )
        })?;
    let mut frame = Vec::with_capacity(body.len() + 4);
    frame.extend_from_slice(&len.to_be_bytes());
    frame.extend_from_slice(body);
    writer.write_all(&frame)?;
    writer.flush()
}

/// Writes one JSON frame. A reply too large for a frame becomes an error
/// frame so the client isn't left waiting.
fn write_json(writer: &mut impl Write, message: &impl Serialize) -> io::Result<()> {
    let body = serde_json::to_vec(message).map_err(io::Error::other)?;
    if body.len() > MAX_FRAME_BYTES {
        let reply = ServerMessage::Error {
            code: FRAME_TOO_LARGE.to_owned(),
            message: too_large(body.len()),
            protocol_version: None,
        };
        let body = serde_json::to_vec(&reply).map_err(io::Error::other)?;
        return write_frame(writer, &body);
    }
    write_frame(writer, &body)
}

fn reply_error(writer: &mut impl Write, code: &str, message: &str) {
    let reply = ServerMessage::Error {
        code: code.to_owned(),
        message: message.to_owned(),
        protocol_version: None,
    };
    let _ = write_json(writer, &reply);
}

fn too_large(len: usize) -> String {
    format!("Frame of {len} bytes exceeds the {MAX_FRAME_BYTES}-byte limit")
}

/// The effective uid of this process, read from a fresh socket's owner so no
/// libc call is needed.
fn current_uid() -> io::Result<u32> {
    static UID: OnceLock<u32> = OnceLock::new();
    if let Some(&uid) = UID.get() {
        return Ok(uid);
    }
    let (socket, _peer) = UnixStream::pair()?;
    let uid = File::from(OwnedFd::from(socket)).metadata()?.uid();
    Ok(*UID.get_or_init(|| uid))
}

#[derive(Clone, Copy)]
enum Kind {
    Dir,
    File,
    Socket,
}

enum PathProblem {
    Missing,
    Unsafe(String),
    Io(io::Error),
}

/// Checks that `path` is a `kind` (not a symlink), owned by this user and
/// closed to group and others.
fn check_private(path: &Path, kind: Kind) -> Result<(), PathProblem> {
    let meta = match fs::symlink_metadata(path) {
        Ok(meta) => meta,
        Err(error) if error.kind() == ErrorKind::NotFound => return Err(PathProblem::Missing),
        Err(error) => return Err(PathProblem::Io(error)),
    };
    let file_type = meta.file_type();
    let (right_kind, name) = match kind {
        Kind::Dir => (file_type.is_dir(), "directory"),
        Kind::File => (file_type.is_file(), "regular file"),
        Kind::Socket => (file_type.is_socket(), "socket"),
    };
    if !right_kind {
        return Err(PathProblem::Unsafe(format!("is not a {name}")));
    }
    let uid = current_uid().map_err(PathProblem::Io)?;
    if meta.uid() != uid {
        return Err(PathProblem::Unsafe(format!(
            "is owned by uid {}, not by you (uid {uid})",
            meta.uid()
        )));
    }
    let mode = meta.mode() & 0o777;
    if mode & 0o077 != 0 {
        return Err(PathProblem::Unsafe(format!(
            "has mode {mode:03o}, which lets group or others in"
        )));
    }
    Ok(())
}

fn read_endpoint(path: &Path) -> io::Result<EndpointFile> {
    let mut bytes = Vec::new();
    File::open(path)?
        .take(MAX_ENDPOINT_FILE_BYTES + 1)
        .read_to_end(&mut bytes)?;
    if bytes.len() as u64 > MAX_ENDPOINT_FILE_BYTES {
        return Err(io::Error::new(
            ErrorKind::InvalidData,
            "control.json is larger than an endpoint file can be",
        ));
    }
    // The parse error is dropped: it could quote the file's contents.
    serde_json::from_slice(&bytes).map_err(|_| {
        io::Error::new(
            ErrorKind::InvalidData,
            "control.json is not a valid endpoint file",
        )
    })
}

/// Why [`Client::connect`] failed. The messages say what to do next.
#[derive(Debug)]
pub enum ConnectError {
    /// No control directory or no `control.json` in it.
    NotRunning {
        endpoint: PathBuf,
    },
    /// `control.json` names a socket nothing listens on.
    Stale {
        endpoint: PathBuf,
        pid: u32,
    },
    /// The directory, file or socket is someone else's, the wrong kind of
    /// file, or open to group or others.
    Permission {
        path: PathBuf,
        detail: String,
    },
    /// The server didn't accept the token from `control.json`.
    TokenRejected {
        endpoint: PathBuf,
    },
    ProtocolMismatch {
        server: String,
        client: String,
    },
    /// Any other I/O or handshake failure.
    Failed {
        path: PathBuf,
        detail: String,
    },
}

impl fmt::Display for ConnectError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::NotRunning { endpoint } => write!(
                f,
                "myArc isn't running (no control endpoint at {}); start myArc and try again",
                endpoint.display()
            ),
            Self::Stale { endpoint, pid } => write!(
                f,
                "endpoint is stale: {} was left by myArc process {pid}, which no longer accepts connections; start myArc again to replace it",
                endpoint.display()
            ),
            Self::Permission { path, detail } => write!(
                f,
                "permission/ownership problem: {} {detail}; the control directory must be yours with mode 700, and control.json and the socket yours with mode 600",
                path.display()
            ),
            Self::TokenRejected { endpoint } => write!(
                f,
                "token rejected: myArc didn't accept the token in {}; it may have restarted, so try again",
                endpoint.display()
            ),
            Self::ProtocolMismatch { server, client } => write!(
                f,
                "protocol version mismatch: myArc speaks control protocol {server} and this client speaks {client}; use a client from the same myArc build"
            ),
            Self::Failed { path, detail } => write!(
                f,
                "could not connect to myArc at {}: {detail}",
                path.display()
            ),
        }
    }
}

impl std::error::Error for ConnectError {}

/// One authenticated connection to a running browser's control service.
#[derive(Debug)]
pub struct Client {
    stream: UnixStream,
    session_id: String,
}

impl Client {
    /// Connects through `dir/control.json`, introducing itself as
    /// `myarc-control`.
    pub fn connect(dir: &Path) -> Result<Client, ConnectError> {
        Self::connect_as(dir, "myarc-control", env!("CARGO_PKG_VERSION"))
    }

    /// Connects through `dir/control.json` with a client name and version of
    /// at most 64 bytes each, which the browser can show to the user.
    pub fn connect_as(dir: &Path, name: &str, version: &str) -> Result<Client, ConnectError> {
        let endpoint_path = dir.join(ENDPOINT_FILE);
        let private = |path: &Path, kind| {
            check_private(path, kind).map_err(|problem| match problem {
                PathProblem::Missing => ConnectError::NotRunning {
                    endpoint: endpoint_path.clone(),
                },
                PathProblem::Unsafe(detail) => ConnectError::Permission {
                    path: path.to_owned(),
                    detail,
                },
                PathProblem::Io(error) => ConnectError::Failed {
                    path: path.to_owned(),
                    detail: error.to_string(),
                },
            })
        };
        private(dir, Kind::Dir)?;
        private(&endpoint_path, Kind::File)?;
        let endpoint = read_endpoint(&endpoint_path).map_err(|error| ConnectError::Failed {
            path: endpoint_path.clone(),
            detail: format!("{error}; restart myArc to rewrite it"),
        })?;
        if endpoint.protocol_version != PROTOCOL_VERSION {
            return Err(ConnectError::ProtocolMismatch {
                server: bounded(&endpoint.protocol_version).to_owned(),
                client: PROTOCOL_VERSION.to_owned(),
            });
        }
        let socket = &endpoint.socket;
        let inside = socket.file_name().is_some()
            && socket
                .parent()
                .and_then(|parent| parent.canonicalize().ok())
                .zip(dir.canonicalize().ok())
                .is_some_and(|(parent, dir)| parent == dir);
        if !inside {
            return Err(ConnectError::Permission {
                path: socket.clone(),
                detail: String::from("is outside the control directory"),
            });
        }
        let stale = || ConnectError::Stale {
            endpoint: endpoint_path.clone(),
            pid: endpoint.pid,
        };
        match check_private(socket, Kind::Socket) {
            Err(PathProblem::Missing) => return Err(stale()),
            Err(_) => private(socket, Kind::Socket)?,
            Ok(()) => {}
        }
        let failed = |detail: String| ConnectError::Failed {
            path: socket.clone(),
            detail,
        };
        let mut stream = match UnixStream::connect(socket) {
            Ok(stream) => stream,
            Err(error)
                if matches!(
                    error.kind(),
                    ErrorKind::ConnectionRefused | ErrorKind::NotFound
                ) =>
            {
                return Err(stale());
            }
            Err(error) => return Err(failed(error.to_string())),
        };
        stream
            .set_read_timeout(Some(CLIENT_READ_TIMEOUT))
            .and_then(|()| stream.set_write_timeout(Some(WRITE_TIMEOUT)))
            .map_err(|error| failed(error.to_string()))?;
        let hello = ClientMessage::Hello {
            token: endpoint.token,
            client: ClientInfo {
                name: bounded(name).to_owned(),
                version: bounded(version).to_owned(),
            },
            protocol_version: PROTOCOL_VERSION.to_owned(),
        };
        write_json(&mut stream, &hello).map_err(|error| failed(error.to_string()))?;
        let reply = match read_frame(&mut stream) {
            Ok(Frame::Message(bytes)) => serde_json::from_slice::<ServerMessage>(&bytes)
                .map_err(|_| failed(String::from("myArc sent an unreadable handshake reply")))?,
            Ok(Frame::TooLarge(_)) => {
                return Err(failed(String::from(
                    "myArc sent an oversized handshake reply",
                )));
            }
            Ok(Frame::Closed) => {
                return Err(failed(String::from(
                    "myArc closed the connection during the handshake",
                )));
            }
            Err(error) => return Err(failed(error.to_string())),
        };
        match reply {
            ServerMessage::Welcome { session_id } => Ok(Client {
                stream,
                session_id: bounded(&session_id).to_owned(),
            }),
            ServerMessage::Error { code, .. } if code == TOKEN_REJECTED => {
                Err(ConnectError::TokenRejected {
                    endpoint: endpoint_path,
                })
            }
            ServerMessage::Error {
                code,
                protocol_version,
                ..
            } if code == PROTOCOL_MISMATCH => Err(ConnectError::ProtocolMismatch {
                server: bounded(protocol_version.as_deref().unwrap_or("unknown")).to_owned(),
                client: PROTOCOL_VERSION.to_owned(),
            }),
            ServerMessage::Error { code, message, .. } => {
                Err(failed(format!("{}: {}", bounded(&code), bounded(&message))))
            }
        }
    }

    /// The session ID from the server's welcome.
    pub fn session_id(&self) -> &str {
        &self.session_id
    }

    /// Sends one request and waits for its response. A request too large for
    /// a frame fails with `InvalidInput` before anything is sent. Any other
    /// error closes the connection, and the request may or may not have run.
    pub fn call(&mut self, request: Request) -> io::Result<Response> {
        let body = serde_json::to_vec(&request).map_err(io::Error::other)?;
        if body.len() > MAX_FRAME_BYTES {
            return Err(io::Error::new(
                ErrorKind::InvalidInput,
                too_large(body.len()),
            ));
        }
        let result = self.exchange(&body);
        if result.is_err() {
            let _ = self.stream.shutdown(std::net::Shutdown::Both);
        }
        result
    }

    fn exchange(&mut self, body: &[u8]) -> io::Result<Response> {
        write_frame(&mut self.stream, body)?;
        let bytes = match read_frame(&mut self.stream)? {
            Frame::Message(bytes) => bytes,
            Frame::TooLarge(len) => {
                return Err(io::Error::new(ErrorKind::InvalidData, too_large(len)));
            }
            Frame::Closed => {
                return Err(io::Error::new(
                    ErrorKind::UnexpectedEof,
                    "myArc closed the control connection",
                ));
            }
        };
        if let Ok(response) = serde_json::from_slice::<Response>(&bytes) {
            return Ok(response);
        }
        match serde_json::from_slice::<ServerMessage>(&bytes) {
            Ok(ServerMessage::Error { message, .. }) => Err(io::Error::other(format!(
                "myArc rejected the request: {}",
                bounded(&message)
            ))),
            _ => Err(io::Error::new(
                ErrorKind::InvalidData,
                "myArc sent a reply that is not a control response",
            )),
        }
    }
}

#[cfg(test)]
mod tests {
    use std::sync::Arc;
    use std::sync::atomic::AtomicU32;
    use std::thread::JoinHandle;

    use serde_json::{Value, json};

    use super::*;
    use crate::ControlService;

    /// Unique directory under the system temp dir, removed on drop.
    struct TempDir(PathBuf);

    impl TempDir {
        fn new() -> Self {
            static NEXT: AtomicU32 = AtomicU32::new(0);
            let n = NEXT.fetch_add(1, Ordering::Relaxed);
            let path = std::env::temp_dir().join(format!("mci-{}-{n}", std::process::id()));
            let _ = fs::remove_dir_all(&path);
            Self(path)
        }
    }

    impl Drop for TempDir {
        fn drop(&mut self) {
            let _ = fs::remove_dir_all(&self.0);
        }
    }

    struct Running {
        stop: Arc<AtomicBool>,
        thread: Option<JoinHandle<io::Result<()>>>,
    }

    impl Running {
        fn stop(&mut self) {
            self.stop.store(true, Ordering::Release);
            if let Some(thread) = self.thread.take() {
                thread.join().unwrap().unwrap();
            }
        }
    }

    impl Drop for Running {
        fn drop(&mut self) {
            self.stop();
        }
    }

    fn start(server: Server) -> Running {
        let stop = Arc::new(AtomicBool::new(false));
        let flag = Arc::clone(&stop);
        let thread = thread::spawn(move || {
            let mut service = ControlService::new();
            server.serve(|request| service.handle(request), &flag)
        });
        Running {
            stop,
            thread: Some(thread),
        }
    }

    fn serve(dir: &Path) -> Running {
        start(Server::bind(dir).unwrap())
    }

    fn request(command: &str, params: Value) -> Request {
        Request {
            id: String::from("t"),
            command: command.to_owned(),
            params,
        }
    }

    fn endpoint(dir: &Path) -> EndpointFile {
        read_endpoint(&dir.join(ENDPOINT_FILE)).unwrap()
    }

    fn write_private(path: &Path, bytes: &[u8]) {
        let _ = fs::remove_file(path);
        let mut file = OpenOptions::new()
            .write(true)
            .create_new(true)
            .mode(0o600)
            .open(path)
            .unwrap();
        file.write_all(bytes).unwrap();
    }

    /// Connects without the client and sends `hello` as the first frame.
    fn raw(dir: &Path, hello: &[u8]) -> UnixStream {
        let mut stream = UnixStream::connect(dir.join(SOCKET_FILE)).unwrap();
        stream
            .set_read_timeout(Some(Duration::from_secs(5)))
            .unwrap();
        write_frame(&mut stream, hello).unwrap();
        stream
    }

    fn hello(token: &Token, version: &str) -> Vec<u8> {
        serde_json::to_vec(&json!({
            "type": "hello",
            "token": token,
            "client": { "name": "test", "version": "0" },
            "protocol_version": version,
        }))
        .unwrap()
    }

    fn read_json(stream: &mut UnixStream) -> Value {
        match read_frame(stream).unwrap() {
            Frame::Message(bytes) => serde_json::from_slice(&bytes).unwrap(),
            _ => panic!("expected a frame"),
        }
    }

    fn closed(stream: &mut UnixStream) -> bool {
        matches!(read_frame(stream), Ok(Frame::Closed) | Err(_))
    }

    #[test]
    fn frames_round_trip() {
        let mut buffer = Vec::new();
        write_frame(&mut buffer, b"{\"a\":1}").unwrap();
        assert_eq!(&buffer[..4], &[0, 0, 0, 7]);
        let mut input = &buffer[..];
        assert!(
            matches!(read_frame(&mut input).unwrap(), Frame::Message(body) if body == b"{\"a\":1}")
        );
        assert!(matches!(read_frame(&mut input).unwrap(), Frame::Closed));
    }

    #[test]
    fn oversize_frames_are_refused_both_ways() {
        let header = (MAX_FRAME_BYTES as u32 + 1).to_be_bytes();
        let mut input = &header[..];
        assert!(
            matches!(read_frame(&mut input).unwrap(), Frame::TooLarge(len) if len == MAX_FRAME_BYTES + 1)
        );
        let error = write_frame(&mut Vec::new(), &vec![b' '; MAX_FRAME_BYTES + 1]).unwrap_err();
        assert_eq!(error.kind(), ErrorKind::InvalidInput);
        let mut buffer = Vec::new();
        write_frame(&mut buffer, &vec![b' '; MAX_FRAME_BYTES]).unwrap();
        assert_eq!(buffer.len(), MAX_FRAME_BYTES + 4);
    }

    #[test]
    fn truncated_frames_are_errors() {
        let mut header = &[0u8, 0][..];
        let error = read_frame(&mut header).err().unwrap();
        assert_eq!(error.kind(), ErrorKind::UnexpectedEof);
        let mut body = &[0u8, 0, 0, 9, b'{'][..];
        let error = read_frame(&mut body).err().unwrap();
        assert_eq!(error.kind(), ErrorKind::UnexpectedEof);
        assert!(error.to_string().contains("9-byte frame"), "{error}");
    }

    #[test]
    fn bind_publishes_a_private_endpoint() {
        let dir = TempDir::new();
        let server = Server::bind(&dir.0).unwrap();
        let mode = |path: &Path| fs::metadata(path).unwrap().mode() & 0o777;
        assert_eq!(mode(&dir.0), 0o700);
        assert_eq!(mode(&dir.0.join(ENDPOINT_FILE)), 0o600);
        assert_eq!(mode(&dir.0.join(SOCKET_FILE)), 0o600);
        let file = endpoint(&dir.0);
        assert_eq!(file.socket, dir.0.join(SOCKET_FILE));
        assert_eq!(file.pid, std::process::id());
        assert_eq!(file.protocol_version, PROTOCOL_VERSION);
        assert_eq!(file.token.0.len(), TOKEN_BYTES * 2);
        assert!(file.token.0.bytes().all(|byte| byte.is_ascii_hexdigit()));
        assert!(file.token.matches(&server.token));

        let error = Server::bind(&dir.0).unwrap_err();
        assert_eq!(error.kind(), ErrorKind::AddrInUse);
        drop(server);
        assert!(!dir.0.join(ENDPOINT_FILE).exists());
        assert!(!dir.0.join(SOCKET_FILE).exists());
    }

    #[test]
    fn each_bind_gets_a_fresh_token() {
        let dir = TempDir::new();
        let first = endpoint(Server::bind(&dir.0).unwrap().dir()).token;
        // The server above is already dropped; bind again.
        let server = Server::bind(&dir.0).unwrap();
        assert!(!first.matches(&endpoint(server.dir()).token));
    }

    #[test]
    fn token_stays_out_of_debug_output() {
        let dir = TempDir::new();
        let server = Server::bind(&dir.0).unwrap();
        let file = endpoint(&dir.0);
        let secret = file.token.0.clone();
        for text in [
            format!("{server:?}"),
            format!("{file:?}"),
            format!("{:?}", file.token),
        ] {
            assert!(!text.contains(&secret), "{text}");
        }
        assert!(format!("{file:?}").contains("<redacted>"));
        let _running = start(server);
        let client = Client::connect(&dir.0).unwrap();
        assert!(!format!("{client:?}").contains(&secret));
    }

    #[test]
    fn clients_share_the_service_and_stop_removes_the_endpoint() {
        let dir = TempDir::new();
        let mut running = serve(&dir.0);
        let mut first = Client::connect(&dir.0).unwrap();
        let mut second = Client::connect_as(&dir.0, "second", "1").unwrap();
        assert_ne!(first.session_id(), second.session_id());
        let created = first
            .call(request("space.create", json!({ "name": "Work" })))
            .unwrap();
        assert!(matches!(created.outcome, Outcome::Success { .. }));
        let listed = second.call(request("space.list", Value::Null)).unwrap();
        let Outcome::Success { data } = listed.outcome else {
            panic!("space.list failed");
        };
        assert_eq!(data["spaces"].as_array().unwrap().len(), 2);
        assert_eq!(listed.id, "t");

        running.stop();
        assert!(!dir.0.join(ENDPOINT_FILE).exists());
        assert!(!dir.0.join(SOCKET_FILE).exists());
        assert!(first.call(request("space.list", Value::Null)).is_err());
    }

    #[test]
    fn missing_endpoint_means_not_running() {
        let dir = TempDir::new();
        let error = Client::connect(&dir.0).unwrap_err();
        assert!(matches!(error, ConnectError::NotRunning { .. }));
        assert!(
            error.to_string().starts_with("myArc isn't running"),
            "{error}"
        );
        fs::create_dir(&dir.0).unwrap();
        fs::set_permissions(&dir.0, Permissions::from_mode(0o700)).unwrap();
        assert!(matches!(
            Client::connect(&dir.0).unwrap_err(),
            ConnectError::NotRunning { .. }
        ));
    }

    #[test]
    fn bad_token_is_rejected() {
        let dir = TempDir::new();
        let _running = serve(&dir.0);
        let mut file = endpoint(&dir.0);
        let secret = file.token.0.clone();
        file.token = Token::generate().unwrap();
        write_private(
            &dir.0.join(ENDPOINT_FILE),
            &serde_json::to_vec(&file).unwrap(),
        );
        let error = Client::connect(&dir.0).unwrap_err();
        assert!(
            matches!(error, ConnectError::TokenRejected { .. }),
            "{error}"
        );
        let text = error.to_string();
        assert!(text.starts_with("token rejected"), "{text}");
        assert!(!text.contains(&secret) && !text.contains(&file.token.0));

        let mut stream = raw(&dir.0, &hello(&Token(String::from("00")), PROTOCOL_VERSION));
        let reply = read_json(&mut stream);
        assert_eq!(reply["type"], "error");
        assert_eq!(reply["code"], TOKEN_REJECTED);
        assert!(closed(&mut stream));
    }

    #[test]
    fn protocol_mismatch_is_reported() {
        let dir = TempDir::new();
        let _running = serve(&dir.0);
        let token = endpoint(&dir.0).token;
        let mut stream = raw(&dir.0, &hello(&token, "9.9.9"));
        let reply = read_json(&mut stream);
        assert_eq!(reply["code"], PROTOCOL_MISMATCH);
        assert_eq!(reply["protocol_version"], PROTOCOL_VERSION);
        assert!(closed(&mut stream));

        let mut file = endpoint(&dir.0);
        file.protocol_version = String::from("9.9.9");
        write_private(
            &dir.0.join(ENDPOINT_FILE),
            &serde_json::to_vec(&file).unwrap(),
        );
        let error = Client::connect(&dir.0).unwrap_err();
        assert!(matches!(error, ConnectError::ProtocolMismatch { .. }));
        assert!(error.to_string().starts_with("protocol version mismatch"));
    }

    #[test]
    fn wrong_permissions_are_rejected() {
        let dir = TempDir::new();
        let _running = serve(&dir.0);
        let file = dir.0.join(ENDPOINT_FILE);
        fs::set_permissions(&file, Permissions::from_mode(0o644)).unwrap();
        let error = Client::connect(&dir.0).unwrap_err();
        assert!(matches!(error, ConnectError::Permission { .. }), "{error}");
        let text = error.to_string();
        assert!(text.starts_with("permission/ownership problem"), "{text}");
        assert!(text.contains("644"), "{text}");
        fs::set_permissions(&file, Permissions::from_mode(0o600)).unwrap();
        assert!(Client::connect(&dir.0).is_ok());

        fs::set_permissions(&dir.0, Permissions::from_mode(0o755)).unwrap();
        assert!(matches!(
            Client::connect(&dir.0).unwrap_err(),
            ConnectError::Permission { .. }
        ));
        fs::set_permissions(&dir.0, Permissions::from_mode(0o700)).unwrap();

        let socket = dir.0.join(SOCKET_FILE);
        fs::set_permissions(&socket, Permissions::from_mode(0o666)).unwrap();
        assert!(matches!(
            Client::connect(&dir.0).unwrap_err(),
            ConnectError::Permission { .. }
        ));
    }

    #[test]
    fn socket_outside_the_directory_is_rejected() {
        let dir = TempDir::new();
        let _running = serve(&dir.0);
        let mut file = endpoint(&dir.0);
        file.socket = std::env::temp_dir().join(SOCKET_FILE);
        write_private(
            &dir.0.join(ENDPOINT_FILE),
            &serde_json::to_vec(&file).unwrap(),
        );
        assert!(matches!(
            Client::connect(&dir.0).unwrap_err(),
            ConnectError::Permission { .. }
        ));
    }

    #[test]
    fn stale_endpoint_is_reported_then_replaced() {
        let dir = TempDir::new();
        let server = Server::bind(&dir.0).unwrap();
        let left = fs::read(dir.0.join(ENDPOINT_FILE)).unwrap();
        drop(server);
        // What a crashed server leaves: the endpoint file and a socket with
        // nothing listening.
        write_private(&dir.0.join(ENDPOINT_FILE), &left);
        drop(UnixListener::bind(dir.0.join(SOCKET_FILE)).unwrap());
        fs::set_permissions(dir.0.join(SOCKET_FILE), Permissions::from_mode(0o600)).unwrap();
        let error = Client::connect(&dir.0).unwrap_err();
        assert!(matches!(error, ConnectError::Stale { .. }), "{error}");
        assert!(error.to_string().starts_with("endpoint is stale"));

        let _running = serve(&dir.0);
        let mut client = Client::connect(&dir.0).unwrap();
        assert!(client.call(request("space.list", Value::Null)).is_ok());
    }

    #[test]
    fn bad_json_gets_an_error() {
        let dir = TempDir::new();
        let _running = serve(&dir.0);
        let mut stream = raw(&dir.0, b"{not json");
        let reply = read_json(&mut stream);
        assert_eq!(reply["code"], BAD_HELLO);
        assert!(closed(&mut stream));

        let token = endpoint(&dir.0).token;
        let mut stream = raw(&dir.0, &hello(&token, PROTOCOL_VERSION));
        assert_eq!(read_json(&mut stream)["type"], "welcome");
        write_frame(&mut stream, b"[1, 2").unwrap();
        let reply = read_json(&mut stream);
        assert_eq!(reply["status"], "invalid");
        // The connection survives a bad request.
        write_frame(&mut stream, br#"{"id":"x","command":"space.list"}"#).unwrap();
        assert_eq!(read_json(&mut stream)["status"], "success");
    }

    #[test]
    fn oversize_frame_gets_an_error_then_close() {
        let dir = TempDir::new();
        let _running = serve(&dir.0);
        let mut stream = UnixStream::connect(dir.0.join(SOCKET_FILE)).unwrap();
        stream
            .set_read_timeout(Some(Duration::from_secs(5)))
            .unwrap();
        stream
            .write_all(&(MAX_FRAME_BYTES as u32 + 1).to_be_bytes())
            .unwrap();
        let reply = read_json(&mut stream);
        assert_eq!(reply["code"], FRAME_TOO_LARGE);
        assert!(closed(&mut stream));
    }

    #[test]
    fn truncated_hello_closes_the_connection() {
        let dir = TempDir::new();
        let _running = serve(&dir.0);
        let mut stream = UnixStream::connect(dir.0.join(SOCKET_FILE)).unwrap();
        stream
            .set_read_timeout(Some(Duration::from_secs(5)))
            .unwrap();
        stream.write_all(&[0, 0, 0, 40, b'{']).unwrap();
        stream.shutdown(std::net::Shutdown::Write).unwrap();
        assert!(closed(&mut stream));
        // The server keeps serving others.
        assert!(Client::connect(&dir.0).is_ok());
    }

    #[test]
    fn idle_connections_are_closed() {
        let dir = TempDir::new();
        let mut server = Server::bind(&dir.0).unwrap();
        server.set_idle_timeout(Duration::from_millis(200));
        let _running = start(server);
        let mut client = Client::connect(&dir.0).unwrap();
        assert!(client.call(request("space.list", Value::Null)).is_ok());
        thread::sleep(Duration::from_millis(500));
        assert!(client.call(request("space.list", Value::Null)).is_err());
        let mut fresh = Client::connect(&dir.0).unwrap();
        assert!(fresh.call(request("space.list", Value::Null)).is_ok());
    }

    #[test]
    fn handler_panic_answers_outcome_unknown() {
        let dir = TempDir::new();
        let server = Server::bind(&dir.0).unwrap();
        let stop = Arc::new(AtomicBool::new(false));
        let flag = Arc::clone(&stop);
        let thread = thread::spawn(move || {
            server.serve(
                |request: Request| {
                    if request.command == "boom" {
                        panic!("handler failed");
                    }
                    ControlService::new().handle(request)
                },
                &flag,
            )
        });
        let mut running = Running {
            stop,
            thread: Some(thread),
        };
        let mut client = Client::connect(&dir.0).unwrap();
        let response = client.call(request("boom", Value::Null)).unwrap();
        assert!(matches!(response.outcome, Outcome::OutcomeUnknown { .. }));
        assert!(client.call(request("space.list", Value::Null)).is_ok());
        running.stop();
    }
}
