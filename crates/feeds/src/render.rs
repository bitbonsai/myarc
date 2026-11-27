//! Self-contained, themed HTML renderer for the built-in myArc Feed Viewer.
//!
//! Uses myArc's light and dark brand tokens (see `design.md`):
//! - Light: Ivory `#F5F2EC`, Ink `#182432`, secondary `#54616D`, blue `#034BF4`
//! - Dark: `#0B1420` page, Midnight `#101C2B` panels, Ivory headings, `#D2D9E1`
//!   text, `#B1BCC9` secondary, `#8FB1FF` links/focus

use crate::sanitize::escape_html;
use crate::url::display_host;
use crate::{Feed, FeedKind};

/// Options controlling how the feed viewer page is rendered.
#[derive(Debug, Default, Clone)]
pub struct RenderOptions {
    /// When true, renders the Subscribe / Subscribed button.
    pub show_subscribe_slot: bool,
    /// Reflects whether this Profile is already subscribed to the feed.
    pub is_subscribed: bool,
}

/// Renders a parsed `Feed` into a self-contained HTML5 document with automatic
/// myArc light/dark theming, in-page filtering, copy-feed-URL, inline reading
/// for full-text feeds, Split View card clicks, and a raw XML/JSON source toggle.
pub fn render_viewer_html(feed: &Feed, raw_source: &str, options: &RenderOptions) -> String {
    let mut out = String::with_capacity(16 * 1024 + feed.items.len() * 512);
    let title_esc = escape_html(&feed.title);
    let kind_label = match feed.kind {
        FeedKind::Rss => "RSS",
        FeedKind::Atom => "Atom",
        FeedKind::JsonFeed => "JSON Feed",
    };

    out.push_str("<!doctype html>\n<html lang=\"en\">\n<head>\n<meta charset=\"utf-8\">\n");
    out.push_str("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n");
    out.push_str("<meta name=\"color-scheme\" content=\"light dark\">\n");
    out.push_str(
        "<meta name=\"theme-color\" media=\"(prefers-color-scheme: light)\" content=\"#F5F2EC\">\n",
    );
    out.push_str(
        "<meta name=\"theme-color\" media=\"(prefers-color-scheme: dark)\" content=\"#0B1420\">\n",
    );
    out.push_str("<title>");
    out.push_str(&title_esc);
    out.push_str("</title>\n<style>\n");
    out.push_str(VIEWER_CSS);
    out.push_str("\n</style>\n</head>\n<body>\n<div class=\"shell\">\n");

    // Header: Back link above a hero glass masthead card
    out.push_str("<header class=\"feed-header\">\n");
    out.push_str("<div class=\"feed-back-row\" id=\"back-row\" hidden>\n");
    out.push_str("<button id=\"back-to-page\" class=\"btn-back\" type=\"button\">");
    out.push_str("<svg width=\"14\" height=\"14\" viewBox=\"0 0 16 16\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"1.8\" stroke-linecap=\"round\" stroke-linejoin=\"round\" aria-hidden=\"true\"><path d=\"M10 3.5L5.5 8 10 12.5\"/></svg>");
    out.push_str("<span>Back to page</span></button>\n");
    out.push_str("</div>\n");

    out.push_str("<div class=\"hero-card\">\n");
    out.push_str("<div class=\"hero-top\">\n");
    out.push_str("<button id=\"rss-logo\" class=\"feed-logo\" type=\"button\" aria-label=\"Animate myArc RSS logo\" title=\"Click to animate\">\n");
    out.push_str("<span class=\"feed-logo-fallback\" aria-hidden=\"true\">");
    out.push_str(RSS_ICON_SVG);
    out.push_str("</span>\n");
    out.push_str("<canvas id=\"rss-logo-canvas\" aria-hidden=\"true\"></canvas>\n");
    out.push_str("</button>\n");

    out.push_str("<div class=\"feed-heading\">\n<h1>");
    out.push_str(&title_esc);
    out.push_str("</h1>\n");
    if let Some(subtitle) = &feed.subtitle {
        out.push_str("<p class=\"feed-subtitle\">");
        out.push_str(&escape_html(subtitle));
        out.push_str("</p>\n");
    }
    out.push_str("</div>\n");

    out.push_str("<div class=\"hero-actions\">\n");
    if options.show_subscribe_slot {
        if options.is_subscribed {
            out.push_str("<button id=\"subscribe-btn\" class=\"btn\" type=\"button\" aria-pressed=\"true\">Subscribed</button>\n");
        } else {
            out.push_str("<button id=\"subscribe-btn\" class=\"btn btn-primary\" type=\"button\" aria-pressed=\"false\">Subscribe</button>\n");
        }
    }
    out.push_str("<div class=\"menu-wrap\">\n");
    out.push_str("<button id=\"more-menu-btn\" class=\"btn btn-icon\" type=\"button\" aria-haspopup=\"menu\" aria-expanded=\"false\" aria-controls=\"more-menu\" aria-label=\"More feed actions\">");
    out.push_str("<svg width=\"16\" height=\"16\" viewBox=\"0 0 16 16\" fill=\"currentColor\" aria-hidden=\"true\"><circle cx=\"8\" cy=\"3.2\" r=\"1.35\"/><circle cx=\"8\" cy=\"8\" r=\"1.35\"/><circle cx=\"8\" cy=\"12.8\" r=\"1.35\"/></svg>");
    out.push_str("</button>\n");
    out.push_str("<div id=\"more-menu\" class=\"menu-popover\" role=\"menu\" hidden>\n");
    if let Some(feed_url) = &feed.feed_url {
        out.push_str("<button id=\"copy-url\" class=\"menu-item\" type=\"button\" role=\"menuitem\" data-url=\"");
        out.push_str(&escape_html(feed_url));
        out.push_str("\">Copy feed URL</button>\n");
    }
    out.push_str("<button id=\"toggle-raw\" class=\"menu-item\" type=\"button\" role=\"menuitem\" aria-pressed=\"false\">Raw source</button>\n");
    out.push_str("</div>\n</div>\n</div>\n</div>\n");

    // Footer strip inside the hero card: metadata left, search box with icon right
    out.push_str("<div class=\"hero-foot\">\n");
    out.push_str("<div class=\"feed-meta\">\n");
    out.push_str("<span>");
    out.push_str(kind_label);
    out.push_str("</span>\n");

    let count = feed.items.len();
    let count_label = if count == 1 {
        "1 post".to_string()
    } else if feed.truncated {
        format!("{count}+ posts")
    } else {
        format!("{count} posts")
    };
    out.push_str("<span class=\"meta-dot\" aria-hidden=\"true\">&middot;</span>\n");
    out.push_str("<span>");
    out.push_str(&count_label);
    out.push_str("</span>\n");

    if let Some(site_url) = &feed.site_url {
        let host = display_host(site_url).unwrap_or_else(|| site_url.clone());
        out.push_str("<span class=\"meta-dot\" aria-hidden=\"true\">&middot;</span>\n");
        out.push_str("<a class=\"site-link\" href=\"");
        out.push_str(&escape_html(site_url));
        out.push_str("\" rel=\"noopener noreferrer\">");
        out.push_str(&escape_html(&host));
        out.push_str("</a>\n");
    }
    if let Some(updated) = &feed.updated {
        out.push_str("<span class=\"meta-dot\" aria-hidden=\"true\">&middot;</span>\n");
        out.push_str("<span>Updated ");
        out.push_str(&escape_html(updated));
        out.push_str("</span>\n");
    }
    out.push_str("</div>\n");

    out.push_str("<div class=\"filter-wrap\">\n");
    out.push_str("<svg class=\"filter-icon\" width=\"14\" height=\"14\" viewBox=\"0 0 16 16\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"1.6\" stroke-linecap=\"round\" stroke-linejoin=\"round\" aria-hidden=\"true\"><circle cx=\"7\" cy=\"7\" r=\"4.75\"/><path d=\"M10.6 10.6L14 14\"/></svg>\n");
    out.push_str("<input id=\"filter\" class=\"filter-input\" type=\"search\" placeholder=\"Filter posts\" aria-label=\"Filter posts\">\n");
    out.push_str("</div>\n");
    out.push_str("</div>\n</div>\n");
    out.push_str("</header>\n");

    // Main feed items list
    out.push_str("<main id=\"feed-view\">\n");
    if feed.items.is_empty() {
        out.push_str("<p class=\"empty\">This feed has no items yet.</p>\n");
    } else {
        out.push_str("<ol class=\"item-list\">\n");
        for item in &feed.items {
            if let Some(link) = &item.link {
                out.push_str("<li class=\"item item-clickable\" data-href=\"");
                out.push_str(&escape_html(link));
                out.push_str("\" data-open-mode=\"split\">\n");
            } else {
                out.push_str("<li class=\"item\">\n");
            }
            out.push_str("<article>\n");
            out.push_str("<div class=\"item-head\">\n<div class=\"item-title-row\">\n<h2>");
            if let Some(link) = &item.link {
                out.push_str("<a class=\"item-link\" href=\"");
                out.push_str(&escape_html(link));
                out.push_str("\" rel=\"noopener noreferrer\">");
                out.push_str(&escape_html(&item.title));
                out.push_str("</a>");
            } else {
                out.push_str(&escape_html(&item.title));
            }
            out.push_str("</h2>\n");
            if item.link.is_some() {
                out.push_str(
                    "<span class=\"split-hint\" aria-hidden=\"true\" title=\"Open in Split View\">",
                );
                out.push_str(SPLIT_ICON_SVG);
                out.push_str("</span>\n");
            }
            out.push_str("</div>\n");

            if item.published_display.is_some() || item.author.is_some() {
                out.push_str("<div class=\"item-meta\">");
                if let Some(date) = &item.published_display {
                    out.push_str("<time>");
                    out.push_str(&escape_html(date));
                    out.push_str("</time>");
                }
                if let Some(author) = &item.author {
                    if item.published_display.is_some() {
                        out.push_str("<span class=\"sep\">&middot;</span>");
                    }
                    out.push_str("<span>");
                    out.push_str(&escape_html(author));
                    out.push_str("</span>");
                }
                out.push_str("</div>\n");
            }
            out.push_str("</div>\n");

            if let Some(content_html) = &item.content_html {
                out.push_str("<div class=\"item-content\" data-expanded=\"false\">\n");
                out.push_str("<div class=\"prose\">");
                out.push_str(content_html);
                out.push_str("</div>\n<div class=\"content-fade\" aria-hidden=\"true\"></div>\n");
                out.push_str("</div>\n<button class=\"expand-btn\" type=\"button\" aria-expanded=\"false\">Show more</button>\n");
            } else if let Some(summary) = &item.summary_text {
                out.push_str("<p class=\"item-summary\">");
                out.push_str(&escape_html(summary));
                out.push_str("</p>\n");
            }

            if !item.categories.is_empty() {
                out.push_str("<div class=\"tags\">");
                for cat in &item.categories {
                    out.push_str("<span class=\"tag\">");
                    out.push_str(&escape_html(cat));
                    out.push_str("</span>");
                }
                out.push_str("</div>\n");
            }

            out.push_str("</article>\n</li>\n");
        }
        out.push_str("</ol>\n");
    }
    out.push_str("</main>\n");

    // Raw source view (hidden by default)
    out.push_str("<section id=\"raw-view\" class=\"raw-view\" hidden>\n<pre><code>");
    out.push_str(&escape_html(raw_source));
    out.push_str("</code></pre>\n</section>\n");

    out.push_str("</div>\n<script>\n");
    out.push_str(VIEWER_JS);
    out.push_str("\n</script>\n</body>\n</html>\n");
    out
}

