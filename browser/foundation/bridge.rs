chromium::import! {
    "//myarc:workspace";
}

pub struct Workspace(workspace::Workspace);

#[cxx::bridge(namespace = "myarc::foundation")]
mod ffi {
    struct RenameResult {
        ok: bool,
        name: String,
        error: String,
    }

    extern "Rust" {
        type Workspace;
        fn new_workspace() -> Box<Workspace>;
        fn current_name(workspace: &Workspace) -> String;
        fn rename_space(workspace: &mut Workspace, id: u64, name: &str) -> RenameResult;
    }
}

pub fn new_workspace() -> Box<Workspace> {
    Box::new(Workspace(workspace::Workspace::default()))
}

pub fn current_name(workspace: &Workspace) -> String {
    workspace.0.space_name().to_owned()
}

pub fn rename_space(workspace: &mut Workspace, id: u64, name: &str) -> ffi::RenameResult {
    let result = workspace.0.rename_space(id, name);
    ffi::RenameResult {
        ok: result.is_ok(),
        name: current_name(workspace),
        error: result
            .err()
            .map(|error| error.to_string())
            .unwrap_or_default(),
    }
}
