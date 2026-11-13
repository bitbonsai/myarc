#![forbid(unsafe_code)]

//! Readers for exports from other browsers and the mapping into the workspace
//! model. Readers turn untrusted bytes into a typed report; the mapping previews
//! and writes that report into a `myarc_workspace::Workspace`. Neither opens tabs
//! or fetches URLs. The only dependency is `myarc-workspace`.

pub mod arc_markdown;
pub mod arc_workspace;