const RSS_ICON_SVG: &str = r##"<svg viewBox="0 0 24 24" width="22" height="22" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M4 11a9 9 0 0 1 9 9"/><path d="M4 4a16 16 0 0 1 16 16"/><circle cx="5" cy="19" r="1.5" fill="currentColor" stroke="none"/></svg>"##;

const SPLIT_ICON_SVG: &str = r##"<svg viewBox="0 0 20 20" width="16" height="16" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round"><rect x="2.5" y="3.5" width="15" height="13" rx="2.5"/><line x1="10" y1="3.5" x2="10" y2="16.5"/></svg>"##;

const VIEWER_CSS: &str = r#"
:root {
  --ivory: #F5F2EC;
  --ink: #182432;
  --midnight: #101C2B;
  --blue: #034BF4;
  --blue-hover: #033DD0;

  --page: var(--ivory);
  --panel: #FCFBF8;
  --panel-hover: #FFFFFF;
  --border: rgba(24, 36, 50, 0.09);
  --border-hover: rgba(3, 75, 244, 0.28);
  --heading: var(--ink);
  --text: var(--ink);
  --quiet: #54616D;
  --link-solid: var(--blue);
  --accent-soft: rgba(3, 75, 244, 0.1);
  --radius: 16px;
}
@media (prefers-color-scheme: dark) {
  :root {
    --page: #0B1420;
    --panel: var(--midnight);
    --panel-hover: #152438;
    --border: rgba(245, 242, 236, 0.09);
    --border-hover: rgba(143, 177, 255, 0.32);
    --heading: var(--ivory);
    --text: #D2D9E1;
    --quiet: #B1BCC9;
    --link-solid: #8FB1FF;
    --accent-soft: rgba(143, 177, 255, 0.14);
  }
}
* { box-sizing: border-box; }
body {
  margin: 0;
  background: var(--page);
  color: var(--text);
  font: 15px/1.6 'Outfit', -apple-system, BlinkMacSystemFont, 'SF Pro Text', system-ui, sans-serif;
  -webkit-font-smoothing: antialiased;
}
.shell {
  max-width: 760px;
  margin: 0 auto;
  padding: 40px 24px 72px;
  animation: feed-slide-in 320ms cubic-bezier(0.22, 1, 0.36, 1) both;
}
@keyframes feed-slide-in {
  from {
    opacity: 0;
    transform: translateX(32px);
  }
  to {
    opacity: 1;
    transform: translateX(0);
  }
}
.feed-header {
  margin-bottom: 18px;
}
.feed-back-row {
  margin-bottom: 12px;
}
.feed-back-row[hidden] { display: none; }
.btn-back {
  display: inline-flex;
  align-items: center;
  gap: 5px;
  padding: 2px 0;
  border: 0;
  background: transparent;
  color: var(--quiet);
  font: inherit;
  font-size: 14px;
  font-weight: 500;
  cursor: pointer;
  transition: color 150ms ease;
}
.btn-back:hover {
  color: var(--text);
}
.btn-back:focus-visible {
  outline: 0.1875rem solid var(--link-solid);
  outline-offset: 3px;
  border-radius: 4px;
}
.hero-card {
  position: relative;
  border-radius: 16px;
  border: 1px solid rgba(255, 255, 255, 0.14);
  background:
    radial-gradient(280px 160px at 8% 12%, rgba(255, 102, 178, 0.14), transparent 70%),
    radial-gradient(260px 160px at 26% 0%, rgba(47, 166, 255, 0.14), transparent 72%),
    var(--panel);
}
@media (prefers-color-scheme: light) {
  .hero-card {
    border-color: var(--border);
    background:
      radial-gradient(280px 160px at 8% 12%, rgba(255, 102, 178, 0.10), transparent 70%),
      radial-gradient(260px 160px at 26% 0%, rgba(47, 166, 255, 0.10), transparent 72%),
      var(--panel);
  }
}
.hero-top {
  display: flex;
  align-items: flex-start;
  gap: 16px;
  padding: 20px 20px 18px;
}
.hero-actions {
  flex: none;
  display: flex;
  align-items: center;
  gap: 4px;
}
.hero-foot {
  display: flex;
  flex-wrap: wrap;
  align-items: center;
  justify-content: space-between;
  gap: 12px;
  padding: 10px 20px;
  border-top: 1px solid var(--border);
  border-radius: 0 0 15px 15px;
  background: rgba(0, 0, 0, 0.14);
}
@media (prefers-color-scheme: light) {
  .hero-foot {
    background: rgba(15, 23, 42, 0.025);
  }
}
.feed-logo {
  position: relative;
  width: 60px;
  height: 60px;
  padding: 0;
  border: 0;
  border-radius: 14px;
  background: transparent;
  flex-shrink: 0;
  cursor: pointer;
  display: inline-flex;
  align-items: center;
  justify-content: center;
}
.feed-logo:focus-visible {
  outline: 0.1875rem solid var(--link-solid);
  outline-offset: 2px;
}
.feed-logo-fallback {
  width: 48px;
  height: 48px;
  border-radius: 14px;
  display: inline-flex;
  align-items: center;
  justify-content: center;
  background: var(--accent-soft);
  color: var(--link-solid);
}
.feed-logo.is-live .feed-logo-fallback {
  display: none;
}
.feed-logo canvas {
  position: absolute;
  inset: 0;
  width: 100%;
  height: 100%;
  display: block;
  pointer-events: none;
}
.feed-heading { min-width: 0; flex: 1; }
h1 {
  margin: 0;
  color: var(--heading);
  font-size: 26px;
  line-height: 1.16;
  letter-spacing: -0.025em;
}
.feed-subtitle {
  margin: 6px 0 0;
  color: var(--quiet);
  font-size: 14.5px;
  line-height: 1.45;
}
.feed-meta {
  display: flex;
  flex-wrap: wrap;
  gap: 7px;
  align-items: center;
  font-size: 14px;
  color: var(--quiet);
}
.meta-dot {
  opacity: 0.45;
}
.site-link {
  color: var(--link-solid);
  opacity: 0.9;
  text-decoration: none;
}
.site-link:hover, .site-link:focus-visible {
  opacity: 1;
  text-decoration: underline;
  text-underline-offset: 0.2rem;
}
.filter-wrap {
  position: relative;
  width: 220px;
  flex-shrink: 0;
}
.filter-icon {
  position: absolute;
  left: 10px;
  top: 50%;
  transform: translateY(-50%);
  color: var(--quiet);
  pointer-events: none;
}
.filter-input {
  width: 100%;
  box-sizing: border-box;
  height: 30px;
  padding: 0 10px 0 30px;
  border-radius: 7px;
  border: 1px solid rgba(255, 255, 255, 0.09);
  background: rgba(0, 0, 0, 0.28);
  color: var(--text);
  font: inherit;
  font-size: 14px;
}
@media (prefers-color-scheme: light) {
  .filter-input {
    border-color: rgba(15, 23, 42, 0.10);
    background: rgba(15, 23, 42, 0.04);
  }
}
@media (max-width: 560px) {
  .filter-wrap {
    width: 100%;
  }
}
.filter-input::placeholder { color: var(--quiet); opacity: 0.75; }
.filter-input:focus-visible {
  outline: 0.1875rem solid var(--link-solid);
  outline-offset: 2px;
}
.btn {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  height: 30px;
  min-width: 72px;
  padding: 0 13px;
  border-radius: 7px;
  border: 1px solid rgba(255, 255, 255, 0.16);
  background: rgba(255, 255, 255, 0.09);
  color: var(--text);
  font: inherit;
  font-size: 14px;
  font-weight: 500;
  cursor: pointer;
  transition: background 150ms ease, border-color 150ms ease, transform 180ms ease-out;
}
@media (prefers-color-scheme: light) {
  .btn {
    border-color: var(--border);
    background: var(--panel);
  }
}
.btn:hover { background: var(--panel-hover); border-color: var(--border-hover); }
.btn:focus-visible {
  outline: 0.1875rem solid var(--link-solid);
  outline-offset: 2px;
}
.btn[aria-pressed="true"] {
  border-color: var(--link-solid);
  color: var(--link-solid);
}
.btn-primary {
  background: #0A84FF;
  border-color: rgba(255, 255, 255, 0.22);
  color: #FFFFFF;
  font-weight: 600;
}
.btn-primary:hover {
  background: #1B90FF;
  border-color: rgba(255, 255, 255, 0.28);
}
.btn-icon {
  min-width: 32px;
  width: 32px;
  height: 32px;
  padding: 0;
  border-color: transparent;
  background: transparent;
  color: var(--quiet);
}
.btn-icon:hover {
  border-color: transparent;
  background: rgba(255, 255, 255, 0.08);
  color: var(--text);
}
@media (prefers-color-scheme: light) {
  .btn-icon {
    border-color: transparent;
    background: transparent;
  }
  .btn-icon:hover {
    border-color: transparent;
    background: rgba(15, 23, 42, 0.06);
  }
}
.menu-wrap {
  position: relative;
  flex: none;
}
.menu-popover {
  position: absolute;
  top: calc(100% + 6px);
  right: 0;
  z-index: 20;
  min-width: 176px;
  padding: 6px;
  border-radius: 12px;
  border: 1px solid rgba(255, 255, 255, 0.20);
  background: rgba(24, 24, 29, 0.88);
  backdrop-filter: blur(28px) saturate(160%);
  -webkit-backdrop-filter: blur(28px) saturate(160%);
  box-shadow: 0 14px 36px rgba(0, 0, 0, 0.45);
  display: flex;
  flex-direction: column;
  gap: 2px;
}
.menu-popover[hidden] { display: none; }
@media (prefers-color-scheme: light) {
  .menu-popover {
    border-color: rgba(0, 0, 0, 0.14);
    background: rgba(250, 250, 253, 0.90);
    box-shadow: 0 12px 30px rgba(0, 0, 0, 0.14);
  }
}
.menu-item {
  display: flex;
  align-items: center;
  justify-content: space-between;
  width: 100%;
  min-height: 34px;
  padding: 0 12px;
  border: 0;
  border-radius: 7px;
  background: transparent;
  color: var(--text);
  font: inherit;
  font-size: 14px;
  font-weight: 500;
  text-align: left;
  cursor: pointer;
}
.menu-item:hover, .menu-item:focus-visible {
  background: rgba(255, 255, 255, 0.10);
  outline: none;
}
@media (prefers-color-scheme: light) {
  .menu-item:hover, .menu-item:focus-visible {
    background: rgba(0, 0, 0, 0.06);
  }
}
.menu-item[aria-pressed="true"] {
  color: var(--link-solid);
}
@media (hover: hover) and (pointer: fine) {
  .btn-primary:hover { transform: scale(1.02); }
}
.item-list {
  list-style: none;
  margin: 0;
  padding: 0;
  display: flex;
  flex-direction: column;
  gap: 12px;
}
.item {
  background: var(--panel);
  border: 1px solid var(--border);
  border-radius: var(--radius);
  padding: 18px 20px;
  transition: background 150ms ease, border-color 150ms ease;
}
.item-clickable { cursor: pointer; }
.item-clickable:hover {
  background: var(--panel-hover);
  border-color: var(--border-hover);
}
.item-head {
  display: flex;
  flex-direction: column;
  gap: 4px;
}
.item-title-row {
  display: flex;
  align-items: flex-start;
  justify-content: space-between;
  gap: 12px;
}
.item h2 {
  margin: 0;
  color: var(--heading);
  font-size: 17px;
  line-height: 1.35;
  font-weight: 600;
  letter-spacing: -0.01em;
}
.item h2 a {
  color: inherit;
  text-decoration: none;
}
.item-clickable:hover h2 a,
.item h2 a:focus-visible {
  color: var(--link-solid);
}
.item h2 a:focus-visible {
  outline: 0.1875rem solid var(--link-solid);
  outline-offset: 3px;
  border-radius: 4px;
}
.split-hint {
  flex-shrink: 0;
  margin-top: 3px;
  color: var(--quiet);
  opacity: 0;
  transition: opacity 150ms ease, color 150ms ease;
}
.item-clickable:hover .split-hint,
.item-clickable:focus-within .split-hint {
  opacity: 0.85;
  color: var(--link-solid);
}
.item-meta {
  font-size: 12.5px;
  color: var(--quiet);
  display: flex;
  gap: 6px;
  align-items: center;
}
.item-summary {
  margin: 8px 0 0;
  color: var(--quiet);
  font-size: 14.5px;
  line-height: 1.55;
}
.tags {
  display: flex;
  flex-wrap: wrap;
  gap: 6px;
  margin-top: 10px;
}
.tag {
  font-size: 11.5px;
  padding: 2px 9px;
  border-radius: 999px;
  border: 1px solid var(--border);
  color: var(--quiet);
}
.item-content {
  position: relative;
  margin-top: 10px;
  max-height: 152px;
  overflow: hidden;
  --card-bg: var(--panel);
}
.item-clickable:hover .item-content {
  --card-bg: var(--panel-hover);
}
.item-content[data-expanded="true"],
.item-content[data-fits="true"] {
  max-height: none;
  overflow: visible;
}
.content-fade {
  position: absolute;
  left: 0;
  right: 0;
  bottom: 0;
  height: 64px;
  pointer-events: none;
  background: linear-gradient(to bottom, transparent, var(--card-bg) 88%);
}
.item-content[data-expanded="true"] .content-fade,
.item-content[data-fits="true"] .content-fade {
  display: none;
}
.expand-btn {
  margin-top: 10px;
  display: inline-flex;
  align-items: center;
  padding: 5px 12px;
  border-radius: 999px;
  border: 1px solid var(--border);
  background: var(--accent-soft);
  color: var(--link-solid);
  font: inherit;
  font-size: 12.5px;
  font-weight: 500;
  cursor: pointer;
  transition: background 150ms ease, border-color 150ms ease;
}
.expand-btn:hover {
  border-color: var(--border-hover);
}
.expand-btn:focus-visible {
  outline: 0.1875rem solid var(--link-solid);
  outline-offset: 2px;
}
.prose {
  font-size: 14.5px;
  line-height: 1.65;
  color: var(--text);
  overflow-wrap: anywhere;
}
.prose > :first-child { margin-top: 0; }
.prose > :last-child { margin-bottom: 0; }
.prose img { max-width: 100%; height: auto; border-radius: 10px; }
.prose pre {
  padding: 12px;
  border-radius: 10px;
  background: var(--page);
  overflow-x: auto;
  font-size: 13px;
}
.prose a {
  color: var(--link-solid);
  text-underline-offset: 0.125rem;
}
.raw-view pre {
  margin: 0;
  padding: 18px;
  border-radius: var(--radius);
  border: 1px solid var(--border);
  background: var(--panel);
  color: var(--text);
  overflow-x: auto;
  font: 12.5px/1.55 ui-monospace, SFMono-Regular, Menlo, monospace;
  white-space: pre-wrap;
  word-break: break-word;
}
.empty { color: var(--quiet); text-align: center; padding: 48px 0; }
@media (prefers-reduced-motion: reduce) {
  .shell { animation: none; }
  .btn, .item, .split-hint { transition: none; }
  .btn-primary:hover { transform: none; }
}
"#;

