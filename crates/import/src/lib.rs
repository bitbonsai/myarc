#![forbid(unsafe_code)]

//! Readers for exports from other browsers. Each reader turns untrusted bytes into a
//! typed report and never writes into a workspace, opens tabs or fetches URLs.
//! Mapping a report into the workspace model belongs to the preview and write path
//! (MA-006), not here. This crate has no dependencies.

pub mod arc_markdown;
