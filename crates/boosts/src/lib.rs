#![forbid(unsafe_code)]

//! Dependency-free, bounded Boosts core model, origin matcher, CSS sanitizer,
//! stylesheet compiler, revision history, and JSON codec.
//!
//! Boosts Phase B1 is strictly pure CSS with zero page JavaScript, explicit
//! HTTP(S) origin scoping, Profile/OTR isolation (owned by the browser service),
//! and zero remote CSS fetches.

use std::collections::BTreeSet;
use std::fmt;

pub mod css;
pub mod json;
pub mod origin;

pub use css::{
    CssBlockReason, CssFinding, CssSanitizeResult, compile_boost_css, compile_boosts_css,
    font_stack, inspect_css, sanitize_css, validate_custom_css, validate_selector,
};
pub use json::{decode_boost, decode_snapshot, encode_boost, encode_snapshot};
pub use origin::{ParsedOrigin, Scheme, display_host, normalize_origin, origin_matches};

pub const SNAPSHOT_VERSION: u32 = 1;
pub const MAX_BOOSTS: usize = 256;
pub const MAX_NAME_BYTES: usize = 128;
pub const MAX_ORIGINS_PER_BOOST: usize = 16;
pub const MAX_ORIGIN_BYTES: usize = 256;
pub const MAX_HIDDEN_SELECTORS: usize = 128;
pub const MAX_SELECTOR_BYTES: usize = 256;
pub const MAX_CUSTOM_CSS_BYTES: usize = 65_536;
pub const MAX_SCRIPT_PAYLOAD_BYTES: usize = 65_536;
pub const MAX_REVISIONS: usize = 10;
pub const MAX_FILE_BYTES: usize = 8 * 1024 * 1024;
pub const MAX_JSON_DEPTH: usize = 10;
pub const MIN_TEXT_SCALE_PCT: u16 = 50;
pub const MAX_TEXT_SCALE_PCT: u16 = 200;
pub const DEFAULT_TEXT_SCALE_PCT: u16 = 100;

pub type BoostId = u64;

#[derive(Debug, PartialEq, Eq)]
pub enum BoostsError {
    UnknownBoost(BoostId),
    InvalidName,
    InvalidOrigin,
    EmptyOrigins,
    DuplicateOrigin,
    InvalidAppearance,
    InvalidSelector,
    DisallowedCss(Vec<CssFinding>),
    LimitExceeded(&'static str),
    IdExhausted,
    UnsupportedSnapshotVersion(u32),
    InvalidSnapshot(&'static str),
}

impl fmt::Display for BoostsError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::UnknownBoost(id) => write!(f, "Boost {id} does not exist"),
            Self::InvalidName => {
                write!(
                    f,
                    "Boost name must be non-empty, at most {MAX_NAME_BYTES} UTF-8 bytes, and contain no control characters"
                )
            }
            Self::InvalidOrigin => f.write_str(
                "Boost origin must be a canonical http:// or https:// origin without credentials, paths, wildcards, or control characters",
            ),
            Self::EmptyOrigins => f.write_str("Boost must specify at least one allowed origin"),
            Self::DuplicateOrigin => f.write_str("Boost contains a duplicate origin"),
            Self::InvalidAppearance => f.write_str(
                "Boost appearance values out of range (colors 0xRRGGBB, contrast/tint 0..=100, text scale 50..=200)",
            ),
            Self::InvalidSelector => write!(
                f,
                "Hidden selector must be non-empty, at most {MAX_SELECTOR_BYTES} bytes, balanced, and contain no '{{', '}}', ';', '@', comments, or control characters"
            ),
            Self::DisallowedCss(findings) => {
                if let Some(first) = findings.first() {
                    write!(
                        f,
                        "CSS blocked at line {}: {} ({} issue(s))",
                        first.line,
                        first.reason,
                        findings.len()
                    )
                } else {
                    f.write_str("CSS contains blocked directives")
                }
            }
            Self::LimitExceeded(kind) => write!(f, "Boosts {kind} limit exceeded"),
            Self::IdExhausted => f.write_str("Boost IDs exhausted"),
            Self::UnsupportedSnapshotVersion(v) => {
                write!(
                    f,
                    "Boosts snapshot version {v} is unsupported; expected {SNAPSHOT_VERSION}"
                )
            }
            Self::InvalidSnapshot(reason) => write!(f, "Invalid Boosts snapshot: {reason}"),
        }
    }
}

impl std::error::Error for BoostsError {}

/// Lighting / color mode matching the 3 mode toggles on the floating Boost card.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub enum ColorMode {
    /// Clear / slash-circle: no color override.
    #[default]
    None,
    /// Lightbulb: inverted light/dark mode with media counter-inversion.
    Invert,
    /// Sliders: custom palette colors, contrast, and tint strength.
    Palette,
}

impl ColorMode {
    pub const fn as_str(self) -> &'static str {
        match self {
            Self::None => "none",
            Self::Invert => "invert",
            Self::Palette => "palette",
        }
    }

    pub fn parse(s: &str) -> Option<Self> {
        match s {
            "none" => Some(Self::None),
            "invert" => Some(Self::Invert),
            "palette" => Some(Self::Palette),
            _ => None,
        }
    }
}

/// Custom palette colors and 2D pad contrast/tint controls.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct BoostPalette {
    /// 0xRRGGBB
    pub background: u32,
    /// 0xRRGGBB
    pub text: u32,
    /// 0xRRGGBB
    pub accent: u32,
    /// 0..=100 (50 = neutral)
    pub contrast: u8,
    /// 0..=100 (50 = neutral)
    pub brightness: u8,
    /// 0..=100 (50 = neutral, 0 = grayscale, 100 = 200%)
    pub saturation: u8,
    /// 0..=100
    pub tint_strength: u8,
}

/// 16 local system font presets matching the 4x4 `Aa` grid on the Boost card.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub enum FontPreset {
    #[default]
    Default,
    SystemSans,
    SystemSerif,
    SystemMono,
    Rounded,
    Geometric,
    Humanist,
    Grotesk,
    Didone,
    SlabSerif,
    ClassicSerif,
    Typewriter,
    Handwritten,
    Condensed,
    Wide,
    Comic,
}