const VIEWER_JS: &str = r#"
const filterInput = document.getElementById('filter')
const feedView = document.getElementById('feed-view')
const rawView = document.getElementById('raw-view')
const toggleRaw = document.getElementById('toggle-raw')
const copyUrl = document.getElementById('copy-url')
const subscribeBtn = document.getElementById('subscribe-btn')

if (subscribeBtn) {
  subscribeBtn.addEventListener('click', () => {
    const subscribed = subscribeBtn.getAttribute('aria-pressed') === 'true'
    const next = !subscribed
    subscribeBtn.setAttribute('aria-pressed', String(next))
    subscribeBtn.textContent = next ? 'Subscribed' : 'Subscribe'
    subscribeBtn.classList.toggle('btn-primary', !next)
    if (window.location.protocol !== 'file:') {
      window.location.hash = next ? '#myarc-subscribe' : '#myarc-unsubscribe'
      history.replaceState(null, '', window.location.pathname + window.location.search)
    }
  })
}

const openArticleInSplit = (url) => {
  if (window.location.protocol === 'file:') {
    window.open(url, '_blank', 'noopener,noreferrer')
    return
  }
  window.location.assign(url)
}

if (filterInput) {
  const rows = Array.from(document.querySelectorAll('.item'))
  filterInput.addEventListener('input', () => {
    const q = filterInput.value.trim().toLowerCase()
    for (const row of rows) {
      const match = !q || row.textContent.toLowerCase().includes(q)
      row.hidden = !match
    }
  })
}

