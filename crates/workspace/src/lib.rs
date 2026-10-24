#![forbid(unsafe_code)]

use std::fmt;

pub const MAX_SPACE_NAME_BYTES: usize = 128;
pub const TEST_SPACE_ID: u64 = 1;

#[derive(Debug, PartialEq, Eq)]
pub enum RenameError {
    UnknownSpace,
    EmptyName,
    NameTooLong,
    ControlCharacter,
}

impl fmt::Display for RenameError {
    fn fmt(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
        formatter.write_str(match self {
            Self::UnknownSpace => "Space does not exist",
            Self::EmptyName => "Enter a Space name",
            Self::NameTooLong => "Space name exceeds 128 UTF-8 bytes",
            Self::ControlCharacter => "Space name cannot contain control characters",
        })
    }
}

impl std::error::Error for RenameError {}

// shortcut: one transient Space for M0 bridge proof, replace with the validated MA-004 model.
#[derive(Debug, PartialEq, Eq)]
pub struct Workspace {
    space_name: String,
}

impl Default for Workspace {
    fn default() -> Self {
        Self {
            space_name: String::from("Everyday"),
        }
    }
}

impl Workspace {
    pub fn space_name(&self) -> &str {
        &self.space_name
    }

    pub fn rename_space(&mut self, id: u64, name: &str) -> Result<(), RenameError> {
        if id != TEST_SPACE_ID {
            return Err(RenameError::UnknownSpace);
        }
        if name.len() > MAX_SPACE_NAME_BYTES {
            return Err(RenameError::NameTooLong);
        }
        if name.trim().is_empty() {
            return Err(RenameError::EmptyName);
        }
        if name.chars().any(char::is_control) {
            return Err(RenameError::ControlCharacter);
        }
        self.space_name = name.to_owned();
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn rename_updates_shared_state() {
        let mut workspace = Workspace::default();
        assert_eq!(workspace.rename_space(TEST_SPACE_ID, "Research"), Ok(()));
        assert_eq!(workspace.space_name(), "Research");
        assert_eq!(workspace.rename_space(TEST_SPACE_ID, "Everyday"), Ok(()));
        assert_eq!(workspace.space_name(), "Everyday");
    }

    #[test]
    fn rejects_unknown_space_without_mutation() {
        let mut workspace = Workspace::default();
        assert_eq!(
            workspace.rename_space(2, "Research"),
            Err(RenameError::UnknownSpace)
        );
        assert_eq!(workspace.space_name(), "Everyday");
    }

    #[test]
    fn rejects_blank_names_without_mutation() {
        let mut workspace = Workspace::default();
        for name in ["", " ", "\u{2003}"] {
            assert_eq!(
                workspace.rename_space(TEST_SPACE_ID, name),
                Err(RenameError::EmptyName)
            );
            assert_eq!(workspace.space_name(), "Everyday");
        }
    }

    #[test]
    fn rejects_control_characters_without_mutation() {
        let mut workspace = Workspace::default();
        for name in ["Work\nNotes", "Work\0Notes", "Work\u{0085}Notes"] {
            assert_eq!(
                workspace.rename_space(TEST_SPACE_ID, name),
                Err(RenameError::ControlCharacter)
            );
            assert_eq!(workspace.space_name(), "Everyday");
        }
    }

    #[test]
    fn bounds_utf8_bytes_and_preserves_unicode() {
        let mut workspace = Workspace::default();
        let maximum = "é".repeat(MAX_SPACE_NAME_BYTES / 2);
        assert_eq!(workspace.rename_space(TEST_SPACE_ID, &maximum), Ok(()));
        assert_eq!(workspace.space_name(), maximum);
        assert_eq!(
            workspace.rename_space(TEST_SPACE_ID, &(maximum.clone() + "x")),
            Err(RenameError::NameTooLong)
        );
        assert_eq!(workspace.space_name(), maximum);
        assert_eq!(workspace.rename_space(TEST_SPACE_ID, "Café ☕"), Ok(()));
        assert_eq!(workspace.space_name(), "Café ☕");
    }

    #[test]
    fn repeat_is_idempotent_and_instances_are_independent() {
        let mut first = Workspace::default();
        let second = Workspace::default();
        assert_eq!(first.rename_space(TEST_SPACE_ID, "Research"), Ok(()));
        assert_eq!(first.rename_space(TEST_SPACE_ID, "Research"), Ok(()));
        assert_eq!(first.space_name(), "Research");
        assert_eq!(second.space_name(), "Everyday");
    }
}
