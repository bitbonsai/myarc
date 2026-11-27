use std::env;
use std::fs;
use std::io::{self, Read};
use std::process;

use myarc_feeds::{RenderOptions, parse_feed, render_viewer_html};

fn main() {
    let mut args = env::args().skip(1);
    let mut input_path: Option<String> = None;
    let mut request_url: Option<String> = None;
    let mut show_subscribe_slot = false;

    while let Some(arg) = args.next() {
        match arg.as_str() {
            "--url" => request_url = args.next(),
            "--subscribe" => show_subscribe_slot = true,
            _ if input_path.is_none() => input_path = Some(arg),
            _ => {
                eprintln!("Usage: preview [feed-file] [--url <feed-url>] [--subscribe]");
                process::exit(2);
            }
        }
    }

    let bytes = match input_path {
        Some(path) => fs::read(&path).unwrap_or_else(|err| {
            eprintln!("Failed to read {path}: {err}");
            process::exit(1);
        }),
        None => {
            let mut buf = Vec::new();
            io::stdin().read_to_end(&mut buf).unwrap_or_else(|err| {
                eprintln!("Failed to read stdin: {err}");
                process::exit(1);
            });
            buf
        }
    };

    let feed = parse_feed(&bytes, request_url.as_deref()).unwrap_or_else(|err| {
        eprintln!("Failed to parse feed: {err}");
        process::exit(1);
    });
    let raw = String::from_utf8_lossy(&bytes);
    let html = render_viewer_html(
        &feed,
        &raw,
        &RenderOptions {
            show_subscribe_slot,
            is_subscribed: false,
        },
    );
    print!("{html}");
}
