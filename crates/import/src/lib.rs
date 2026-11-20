#![forbid(unsafe_code)]

//! Readers for exports from other browsers and the mapping into the workspace
//! model. Readers turn untrusted bytes into a typed report; the mapping previews
//! and writes that report into a `myarc_workspace::Workspace`. Neither opens tabs
//! or fetches URLs. The only dependency is `myarc-workspace`, plus `serde_json`
//! for the `arc-sidebar` feature.

pub mod arc_markdown;
pub mod arc_report;
#[cfg(feature = "arc-sidebar")]
pub mod arc_sidebar;
pub mod arc_workspace;