impl FontPreset {
    pub const ALL: [Self; 16] = [
        Self::Default,
        Self::SystemSans,
        Self::SystemSerif,
        Self::SystemMono,
        Self::Rounded,
        Self::Geometric,
        Self::Humanist,
        Self::Grotesk,
        Self::Didone,
        Self::SlabSerif,
        Self::ClassicSerif,
        Self::Typewriter,
        Self::Handwritten,
        Self::Condensed,
        Self::Wide,
        Self::Comic,
    ];

    pub const fn as_str(self) -> &'static str {
        match self {
            Self::Default => "default",
            Self::SystemSans => "system_sans",
            Self::SystemSerif => "system_serif",
            Self::SystemMono => "system_mono",
            Self::Rounded => "rounded",
            Self::Geometric => "geometric",
            Self::Humanist => "humanist",
            Self::Grotesk => "grotesk",
            Self::Didone => "didone",
            Self::SlabSerif => "slab_serif",
            Self::ClassicSerif => "classic_serif",
            Self::Typewriter => "typewriter",
            Self::Handwritten => "handwritten",
            Self::Condensed => "condensed",
            Self::Wide => "wide",
            Self::Comic => "comic",
        }
    }

    pub fn parse(s: &str) -> Option<Self> {
        match s {
            "default" => Some(Self::Default),
            "system_sans" => Some(Self::SystemSans),
            "system_serif" => Some(Self::SystemSerif),
            "system_mono" => Some(Self::SystemMono),
            "rounded" => Some(Self::Rounded),
            "geometric" => Some(Self::Geometric),
            "humanist" => Some(Self::Humanist),
            "grotesk" => Some(Self::Grotesk),
            "didone" => Some(Self::Didone),
            "slab_serif" => Some(Self::SlabSerif),
            "classic_serif" => Some(Self::ClassicSerif),
            "typewriter" => Some(Self::Typewriter),
            "handwritten" => Some(Self::Handwritten),
            "condensed" => Some(Self::Condensed),
            "wide" => Some(Self::Wide),
            "comic" => Some(Self::Comic),
            _ => None,
        }
    }
}

/// Text case override matching the `Case` button on the Boost card.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub enum TextTransform {
    #[default]
    None,
    Uppercase,
    Lowercase,
    Capitalize,
}

impl TextTransform {
    pub const fn as_str(self) -> &'static str {
        match self {
            Self::None => "none",
            Self::Uppercase => "uppercase",
            Self::Lowercase => "lowercase",
            Self::Capitalize => "capitalize",
        }
    }

    pub fn parse(s: &str) -> Option<Self> {
        match s {
            "none" => Some(Self::None),
            "uppercase" => Some(Self::Uppercase),
            "lowercase" => Some(Self::Lowercase),
            "capitalize" => Some(Self::Capitalize),
            _ => None,
        }
    }
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct BoostAppearance {
    pub color_mode: ColorMode,
    pub palette: Option<BoostPalette>,
    pub font: FontPreset,
    pub text_scale_pct: u16,
    pub text_transform: TextTransform,
}