for (const card of document.querySelectorAll('.item-clickable')) {
  card.addEventListener('click', (event) => {
    if (event.metaKey || event.ctrlKey || event.shiftKey || event.altKey || event.button !== 0) {
      return
    }
    if (event.target.closest('.expand-btn, .prose a, button')) {
      return
    }
    const url = card.dataset.href
    if (!url) return
    event.preventDefault()
    openArticleInSplit(url)
  })
}

for (const btn of document.querySelectorAll('.expand-btn')) {
  const box = btn.previousElementSibling
  if (!box || !box.classList.contains('item-content')) continue
  if (box.scrollHeight <= box.clientHeight + 8) {
    box.dataset.fits = 'true'
    btn.hidden = true
    continue
  }
  btn.addEventListener('click', (event) => {
    event.stopPropagation()
    const expanded = box.dataset.expanded === 'true'
    box.dataset.expanded = String(!expanded)
    btn.setAttribute('aria-expanded', String(!expanded))
    btn.textContent = expanded ? 'Show more' : 'Show less'
  })
}

const backRow = document.getElementById('back-row')
const backBtn = document.getElementById('back-to-page')
if (backRow && backBtn && window.history.length > 1) {
  backRow.hidden = false
  backBtn.addEventListener('click', () => {
    window.location.hash = 'myarc-feed-back'
  })
}

