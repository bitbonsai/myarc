//! Writes the workspace file so a crash or a killed process never leaves a
//! partial one in its place. The adapter encodes the file; this module only
//! moves bytes.
//!
//! A save writes the whole file to `<file>.tmp`, keeps the current file as
//! `<file>.bak` and renames the temporary file over the current one. A rename
//! within a directory is atomic, so the file on disk is always one complete
//! generation, the old one or the new one. Once `write` returns, the bytes are
//! in the kernel: a process kill after that, even a SIGKILL, keeps them.
//!
//! `write` does not wait for the disk, which takes milliseconds on macOS. The
//! adapter calls `sync` afterwards off the UI thread to flush the file and its
//! directory. Until that returns, a power loss can lose the newest generation;
//! the adapter then falls back to the backup when the file fails to decode.
//!
//! `set_aside` moves a file that failed to decode to `<file>.corrupt`, so a
//! later save cannot turn it into the backup and the user can still recover it.

use std::ffi::OsString;
use std::fs::{self, File};
use std::io::{self, ErrorKind, Write};
use std::path::{Path, PathBuf};

fn sibling(path: &Path, suffix: &str) -> PathBuf {
    let mut name = OsString::from(path.as_os_str());
    name.push(suffix);
    PathBuf::from(name)
}

/// Where `write` keeps the generation before the current one.
pub fn backup_path(path: &Path) -> PathBuf {
    sibling(path, ".bak")
}

/// Where `set_aside` moves a file that failed to decode.
pub fn aside_path(path: &Path) -> PathBuf {
    sibling(path, ".corrupt")
}

fn temp_path(path: &Path) -> PathBuf {
    sibling(path, ".tmp")
}

/// Replaces the file at `path` with `bytes` in one atomic rename, keeping the
/// current file as the backup. On error the current file is unchanged. A
/// temporary file left by an earlier crash is overwritten.
pub fn write(path: &Path, bytes: &[u8]) -> io::Result<()> {
    let temp = temp_path(path);
    let written = File::create(&temp).and_then(|mut file| file.write_all(bytes));
    if let Err(error) = written {
        let _ = fs::remove_file(&temp);
        return Err(error);
    }
    keep_backup(path);
    fs::rename(&temp, path).inspect_err(|_| {
        let _ = fs::remove_file(&temp);
    })
}

/// Links the current file as the backup, through a temporary name so the
/// backup is never half-replaced and the current file never goes missing. A
/// file system without hard links gets a copy. The backup is a convenience:
/// failing to make one does not fail the save.
fn keep_backup(path: &Path) {
    let staged = sibling(path, ".bak.tmp");
    let _ = fs::remove_file(&staged);
    let made = match fs::hard_link(path, &staged) {
        Ok(()) => Ok(()),
        Err(error) if error.kind() == ErrorKind::NotFound => return,
        Err(_) => fs::copy(path, &staged).map(|_| ()),
    };
    if made
        .and_then(|()| fs::rename(&staged, backup_path(path)))
        .is_err()
    {
        let _ = fs::remove_file(&staged);
    }
}

/// Flushes the file at `path` and its directory to the disk. Slow: call it off
/// the UI thread.
pub fn sync(path: &Path) -> io::Result<()> {
    File::open(path)?.sync_all()?;
    match path.parent() {
        Some(dir) if !dir.as_os_str().is_empty() => File::open(dir)?.sync_all(),
        _ => Ok(()),
    }
}