impl Default for BoostAppearance {
    fn default() -> Self {
        Self {
            color_mode: ColorMode::None,
            palette: None,
            font: FontPreset::Default,
            text_scale_pct: DEFAULT_TEXT_SCALE_PCT,
            text_transform: TextTransform::None,
        }
    }
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct BoostRevision {
    pub version: u32,
    pub updated_secs: u64,
    pub appearance: BoostAppearance,
    pub hidden_selectors: Vec<String>,
    pub custom_css: String,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Boost {
    pub id: BoostId,
    pub name: String,
    pub origins: Vec<String>,
    pub include_subdomains: bool,
    pub apply_to_subframes: bool,
    pub enabled: bool,
    pub version: u32,
    pub updated_secs: u64,
    pub appearance: BoostAppearance,
    pub hidden_selectors: Vec<String>,
    pub custom_css: String,
    /// B3 reserved; ignored and never executed in B1.
    pub script_payload: Option<String>,
    /// Bounded undo history ring (oldest first, at most `MAX_REVISIONS`).
    pub revisions: Vec<BoostRevision>,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct BoostsSnapshot {
    pub version: u32,
    pub global_enabled: bool,
    pub boosts: Vec<Boost>,
    pub next_boost_id: BoostId,
}

/// Inspection result returned when importing a Boost so the UI can display
/// blocked CSS findings or an inert script payload without enabling or executing it.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct ImportReport {
    pub id: BoostId,
    pub css_findings: Vec<CssFinding>,
    pub has_inert_script: bool,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct BoostStore {
    state: BoostsSnapshot,
}

impl Default for BoostStore {
    fn default() -> Self {
        Self::new()
    }
}

impl BoostStore {
    pub fn new() -> Self {
        Self {
            state: BoostsSnapshot {
                version: SNAPSHOT_VERSION,
                global_enabled: true,
                boosts: Vec::new(),
                next_boost_id: 1,
            },
        }
    }

    pub fn from_snapshot(mut snapshot: BoostsSnapshot) -> Result<Self, BoostsError> {
        if snapshot.version != SNAPSHOT_VERSION {
            return Err(BoostsError::UnsupportedSnapshotVersion(snapshot.version));
        }
        if snapshot.next_boost_id == 0 {
            return Err(BoostsError::InvalidSnapshot(
                "next_boost_id must be nonzero",
            ));
        }
        if snapshot.boosts.len() > MAX_BOOSTS {
            return Err(BoostsError::LimitExceeded("boosts"));
        }

        let mut seen_ids = BTreeSet::new();
        for boost in &mut snapshot.boosts {
            if boost.id == 0 || boost.id >= snapshot.next_boost_id || !seen_ids.insert(boost.id) {
                return Err(BoostsError::InvalidSnapshot(
                    "Boost IDs must be nonzero, unique, and less than next_boost_id",
                ));
            }
            boost.name = validate_name(&boost.name)?;
            boost.origins = validate_origins(&boost.origins)?;
            validate_appearance(&boost.appearance)?;
            boost.hidden_selectors = validate_selectors(&boost.hidden_selectors)?;
            if boost.custom_css.len() > MAX_CUSTOM_CSS_BYTES {
                return Err(BoostsError::LimitExceeded("custom CSS"));
            }
            if boost.enabled {
                validate_custom_css(&boost.custom_css)?;
            }
            if let Some(script) = &boost.script_payload {
                if script.len() > MAX_SCRIPT_PAYLOAD_BYTES {
                    return Err(BoostsError::LimitExceeded("script payload"));
                }
            }
            if boost.revisions.len() > MAX_REVISIONS {
                return Err(BoostsError::LimitExceeded("revisions"));
            }
            for rev in &mut boost.revisions {
                validate_appearance(&rev.appearance)?;
                rev.hidden_selectors = validate_selectors(&rev.hidden_selectors)?;
                if rev.custom_css.len() > MAX_CUSTOM_CSS_BYTES {
                    return Err(BoostsError::LimitExceeded("revision custom CSS"));
                }
            }
        }

        Ok(Self { state: snapshot })
    }

    pub fn snapshot(&self) -> BoostsSnapshot {
        self.state.clone()
    }

    pub fn global_enabled(&self) -> bool {
        self.state.global_enabled
    }

    /// Toggles the Profile-wide Boosts kill switch. Returns whether it changed.
    pub fn set_global_enabled(&mut self, enabled: bool) -> bool {
        if self.state.global_enabled == enabled {
            return false;
        }
        self.state.global_enabled = enabled;
        true
    }

    pub fn boosts(&self) -> &[Boost] {
        &self.state.boosts
    }

    pub fn boost(&self, id: BoostId) -> Option<&Boost> {
        self.state.boosts.iter().find(|b| b.id == id)
    }

    /// Creates a new enabled Boost scoped to `origins`.
    pub fn create_boost(
        &mut self,
        name: &str,
        origins: &[&str],
        now_secs: u64,
    ) -> Result<BoostId, BoostsError> {
        if self.state.boosts.len() >= MAX_BOOSTS {
            return Err(BoostsError::LimitExceeded("boosts"));
        }
        let clean_name = validate_name(name)?;
        let origin_strings: Vec<String> = origins.iter().map(|s| (*s).to_owned()).collect();
        let clean_origins = validate_origins(&origin_strings)?;
        let id = self.state.next_boost_id;
        let next = id.checked_add(1).ok_or(BoostsError::IdExhausted)?;

        self.state.boosts.push(Boost {
            id,
            name: clean_name,
            origins: clean_origins,
            include_subdomains: false,
            apply_to_subframes: false,
            enabled: true,
            version: 1,
            updated_secs: now_secs,
            appearance: BoostAppearance::default(),
            hidden_selectors: Vec::new(),
            custom_css: String::new(),
            script_payload: None,
            revisions: Vec::new(),
        });
        self.state.next_boost_id = next;
        Ok(id)
    }

    /// Imports an untrusted Boost in a disabled, inert state.
    ///
    /// Never compiles CSS or executes scripts. Forces `enabled = false`, assigns
    /// a fresh local ID, validates bounds/origins/selectors, and returns any
    /// CSS security findings or inert script indicator for the inspection UI.
    pub fn import_boost(
        &mut self,
        mut boost: Boost,
        now_secs: u64,
    ) -> Result<ImportReport, BoostsError> {
        if self.state.boosts.len() >= MAX_BOOSTS {
            return Err(BoostsError::LimitExceeded("boosts"));
        }
        boost.name = validate_name(&boost.name)?;
        boost.origins = validate_origins(&boost.origins)?;
        validate_appearance(&boost.appearance)?;
        boost.hidden_selectors = validate_selectors(&boost.hidden_selectors)?;
        if boost.custom_css.len() > MAX_CUSTOM_CSS_BYTES {
            return Err(BoostsError::LimitExceeded("custom CSS"));
        }
        if let Some(script) = &boost.script_payload {
            if script.len() > MAX_SCRIPT_PAYLOAD_BYTES {
                return Err(BoostsError::LimitExceeded("script payload"));
            }
        }
        if boost.revisions.len() > MAX_REVISIONS {
            let excess = boost.revisions.len() - MAX_REVISIONS;
            boost.revisions.drain(0..excess);
        }
        for rev in &mut boost.revisions {
            validate_appearance(&rev.appearance)?;
            rev.hidden_selectors = validate_selectors(&rev.hidden_selectors)?;
            if rev.custom_css.len() > MAX_CUSTOM_CSS_BYTES {
                return Err(BoostsError::LimitExceeded("revision custom CSS"));
            }
        }

        let css_findings = inspect_css(&boost.custom_css);
        let has_inert_script = boost
            .script_payload
            .as_deref()
            .is_some_and(|s| !s.trim().is_empty());

        let id = self.state.next_boost_id;
        let next = id.checked_add(1).ok_or(BoostsError::IdExhausted)?;
        boost.id = id;
        boost.enabled = false;
        boost.version = boost.version.max(1);
        boost.updated_secs = now_secs;

        self.state.boosts.push(boost);
        self.state.next_boost_id = next;

        Ok(ImportReport {
            id,
            css_findings,
            has_inert_script,
        })
    }

    pub fn rename_boost(
        &mut self,
        id: BoostId,
        name: &str,
        now_secs: u64,
    ) -> Result<bool, BoostsError> {
        let clean = validate_name(name)?;
        let boost = self.boost_mut(id)?;
        if boost.name == clean {
            return Ok(false);
        }
        boost.name = clean;
        boost.updated_secs = now_secs;
        Ok(true)
    }

    pub fn set_enabled(
        &mut self,
        id: BoostId,
        enabled: bool,
        now_secs: u64,
    ) -> Result<bool, BoostsError> {
        let boost = self.boost_mut(id)?;
        if enabled {
            validate_custom_css(&boost.custom_css)?;
        }
        if boost.enabled == enabled {
            return Ok(false);
        }
        boost.enabled = enabled;
        boost.updated_secs = now_secs;
        Ok(true)
    }

    pub fn set_scope(
        &mut self,
        id: BoostId,
        origins: &[&str],
        include_subdomains: bool,
        apply_to_subframes: bool,
        now_secs: u64,
    ) -> Result<bool, BoostsError> {
        let origin_strings: Vec<String> = origins.iter().map(|s| (*s).to_owned()).collect();
        let clean_origins = validate_origins(&origin_strings)?;
        let boost = self.boost_mut(id)?;
        if boost.origins == clean_origins
            && boost.include_subdomains == include_subdomains
            && boost.apply_to_subframes == apply_to_subframes
        {
            return Ok(false);
        }
        boost.origins = clean_origins;
        boost.include_subdomains = include_subdomains;
        boost.apply_to_subframes = apply_to_subframes;
        boost.version = boost.version.saturating_add(1);
        boost.updated_secs = now_secs;
        Ok(true)
    }

    pub fn set_appearance(
        &mut self,
        id: BoostId,
        appearance: BoostAppearance,
        now_secs: u64,
    ) -> Result<bool, BoostsError> {
        validate_appearance(&appearance)?;
        let boost = self.boost_mut(id)?;
        if boost.appearance == appearance {
            return Ok(false);
        }
        push_revision(boost);
        boost.appearance = appearance;
        boost.version = boost.version.saturating_add(1);
        boost.updated_secs = now_secs;
        Ok(true)
    }

    pub fn add_hidden_selector(
        &mut self,
        id: BoostId,
        selector: &str,
        now_secs: u64,
    ) -> Result<bool, BoostsError> {
        let clean = validate_selector(selector)?;
        let boost = self.boost_mut(id)?;
        if boost.hidden_selectors.contains(&clean) {
            return Ok(false);
        }
        if boost.hidden_selectors.len() >= MAX_HIDDEN_SELECTORS {
            return Err(BoostsError::LimitExceeded("hidden selectors"));
        }
        push_revision(boost);
        boost.hidden_selectors.push(clean);
        boost.version = boost.version.saturating_add(1);
        boost.updated_secs = now_secs;
        Ok(true)
    }

    pub fn remove_hidden_selector(
        &mut self,
        id: BoostId,
        selector: &str,
        now_secs: u64,
    ) -> Result<bool, BoostsError> {
        let boost = self.boost_mut(id)?;
        let Some(pos) = boost.hidden_selectors.iter().position(|s| s == selector) else {
            return Ok(false);
        };
        push_revision(boost);
        boost.hidden_selectors.remove(pos);
        boost.version = boost.version.saturating_add(1);
        boost.updated_secs = now_secs;
        Ok(true)
    }

    pub fn clear_hidden_selectors(
        &mut self,
        id: BoostId,
        now_secs: u64,
    ) -> Result<bool, BoostsError> {
        let boost = self.boost_mut(id)?;
        if boost.hidden_selectors.is_empty() {
            return Ok(false);
        }
        push_revision(boost);
        boost.hidden_selectors.clear();
        boost.version = boost.version.saturating_add(1);
        boost.updated_secs = now_secs;
        Ok(true)
    }

    pub fn set_custom_css(
        &mut self,
        id: BoostId,
        custom_css: &str,
        now_secs: u64,
    ) -> Result<bool, BoostsError> {
        validate_custom_css(custom_css)?;
        let boost = self.boost_mut(id)?;
        if boost.custom_css == custom_css {
            return Ok(false);
        }
        push_revision(boost);
        boost.custom_css = custom_css.to_owned();
        boost.version = boost.version.saturating_add(1);
        boost.updated_secs = now_secs;
        Ok(true)
    }

    /// Resets appearance, hidden selectors, and custom CSS to defaults (recording a revision).
    pub fn reset_boost(&mut self, id: BoostId, now_secs: u64) -> Result<bool, BoostsError> {
        let boost = self.boost_mut(id)?;
        let default_appearance = BoostAppearance::default();
        if boost.appearance == default_appearance
            && boost.hidden_selectors.is_empty()
            && boost.custom_css.is_empty()
        {
            return Ok(false);
        }
        push_revision(boost);
        boost.appearance = default_appearance;
        boost.hidden_selectors.clear();
        boost.custom_css.clear();
        boost.version = boost.version.saturating_add(1);
        boost.updated_secs = now_secs;
        Ok(true)
    }

    /// Restores the most recent revision from the bounded revision ring.
    /// Returns `Ok(false)` when no prior revision is available.
    pub fn undo(&mut self, id: BoostId, now_secs: u64) -> Result<bool, BoostsError> {
        let boost = self.boost_mut(id)?;
        let Some(prev) = boost.revisions.pop() else {
            return Ok(false);
        };
        boost.appearance = prev.appearance;
        boost.hidden_selectors = prev.hidden_selectors;
        boost.custom_css = prev.custom_css;
        boost.version = boost.version.saturating_add(1);
        boost.updated_secs = now_secs;
        Ok(true)
    }

    pub fn delete_boost(&mut self, id: BoostId) -> Result<(), BoostsError> {
        let pos = self
            .state
            .boosts
            .iter()
            .position(|b| b.id == id)
            .ok_or(BoostsError::UnknownBoost(id))?;
        self.state.boosts.remove(pos);
        Ok(())
    }

    /// Returns all active Boosts matching `page_origin` (respecting `global_enabled`
    /// and `apply_to_subframes`).
    pub fn matching_boosts(&self, page_origin: &str, is_subframe: bool) -> Vec<&Boost> {
        if !self.state.global_enabled {
            return Vec::new();
        }
        let Ok(page) = ParsedOrigin::parse(page_origin) else {
            return Vec::new();
        };
        self.state
            .boosts
            .iter()
            .filter(|boost| {
                boost.enabled
                    && (!is_subframe || boost.apply_to_subframes)
                    && boost.origins.iter().any(|allowed| {
                        ParsedOrigin::parse(allowed)
                            .is_ok_and(|a| a.matches(&page, boost.include_subdomains))
                    })
            })
            .collect()
    }

    /// Compiles one deterministic stylesheet for all active Boosts matching `page_origin`.
    pub fn compile_for_origin(&self, page_origin: &str, is_subframe: bool) -> String {
        let matches = self.matching_boosts(page_origin, is_subframe);
        compile_boosts_css(&matches)
    }

    /// Case-insensitive search across Boost names and origins for the Library pane.
    pub fn search(&self, query: &str) -> Vec<&Boost> {
        let q = query.trim().to_ascii_lowercase();
        if q.is_empty() {
            return self.state.boosts.iter().collect();
        }
        self.state
            .boosts
            .iter()
            .filter(|boost| {
                boost.name.to_ascii_lowercase().contains(&q)
                    || boost
                        .origins
                        .iter()
                        .any(|o| o.to_ascii_lowercase().contains(&q))
            })
            .collect()
    }

    fn boost_mut(&mut self, id: BoostId) -> Result<&mut Boost, BoostsError> {
        self.state
            .boosts
            .iter_mut()
            .find(|b| b.id == id)
            .ok_or(BoostsError::UnknownBoost(id))
    }
}

fn push_revision(boost: &mut Boost) {
    if boost.revisions.len() >= MAX_REVISIONS {
        let excess = boost.revisions.len() - MAX_REVISIONS + 1;
        boost.revisions.drain(0..excess);
    }
    boost.revisions.push(BoostRevision {
        version: boost.version,
        updated_secs: boost.updated_secs,
        appearance: boost.appearance.clone(),
        hidden_selectors: boost.hidden_selectors.clone(),
        custom_css: boost.custom_css.clone(),
    });
}

fn validate_name(raw: &str) -> Result<String, BoostsError> {
    let trimmed = raw.trim();
    if trimmed.is_empty() || trimmed.len() > MAX_NAME_BYTES || trimmed.chars().any(char::is_control)
    {
        return Err(BoostsError::InvalidName);
    }
    Ok(trimmed.to_owned())
}

fn validate_origins(origins: &[String]) -> Result<Vec<String>, BoostsError> {
    if origins.is_empty() {
        return Err(BoostsError::EmptyOrigins);
    }
    if origins.len() > MAX_ORIGINS_PER_BOOST {
        return Err(BoostsError::LimitExceeded("origins"));
    }
    let mut normalized = Vec::with_capacity(origins.len());
    let mut seen = BTreeSet::new();
    for raw in origins {
        let norm = normalize_origin(raw)?;
        if !seen.insert(norm.clone()) {
            return Err(BoostsError::DuplicateOrigin);
        }
        normalized.push(norm);
    }
    Ok(normalized)
}

fn validate_appearance(appearance: &BoostAppearance) -> Result<(), BoostsError> {
    if !(MIN_TEXT_SCALE_PCT..=MAX_TEXT_SCALE_PCT).contains(&appearance.text_scale_pct) {
        return Err(BoostsError::InvalidAppearance);
    }
    if let Some(p) = appearance.palette {
        if p.background > 0xFF_FFFF
            || p.text > 0xFF_FFFF
            || p.accent > 0xFF_FFFF
            || p.contrast > 100
            || p.brightness > 100
            || p.saturation > 100
            || p.tint_strength > 100
        {
            return Err(BoostsError::InvalidAppearance);
        }
    }
    Ok(())
}

fn validate_selectors(selectors: &[String]) -> Result<Vec<String>, BoostsError> {
    if selectors.len() > MAX_HIDDEN_SELECTORS {
        return Err(BoostsError::LimitExceeded("hidden selectors"));
    }
    let mut out = Vec::with_capacity(selectors.len());
    for sel in selectors {
        out.push(validate_selector(sel)?);
    }
    Ok(out)
}

#[cfg(test)]
mod tests {
    use super::*;

    // --- Origin validation and matching ---

    #[test]
    fn origin_normalization_and_exact_matching() {
        assert_eq!(
            normalize_origin("HTTPS://Example.COM:443").unwrap(),
            "https://example.com"
        );
        assert_eq!(
            normalize_origin("http://127.0.0.1:80").unwrap(),
            "http://127.0.0.1"
        );
        assert_eq!(
            normalize_origin("http://127.0.0.1:8080").unwrap(),
            "http://127.0.0.1:8080"
        );
        assert_eq!(
            normalize_origin("http://[::1]:3000").unwrap(),
            "http://[::1]:3000"
        );
        assert_eq!(
            display_host("https://www.npmjs.com").as_deref(),
            Some("www.npmjs.com")
        );
        assert_eq!(
            display_host("http://127.0.0.1:8080").as_deref(),
            Some("127.0.0.1:8080")
        );

        assert!(origin_matches(
            "https://example.com",
            "https://example.com",
            false
        ));
        assert!(origin_matches(
            "https://example.com",
            "https://example.com:443",
            false
        ));
        assert!(!origin_matches(
            "https://example.com",
            "http://example.com",
            false
        ));
        assert!(!origin_matches(
            "https://example.com",
            "https://example.com:8443",
            false
        ));
        assert!(!origin_matches(
            "https://example.com",
            "https://sub.example.com",
            false
        ));
    }

    #[test]
    fn origin_subdomain_matching_rejects_lookalikes_and_ips() {
        assert!(origin_matches(
            "https://example.com",
            "https://example.com",
            true
        ));
        assert!(origin_matches(
            "https://example.com",
            "https://sub.example.com",
            true
        ));
        assert!(origin_matches(
            "https://example.com",
            "https://a.b.example.com",
            true
        ));

        // Look-alike hosts must never match.
        assert!(!origin_matches(
            "https://example.com",
            "https://notexample.com",
            true
        ));
        assert!(!origin_matches(
            "https://example.com",
            "https://example.com.evil.test",
            true
        ));
        // Scheme/port mismatches must not match even with include_subdomains.
        assert!(!origin_matches(
            "https://example.com",
            "http://sub.example.com",
            true
        ));
        assert!(!origin_matches(
            "https://example.com",
            "https://sub.example.com:8443",
            true
        ));
        // IP literals never accept subdomains.
        assert!(!origin_matches(
            "http://127.0.0.1",
            "http://a.127.0.0.1",
            true
        ));
        assert!(!origin_matches(
            "http://[::1]:8080",
            "http://sub.[::1]:8080",
            true
        ));
    }

    #[test]
    fn origin_rejects_privileged_schemes_wildcards_paths_and_credentials() {
        for bad in [
            "",
            "chrome://settings",
            "chrome-untrusted://new-tab-page",
            "chrome-extension://abcdefghijklmnop",
            "devtools://devtools",
            "file:///etc/passwd",
            "about:blank",
            "data:text/html,hi",
            "blob:https://example.com/123",
            "javascript:alert(1)",
            "view-source:https://example.com",
            "ftp://example.com",
            "https://*",
            "https://*.example.com",
            "https://user:pass@example.com",
            "https://example.com/",
            "https://example.com/path",
            "https://example.com?q=1",
            "https://example.com#frag",
            "https://example.com\\evil",
            "https://example.com:0",
            "https://example.com:99999",
            "https://-bad.example.com",
            "https://bad..example.com",
            "https://example.com\n",
        ] {
            assert_eq!(
                normalize_origin(bad),
                Err(BoostsError::InvalidOrigin),
                "{bad}"
            );
            assert!(!origin_matches("https://example.com", bad, true), "{bad}");
        }
    }

    // --- CSS sanitizer & selector validator ---

    #[test]
    fn css_sanitizer_blocks_import_and_remote_urls_including_escapes_and_comments() {
        let cases = [
            (
                "@import url(https://evil.test/a.css);",
                CssBlockReason::AtImport,
            ),
            (
                "@import 'https://evil.test/a.css';",
                CssBlockReason::AtImport,
            ),
            ("@IMPORT \"a.css\";", CssBlockReason::AtImport),
            (
                "@\\69 mport \"https://evil.test/a.css\";",
                CssBlockReason::AtImport,
            ),
            ("@im\\70 ort 'a.css';", CssBlockReason::AtImport),
            (
                "body { background: url(http://evil.test/a.png); }",
                CssBlockReason::DisallowedUrl,
            ),
            (
                "body { background: url(https://evil.test/a.png); }",
                CssBlockReason::DisallowedUrl,
            ),
            (
                "body { background: url(//evil.test/a.png); }",
                CssBlockReason::DisallowedUrl,
            ),
            (
                "body { background: url(/relative.png); }",
                CssBlockReason::DisallowedUrl,
            ),
            (
                "body { background: url(data:image/png;base64,AA==); }",
                CssBlockReason::DisallowedUrl,
            ),
            (
                "body { background: url(blob:https://evil.test/1); }",
                CssBlockReason::DisallowedUrl,
            ),
            (
                "body { background: url(file:///etc/passwd); }",
                CssBlockReason::DisallowedUrl,
            ),
            (
                "body { background: url(chrome://settings); }",
                CssBlockReason::DisallowedUrl,
            ),
            (
                "body { background: URL(https://evil.test/a.png); }",
                CssBlockReason::DisallowedUrl,
            ),
            (
                "body { background: \\75 rl(https://evil.test/a.png); }",
                CssBlockReason::DisallowedUrl,
            ),
            (
                "body { background: u\\72 l('https://evil.test/a.png'); }",
                CssBlockReason::DisallowedUrl,
            ),
            (
                "body { background: /*x*/url(https://evil.test/a.png); }",
                CssBlockReason::DisallowedUrl,
            ),
            (
                "body { background: url(/*x*/https://evil.test/a.png); }",
                CssBlockReason::DisallowedUrl,
            ),
            (
                "body { background: image-set('a.png' 1x, 'b.png' 2x); }",
                CssBlockReason::DisallowedImageSet,
            ),
            (
                "body { background: -webkit-image-set('a.png' 1x); }",
                CssBlockReason::DisallowedImageSet,
            ),
            (
                "body { background: src('https://evil.test/a.png'); }",
                CssBlockReason::DisallowedSrc,
            ),
            ("body { color: red;\0 }", CssBlockReason::ControlCharacter),
        ];

        for (css, expected) in cases {
            let res = sanitize_css(css);
            assert!(
                res.findings.iter().any(|f| f.reason == expected),
                "expected {expected:?} for `{css}`, got {:?}",
                res.findings
            );
            assert!(
                !res.sanitized.contains("evil.test") && !res.sanitized.contains("a.png"),
                "sanitized CSS still leaked blocked target for `{css}`: {}",
                res.sanitized
            );
        }
    }

    #[test]
    fn css_sanitizer_allows_safe_rules_and_local_svg_fragments() {
        let safe = r##"
            /* Safe comment */
            body { color: #222; filter: url(#my-filter); clip-path: url("#clip_1"); }
            .card { background: linear-gradient(45deg, #111, #333); }
        "##;
        let res = sanitize_css(safe);
        assert!(
            res.findings.is_empty(),
            "unexpected findings: {:?}",
            res.findings
        );
        assert!(res.sanitized.contains("url(#my-filter)"));
        assert!(res.sanitized.contains("url(#clip_1)"));
        assert_eq!(validate_custom_css(safe), Ok(()));
    }

    #[test]
    fn css_sanitizer_closes_unclosed_comments_strings_and_braces() {
        let unclosed_comment = "body { color: red; } /* unclosed";
        let res1 = sanitize_css(unclosed_comment);
        assert!(res1.sanitized.ends_with(" */"));

        let unclosed_string = "body::after { content: \"unclosed";
        let res2 = sanitize_css(unclosed_string);
        assert!(res2.sanitized.contains("\"unclosed\""));
        assert!(res2.sanitized.ends_with('}'));

        let unclosed_brace = "body { color: red;";
        let res3 = sanitize_css(unclosed_brace);
        assert!(res3.sanitized.ends_with('}'));
    }

    #[test]
    fn css_sanitizer_reports_accurate_line_numbers() {
        let css = "body {\n  color: red;\n  background: url(https://evil.test/x.png);\n}";
        let findings = inspect_css(css);
        assert_eq!(
            findings,
            vec![CssFinding {
                line: 3,
                reason: CssBlockReason::DisallowedUrl,
            }]
        );
    }

    #[test]
    fn selector_validator_blocks_breakouts_and_allows_valid_selectors() {
        for good in [
            "#banner",
            ".ad-slot",
            "div > section.promo:nth-of-type(2)",
            "a[href^=\"https://ads.example\"]",
            ".w-1\\/2",
        ] {
            assert!(validate_selector(good).is_ok(), "{good}");
        }

        for bad in [
            "",
            "   ",
            "} body { color: red",
            "div { display: block; }",
            "div; body",
            "@import url(x)",
            "div /* comment */",
            "div[attr='unclosed",
            "div:nth-child(2",
            "div\nspan",
        ] {
            assert_eq!(
                validate_selector(bad),
                Err(BoostsError::InvalidSelector),
                "{bad}"
            );
        }

        let oversized = "a".repeat(MAX_SELECTOR_BYTES + 1);
        assert_eq!(
            validate_selector(&oversized),
            Err(BoostsError::InvalidSelector)
        );
    }

    // --- Compiler ---

    #[test]
    fn compiler_emits_deterministic_pure_css_for_all_appearance_controls() {
        let mut store = BoostStore::new();
        let id = store
            .create_boost("My Boost", &["https://fly.io"], 100)
            .unwrap();
        store
            .set_appearance(
                id,
                BoostAppearance {
                    color_mode: ColorMode::Palette,
                    palette: Some(BoostPalette {
                        background: 0x112233,
                        text: 0xEEEEEE,
                        accent: 0xFF6600,
                        contrast: 70,
                        brightness: 55,
                        saturation: 45,
                        tint_strength: 80,
                    }),
                    font: FontPreset::SystemMono,
                    text_scale_pct: 120,
                    text_transform: TextTransform::Uppercase,
                },
                101,
            )
            .unwrap();
        store
            .add_hidden_selector(id, ".cookie-banner", 102)
            .unwrap();
        store
            .set_custom_css(id, "main { max-width: 720px; }", 103)
            .unwrap();

        let css = store.compile_for_origin("https://fly.io", false);
        assert!(css.contains("#112233"));
        assert!(css.contains("#eeeeee"));
        assert!(css.contains("#ff6600"));
        assert!(css.contains("contrast(120%)"));
        assert!(css.contains("ui-monospace"));
        assert!(css.contains("font-size: 120% !important;"));
        assert!(css.contains("text-transform: uppercase !important;"));
        assert!(css.contains(".cookie-banner { display: none !important; }"));
        assert!(css.contains("main { max-width: 720px; }"));

        // Invert mode emits invert + media counter-invert.
        store
            .set_appearance(
                id,
                BoostAppearance {
                    color_mode: ColorMode::Invert,
                    palette: None,
                    font: FontPreset::Default,
                    text_scale_pct: 100,
                    text_transform: TextTransform::None,
                },
                104,
            )
            .unwrap();
        let invert_css = store.compile_for_origin("https://fly.io", false);
        assert!(invert_css.contains("invert(1) hue-rotate(180deg)"));
        assert!(invert_css.contains("img, video, picture, canvas, svg"));
    }

    #[test]
    fn all_16_font_presets_round_trip_and_use_local_stacks() {
        assert_eq!(FontPreset::ALL.len(), 16);
        for preset in FontPreset::ALL {
            assert_eq!(FontPreset::parse(preset.as_str()), Some(preset));
            if let Some(stack) = font_stack(preset) {
                assert!(!stack.contains("http"));
                assert!(!stack.contains("url("));
            }
        }
    }

    // --- Store, revisions, bounds, global kill switch, import safety ---

    #[test]
    fn revisions_undo_and_cap_at_max_revisions() {
        let mut store = BoostStore::new();
        let id = store
            .create_boost("Reader", &["https://news.ycombinator.com"], 1)
            .unwrap();

        for i in 1..=15u64 {
            store
                .set_custom_css(id, &format!("body {{ opacity: 0.{i}; }}"), i + 1)
                .unwrap();
        }
        let boost = store.boost(id).unwrap();
        assert_eq!(boost.revisions.len(), MAX_REVISIONS);
        assert_eq!(boost.custom_css, "body { opacity: 0.15; }");

        assert_eq!(store.undo(id, 100), Ok(true));
        assert_eq!(
            store.boost(id).unwrap().custom_css,
            "body { opacity: 0.14; }"
        );

        // Drain remaining 9 revisions, then undo returns Ok(false).
        for _ in 0..9 {
            assert_eq!(store.undo(id, 101), Ok(true));
        }
        assert_eq!(store.undo(id, 102), Ok(false));
    }

    #[test]
    fn global_kill_switch_and_subframe_isolation() {
        let mut store = BoostStore::new();
        let id = store
            .create_boost("Site", &["https://example.com"], 10)
            .unwrap();
        store.add_hidden_selector(id, ".promo", 11).unwrap();

        assert!(
            !store
                .compile_for_origin("https://example.com", false)
                .is_empty()
        );
        // Subframes skipped by default.
        assert!(
            store
                .compile_for_origin("https://example.com", true)
                .is_empty()
        );

        // Enable subframes.
        store
            .set_scope(id, &["https://example.com"], false, true, 12)
            .unwrap();
        assert!(
            !store
                .compile_for_origin("https://example.com", true)
                .is_empty()
        );

        // Global kill switch disables all output immediately.
        assert!(store.set_global_enabled(false));
        assert!(
            store
                .compile_for_origin("https://example.com", false)
                .is_empty()
        );
        assert!(
            store
                .compile_for_origin("https://example.com", true)
                .is_empty()
        );
        assert!(store.set_global_enabled(true));
        assert!(
            !store
                .compile_for_origin("https://example.com", false)
                .is_empty()
        );
    }

    #[test]
    fn import_forces_disabled_and_reports_blocked_css_and_inert_script() {
        let untrusted = Boost {
            id: 999,
            name: "Imported".into(),
            origins: vec!["https://example.com".into()],
            include_subdomains: false,
            apply_to_subframes: false,
            enabled: true, // must be forced false!
            version: 4,
            updated_secs: 50,
            appearance: BoostAppearance::default(),
            hidden_selectors: vec![".ad".into()],
            custom_css: "@import url(https://evil.test/canary.css);\nbody { background: url(https://evil.test/canary.png); }".into(),
            script_payload: Some("alert('xss')".into()),
            revisions: Vec::new(),
        };

        let mut store = BoostStore::new();
        let report = store.import_boost(untrusted, 200).unwrap();
        assert_eq!(report.id, 1);
        assert!(report.has_inert_script);
        assert_eq!(report.css_findings.len(), 2);

        assert!(!store.boost(report.id).unwrap().enabled);
        assert!(
            store
                .compile_for_origin("https://example.com", false)
                .is_empty()
        );

        // Cannot enable until blocked CSS is cleaned.
        assert!(matches!(
            store.set_enabled(report.id, true, 201),
            Err(BoostsError::DisallowedCss(_))
        ));

        // Defense-in-depth: even compiling the raw Boost strips remote URLs.
        let compiled = compile_boost_css(store.boost(report.id).unwrap());
        assert!(!compiled.contains("evil.test"));
        assert!(!compiled.contains("@import"));
    }

    #[test]
    fn bounds_enforcement_on_store_and_snapshot() {
        let mut store = BoostStore::new();
        for i in 0..MAX_BOOSTS {
            store
                .create_boost(
                    &format!("B{i}"),
                    &[&format!("https://s{i}.example.test")],
                    1,
                )
                .unwrap();
        }
        assert_eq!(
            store.create_boost("Overflow", &["https://extra.example.test"], 1),
            Err(BoostsError::LimitExceeded("boosts"))
        );

        // >16 origins rejected.
        let mut too_many_origins = Vec::new();
        for i in 0..=MAX_ORIGINS_PER_BOOST {
            too_many_origins.push(format!("https://o{i}.example.test"));
        }
        let refs: Vec<&str> = too_many_origins.iter().map(String::as_str).collect();
        let mut s2 = BoostStore::new();
        assert_eq!(
            s2.create_boost("TooMany", &refs, 1),
            Err(BoostsError::LimitExceeded("origins"))
        );

        // >128 selectors rejected.
        let id = s2.create_boost("S", &["https://example.test"], 1).unwrap();
        for i in 0..MAX_HIDDEN_SELECTORS {
            s2.add_hidden_selector(id, &format!(".s{i}"), 2).unwrap();
        }
        assert_eq!(
            s2.add_hidden_selector(id, ".overflow", 3),
            Err(BoostsError::LimitExceeded("hidden selectors"))
        );

        // >64 KiB CSS rejected.
        let huge_css = "a".repeat(MAX_CUSTOM_CSS_BYTES + 1);
        assert_eq!(
            s2.set_custom_css(id, &huge_css, 4),
            Err(BoostsError::LimitExceeded("custom CSS"))
        );
    }

    // --- JSON codec round-trip, recovery, and adversarial inputs ---

    #[test]
    fn json_snapshot_and_single_boost_round_trip() {
        let mut store = BoostStore::new();
        let id = store
            .create_boost(
                "Café Boost ☕",
                &["https://www.npmjs.com", "https://fly.io"],
                100,
            )
            .unwrap();
        store
            .set_appearance(
                id,
                BoostAppearance {
                    color_mode: ColorMode::Palette,
                    palette: Some(BoostPalette {
                        background: 0x0A0B0C,
                        text: 0xF0F1F2,
                        accent: 0x7744FF,
                        contrast: 65,
                        brightness: 50,
                        saturation: 50,
                        tint_strength: 40,
                    }),
                    font: FontPreset::Geometric,
                    text_scale_pct: 110,
                    text_transform: TextTransform::Capitalize,
                },
                101,
            )
            .unwrap();
        store.add_hidden_selector(id, ".ad-banner", 102).unwrap();
        store
            .set_custom_css(
                id,
                "body { /* quote \"test\" */\n  letter-spacing: 0.02em;\n}",
                103,
            )
            .unwrap();

        let encoded = encode_snapshot(&store.snapshot());
        let decoded = decode_snapshot(encoded.as_bytes()).unwrap();
        let restored = BoostStore::from_snapshot(decoded).unwrap();
        assert_eq!(restored, store);

        // Single Boost export/import round-trip.
        let boost = store.boost(id).unwrap();
        let boost_json = encode_boost(boost);
        let decoded_boost = decode_boost(boost_json.as_bytes()).unwrap();
        assert_eq!(&decoded_boost, boost);
    }

    #[test]
    fn json_rejects_unsupported_version_oversized_deep_and_corrupt_inputs() {
        let future = br#"{"version":99,"global_enabled":true,"next_boost_id":1,"boosts":[]}"#;
        assert_eq!(
            decode_snapshot(future),
            Err(BoostsError::UnsupportedSnapshotVersion(99))
        );

        let deeply_nested = format!(
            "{}{}",
            "[".repeat(MAX_JSON_DEPTH + 2),
            "]".repeat(MAX_JSON_DEPTH + 2)
        );
        assert!(matches!(
            decode_snapshot(deeply_nested.as_bytes()),
            Err(BoostsError::InvalidSnapshot(_))
        ));

        for corrupt in [
            b"".as_slice(),
            b"{",
            b"{\"version\":1,\"next_boost_id\":1,\"boosts\":[]} trailing",
            b"\xff\xfe",
        ] {
            assert!(decode_snapshot(corrupt).is_err());
        }
    }

    #[test]
    fn search_matches_name_and_origin_case_insensitively() {
        let mut store = BoostStore::new();
        let a = store
            .create_boost("My Boost", &["https://fly.io"], 1)
            .unwrap();
        let b = store
            .create_boost("HN Reader", &["https://news.ycombinator.com"], 2)
            .unwrap();
        assert_eq!(store.search("").len(), 2);
        assert_eq!(
            store.search("FLY").iter().map(|x| x.id).collect::<Vec<_>>(),
            vec![a]
        );
        assert_eq!(
            store
                .search("reader")
                .iter()
                .map(|x| x.id)
                .collect::<Vec<_>>(),
            vec![b]
        );
        assert!(store.search("nonexistent").is_empty());
    }
}