const moreMenuBtn = document.getElementById('more-menu-btn')
const moreMenu = document.getElementById('more-menu')
const closeMoreMenu = () => {
  if (!moreMenu || !moreMenuBtn) return
  moreMenu.hidden = true
  moreMenuBtn.setAttribute('aria-expanded', 'false')
}

if (moreMenuBtn && moreMenu) {
  moreMenuBtn.addEventListener('click', (event) => {
    event.stopPropagation()
    const nextOpen = moreMenu.hidden
    moreMenu.hidden = !nextOpen
    moreMenuBtn.setAttribute('aria-expanded', String(nextOpen))
  })
  document.addEventListener('click', (event) => {
    if (!moreMenu.hidden && !moreMenu.contains(event.target) && event.target !== moreMenuBtn) {
      closeMoreMenu()
    }
  })
  document.addEventListener('keydown', (event) => {
    if (event.key === 'Escape' && !moreMenu.hidden) {
      closeMoreMenu()
      moreMenuBtn.focus()
    }
  })
}

if (toggleRaw && feedView && rawView) {
  toggleRaw.addEventListener('click', () => {
    const showingRaw = !rawView.hidden
    rawView.hidden = showingRaw
    feedView.hidden = !showingRaw
    toggleRaw.setAttribute('aria-pressed', String(!showingRaw))
    closeMoreMenu()
  })
}