/// Moves the file at `path` to `aside_path`, replacing an older one there,
/// and returns where it went.
pub fn set_aside(path: &Path) -> io::Result<PathBuf> {
    let aside = aside_path(path);
    fs::rename(path, &aside)?;
    Ok(aside)
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::sync::atomic::{AtomicUsize, Ordering};

    struct Dir(PathBuf);

    impl Dir {
        fn new() -> Self {
            static NEXT: AtomicUsize = AtomicUsize::new(0);
            let n = NEXT.fetch_add(1, Ordering::Relaxed);
            let dir = std::env::temp_dir().join(format!("myarc-store-{}-{n}", std::process::id()));
            let _ = fs::remove_dir_all(&dir);
            fs::create_dir_all(&dir).unwrap();
            Self(dir)
        }

        fn file(&self) -> PathBuf {
            self.0.join("myarc-workspace-v1.json")
        }

        fn names(&self) -> Vec<String> {
            let mut names: Vec<String> = fs::read_dir(&self.0)
                .unwrap()
                .map(|entry| entry.unwrap().file_name().into_string().unwrap())
                .collect();
            names.sort();
            names
        }
    }

    impl Drop for Dir {
        fn drop(&mut self) {
            let _ = fs::remove_dir_all(&self.0);
        }
    }

    #[test]
    fn a_first_write_creates_the_file_without_a_backup() {
        let dir = Dir::new();
        let file = dir.file();
        write(&file, b"one").unwrap();
        assert_eq!(fs::read(&file).unwrap(), b"one");
        assert_eq!(dir.names(), ["myarc-workspace-v1.json"]);
    }

    #[test]
    fn each_write_keeps_the_previous_generation_as_the_backup() {
        let dir = Dir::new();
        let file = dir.file();
        write(&file, b"one").unwrap();
        write(&file, b"two").unwrap();
        assert_eq!(fs::read(&file).unwrap(), b"two");
        assert_eq!(fs::read(backup_path(&file)).unwrap(), b"one");
        write(&file, b"three").unwrap();
        assert_eq!(fs::read(&file).unwrap(), b"three");
        assert_eq!(fs::read(backup_path(&file)).unwrap(), b"two");
        assert_eq!(
            dir.names(),
            ["myarc-workspace-v1.json", "myarc-workspace-v1.json.bak"]
        );
    }

    #[test]
    fn a_torn_temporary_file_from_a_crash_never_replaces_the_file() {
        let dir = Dir::new();
        let file = dir.file();
        write(&file, b"{\"good\":true}").unwrap();
        // A crash mid-write leaves half a temporary file behind. The current
        // file is untouched, and the next save overwrites the leftover.
        fs::write(temp_path(&file), b"{\"go").unwrap();
        assert_eq!(fs::read(&file).unwrap(), b"{\"good\":true}");
        write(&file, b"{\"good\":2}").unwrap();
        assert_eq!(fs::read(&file).unwrap(), b"{\"good\":2}");
        assert!(!temp_path(&file).exists());
    }

    #[test]
    fn a_failed_write_leaves_the_file_and_backup_unchanged() {
        let dir = Dir::new();
        let file = dir.file();
        write(&file, b"one").unwrap();
        write(&file, b"two").unwrap();
        // A directory where the temporary file goes makes the write fail, as
        // a full disk or a permission error would.
        fs::create_dir(temp_path(&file)).unwrap();
        assert!(write(&file, b"three").is_err());
        assert_eq!(fs::read(&file).unwrap(), b"two");
        assert_eq!(fs::read(backup_path(&file)).unwrap(), b"one");
    }

    #[test]
    fn the_file_is_never_missing_while_a_backup_is_made() {
        let dir = Dir::new();
        let file = dir.file();
        write(&file, b"one").unwrap();
        // A staged backup left by a crash is replaced, and the file stays.
        fs::write(sibling(&file, ".bak.tmp"), b"stale").unwrap();
        write(&file, b"two").unwrap();
        assert_eq!(fs::read(&file).unwrap(), b"two");
        assert_eq!(fs::read(backup_path(&file)).unwrap(), b"one");
        assert!(!sibling(&file, ".bak.tmp").exists());
    }

    #[test]
    fn sync_flushes_the_file_and_its_directory() {
        let dir = Dir::new();
        let file = dir.file();
        write(&file, b"one").unwrap();
        sync(&file).unwrap();
        assert_eq!(
            sync(&dir.0.join("missing")).unwrap_err().kind(),
            ErrorKind::NotFound
        );
    }

    #[test]
    fn a_damaged_file_set_aside_keeps_the_backup_for_the_next_save() {
        let dir = Dir::new();
        let file = dir.file();
        write(&file, b"good").unwrap();
        write(&file, b"newer").unwrap();
        // Power loss tore the newest generation; the loader reads the backup
        // and sets the damaged file aside.
        fs::write(&file, b"ne").unwrap();
        let aside = set_aside(&file).unwrap();
        assert_eq!(aside, aside_path(&file));
        assert_eq!(fs::read(&aside).unwrap(), b"ne");
        assert!(!file.exists());
        // The next save starts the file again and leaves the good backup.
        write(&file, b"restored").unwrap();
        assert_eq!(fs::read(&file).unwrap(), b"restored");
        assert_eq!(fs::read(backup_path(&file)).unwrap(), b"good");
    }
}