if (copyUrl) {
  copyUrl.addEventListener('click', async () => {
    const url = copyUrl.dataset.url || window.location.href
    try {
      await navigator.clipboard.writeText(url)
      const prev = copyUrl.textContent
      copyUrl.textContent = 'Copied'
      setTimeout(() => {
        copyUrl.textContent = prev
        closeMoreMenu()
      }, 900)
    } catch {
      copyUrl.textContent = 'Copy failed'
    }
  })
}

const mountRssLogo = (button, canvas) => {
  const gl = canvas.getContext('webgl2', { alpha: true, antialias: true, preserveDrawingBuffer: true })
  if (!gl) return null

  const still = matchMedia('(prefers-reduced-motion: reduce)').matches
  const add = (a, b, s = 1) => [a[0] + b[0] * s, a[1] + b[1] * s, a[2] + b[2] * s]
  const sub = (a, b) => add(a, b, -1)
  const norm = (a) => { const l = Math.hypot(a[0], a[1], a[2]) || 1; return [a[0] / l, a[1] / l, a[2] / l] }
  const cross = (a, b) => [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]]
  const brandPal = [[1, .40, .70], [.90, .36, .86], [.70, .36, .98], [.46, .40, 1], [.28, .52, 1], [.12, .72, 1], [.35, .84, .98], [.75, .90, 1]]

  const tube = (pts, a, b, closed, scales, face, pal, kind, tmin = 0, tmax = 1) => {
    const rings = [], n = pts.length
    const frame = (i) => {
      const p0 = pts[closed ? (i + n - 1) % n : Math.max(0, i - 1)]
      const p1 = pts[closed ? (i + 1) % n : Math.min(n - 1, i + 1)]
      const t = norm(sub(p1, p0)), B = norm(cross(t, face)), N = cross(B, t)
      return {
        t, B, N, c: pts[i],
        a: a * (scales?.[i]?.[0] ?? 1),
        b: b * (scales?.[i]?.[1] ?? 1),
        s: 1, k: 0,
        u: tmin + (tmax - tmin) * (i / Math.max(1, n - 1)),
      }
    }
    const cap = (i, sign, u) => Array.from({ length: 10 }, (_, j) => {
      const f = frame(i), q = (j + 1) * Math.PI / 20
      return { ...f, u, c: add(f.c, f.t, sign * f.a * Math.sin(q)), s: Math.cos(q), k: sign * Math.sin(q) }
    })
    if (!closed) rings.push(...cap(0, -1, tmin).reverse())
    for (let i = 0; i < n + (closed ? 1 : 0); i++) rings.push(frame(i % n))
    if (!closed) rings.push(...cap(n - 1, 1, tmax))
    const v = [], idx = []
    rings.forEach((r, i) => {
      for (let j = 0; j <= 36; j++) {
        const q = j * Math.PI / 18, cs = Math.cos(q), sn = Math.sin(q)
        const p = add(add(r.c, r.N, r.a * cs * r.s), r.B, r.b * sn * r.s)
        const nr = norm(add(norm(add(r.N.map((x) => x * r.b * cs), r.B, r.a * sn)).map((x) => x * Math.sqrt(1 - r.k * r.k)), r.t, r.k))
        v.push(...p, ...nr, r.u)
        if (i < rings.length - 1 && j < 36) {
          const k = i * 37 + j
          idx.push(k, k + 1, k + 37, k + 37, k + 1, k + 38)
        }
      }
    })
    return { v, idx, pal, kind }
  }

  const sphere = (c, r, pal, kind, t = .05) => {
    const v = [], idx = []
    for (let i = 0; i <= 28; i++) for (let j = 0; j <= 40; j++) {
      const a = i * Math.PI / 28, b = j * Math.PI / 20
      const n = [Math.sin(a) * Math.cos(b), Math.cos(a), Math.sin(a) * Math.sin(b)]
      v.push(...add(c, n, r), ...n, t)
      if (i < 28 && j < 40) {
        const k = i * 41 + j
        idx.push(k, k + 1, k + 41, k + 41, k + 1, k + 42)
      }
    }
    return { v, idx, pal, kind }
  }

  const arcPoints = (radius, start, end, cx, cy, z, segments = 72) =>
    Array.from({ length: segments + 1 }, (_, i) => {
      const t = start + (end - start) * (i / segments)
      return [cx + radius * Math.cos(t), cy + radius * Math.sin(t), z]
    })

  const VS = '#version 300 es\n' +
    'in vec3 p,n;in float u;uniform mat3 rot;uniform float scale;out vec3 P,N,O;out float U;' +
    'void main(){O=p;U=u;P=rot*(p*scale);N=rot*n;float d=5.-P.z;gl_Position=vec4(P.xy*2.85,(d-3.)/6.*d,d);}'
  const FS = '#version 300 es\nprecision highp float;\n' +
    'in vec3 P,N,O;in float U;uniform vec3 pal[8];uniform float kind;uniform float alphaMul;out vec4 outColor;' +
    'vec3 palSample(float t){t=clamp(t,0.,1.)*7.;int i=int(min(floor(t),6.));return mix(pal[i],pal[i+1],smoothstep(0.,1.,t-float(i)));}' +
    'void main(){' +
    '  vec3 base=palSample(U);' +
    '  vec3 n=normalize(N),v=normalize(vec3(0,0,5)-P),L=normalize(vec3(-.45,.8,.65));' +
    '  float nv=max(dot(n,v),0.),d=dot(n,L)*.5+.5,fr=pow(1.-nv,2.2);' +
    '  vec3 col=base*(.56+.58*d*d);' +
    '  col=mix(col,base*base*1.1,(1.-d)*.35);' +
    '  col=mix(col,mix(base,vec3(1),.74),fr*.64);' +
    '  col+=pow(max(dot(n,normalize(L+v)),0.),28.)*.16;' +
    '  if(kind>.5){' +
    '    float edge=pow(1.-abs(dot(n,v)),2.4);' +
    '    float spec=pow(max(dot(n,normalize(L+v)),0.),56.);' +
    '    vec3 glass=mix(base,vec3(.98,.985,1.),.34+edge*.22);' +
    '    outColor=vec4(glass+spec*.35, clamp(alphaMul*(.20+edge*.42+spec*.24),0.,.96));' +
    '  } else {' +
    '    outColor=vec4(col,1.);' +
    '  }' +
    '}'

  const program = gl.createProgram()
  for (const [type, src] of [[gl.VERTEX_SHADER, VS], [gl.FRAGMENT_SHADER, FS]]) {
    const s = gl.createShader(type)
    gl.shaderSource(s, src)
    gl.compileShader(s)
    if (!gl.getShaderParameter(s, gl.COMPILE_STATUS)) return null
    gl.attachShader(program, s)
  }
  gl.linkProgram(program)
  if (!gl.getProgramParameter(program, gl.LINK_STATUS)) return null
  gl.useProgram(program)

  const center = [-.55, -.58, 0]
  const innerPts = arcPoints(.76, 0, Math.PI / 2, center[0], center[1], 0, 64)
  const outerPts = arcPoints(1.42, 0, Math.PI / 2, center[0], center[1], 0, 72)

  const objects = [
    sphere(center, .24, brandPal, 1, .02),
    tube(innerPts, .18, .18, false, null, [0, 0, 1], brandPal, 1, .12, .55),
    tube(outerPts, .20, .20, false, null, [0, 0, 1], brandPal, 1, .36, 1.0),
  ]

  const meshes = objects.map((m) => {
    const vao = gl.createVertexArray()
    gl.bindVertexArray(vao)
    gl.bindBuffer(gl.ARRAY_BUFFER, gl.createBuffer())
    gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(m.v), gl.STATIC_DRAW)
    for (const [name, size, off] of [['p', 3, 0], ['n', 3, 12], ['u', 1, 24]]) {
      const loc = gl.getAttribLocation(program, name)
      gl.enableVertexAttribArray(loc)
      gl.vertexAttribPointer(loc, size, gl.FLOAT, false, 28, off)
    }
    gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, gl.createBuffer())
    gl.bufferData(gl.ELEMENT_ARRAY_BUFFER, new Uint16Array(m.idx), gl.STATIC_DRAW)
    return { ...m, vao }
  })

  const ux = gl.getUniformLocation(program, 'rot')
  const us = gl.getUniformLocation(program, 'scale')
  const up = gl.getUniformLocation(program, 'pal[0]')
  const uk = gl.getUniformLocation(program, 'kind')
  const ua = gl.getUniformLocation(program, 'alphaMul')
  const palData = new Float32Array(brandPal.flat())

  const restX = 0, restY = 0, alpha = .90
  let curX = restX, curY = restY, curScale = 1, raf = 0, anim = null, mode = 0

  const render = () => {
    const dpr = Math.min((devicePixelRatio || 1) * 2, 4)
    const size = Math.round(canvas.clientWidth * dpr)
    if (!size) return
    if (canvas.width !== size || canvas.height !== size) canvas.width = canvas.height = size
    gl.viewport(0, 0, size, size)
    gl.clearColor(0, 0, 0, 0)
    gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT)
    gl.enable(gl.DEPTH_TEST)
    const c = Math.cos(curY), s = Math.sin(curY), a = Math.cos(curX), b = Math.sin(curX)
    gl.useProgram(program)
    gl.uniformMatrix3fv(ux, false, [c, b * s, -a * s, 0, a, b, s, -b * c, a * c])
    gl.uniform1f(us, curScale)
    gl.uniform3fv(up, palData)
    gl.uniform1f(ua, alpha)
    gl.enable(gl.BLEND)
    gl.blendFuncSeparate(gl.SRC_ALPHA, gl.ONE_MINUS_SRC_ALPHA, gl.ONE, gl.ONE_MINUS_SRC_ALPHA)
    gl.depthMask(false)
    gl.enable(gl.CULL_FACE)
    for (const m of meshes) {
      gl.uniform1f(uk, m.kind)
      gl.bindVertexArray(m.vao)
      gl.cullFace(gl.FRONT)
      gl.drawElements(gl.TRIANGLES, m.idx.length, gl.UNSIGNED_SHORT, 0)
      gl.cullFace(gl.BACK)
      gl.drawElements(gl.TRIANGLES, m.idx.length, gl.UNSIGNED_SHORT, 0)
    }
    gl.disable(gl.CULL_FACE)
    gl.depthMask(true)
  }

  const easeOutBack = (t) => 1 + 2.1 * Math.pow(t - 1, 3) + 1.1 * Math.pow(t - 1, 2)
  const easeInOut = (t) => t < .5 ? 4 * t * t * t : 1 - Math.pow(-2 * t + 2, 3) / 2

  const tick = (now) => {
    raf = 0
    if (!anim) return
    const p = Math.min((now - anim.t0) / anim.dur, 1)
    if (anim.kind === 0) {
      curY = restY + Math.PI * 2 * easeOutBack(p)
      curX = restX + .28 * Math.sin(Math.PI * p)
      curScale = 1 + .08 * Math.sin(Math.PI * p)
    } else {
      curY = restY - Math.PI * 2 * easeInOut(p)
      curX = restX + Math.PI * 2 * easeInOut(p) * .25 * Math.sin(Math.PI * p)
      curScale = 1 + .12 * Math.sin(Math.PI * 2 * p) * (1 - p)
    }
    render()
    if (p < 1) {
      raf = requestAnimationFrame(tick)
    } else {
      anim = null
      curX = restX
      curY = restY
      curScale = 1
      render()
    }
  }

  const play = () => {
    if (still) return
    anim = { kind: mode++ % 2, t0: performance.now(), dur: 980 }
    if (!raf) raf = requestAnimationFrame(tick)
  }

  button.addEventListener('click', play)
  new ResizeObserver(render).observe(canvas)
  render()
  return { play }
}

const logoBtn = document.getElementById('rss-logo')
const logoCanvas = document.getElementById('rss-logo-canvas')
if (logoBtn && logoCanvas) {
  try {
    if (mountRssLogo(logoBtn, logoCanvas)) logoBtn.classList.add('is-live')
  } catch {
    // Keep SVG fallback when WebGL2 is unavailable.
  }
}
"#;
