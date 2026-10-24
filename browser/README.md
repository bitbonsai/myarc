# M0 foundation experiment

Status: unmodified Chromium built and launched; a Rust-owned rename command works through a native toolbar test control. The integration builds and passes its core/bridge checks on two upstream revisions. Foundation acceptance remains open. [upstream.json](upstream.json) pins the feasibility candidate and patch; [evidence](evidence/foundation.json) records verified scope and outstanding checks. This development build is not a myarc release.

## Bounded outcomes

MA-002.a completed the unmodified `chrome` build and disposable-profile launch. MA-002.b proves one transient Rust workspace command through Cxx and a native Views control. MA-002.c reapplies that patch and runs the integrated checks on a second revision. These bounded slices leave the full [MA-002 gate](../plan/EXECUTION.md) open for extension lifecycle, isolation/recovery, licensing and maintenance evidence.

MA-001.a has an independent [onboarding motion study](../design/onboarding/README.md). It cannot substitute for native build evidence.

## Local build recipe

Reviewed upstream instructions at `7ae71de816ba32868ff9f974c2716f04ebd9b860`: [Mac build](https://chromium.googlesource.com/chromium/src/+/7ae71de816ba32868ff9f974c2716f04ebd9b860/docs/mac_build_instructions.md), [Apple Silicon](https://chromium.googlesource.com/chromium/src/+/7ae71de816ba32868ff9f974c2716f04ebd9b860/docs/mac_arm64.md), and [Rust FFI](https://chromium.googlesource.com/chromium/src/+/7ae71de816ba32868ff9f974c2716f04ebd9b860/docs/rust/ffi.md).

Prerequisites: Apple Silicon Mac, APFS volume, Xcode with accepted license/first-launch setup and the Metal Toolchain component, Git, Python and `git-lfs` when LFS filters are configured. The candidate's official SDK setting is 27.0. Install Apple's missing Metal component with `xcodebuild -downloadComponent MetalToolchain`; verify `xcrun --find metal`. Use Chromium's fetched toolchains for the integrated build rather than substituting system Rust.

Run from the myarc repository root. Choose a new external checkout path with no spaces. These commands acquire large dependencies and run upstream hooks. Keep local logs, profiles and source outside this repository.

```sh
set -eu
REPO="$PWD"
WORK="$HOME/.cache/myarc-m0"
REV=7ae71de816ba32868ff9f974c2716f04ebd9b860

mkdir -p "$WORK"
git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git "$WORK/depot_tools"
git -C "$WORK/depot_tools" checkout dbc99f8b5b88e43a2b907d8ddd93c269d24c287e
export PATH="$PATH:$WORK/depot_tools"
export DEPOT_TOOLS_UPDATE=0
export DEPOT_TOOLS_METRICS=0
"$WORK/depot_tools/ensure_bootstrap" > "$WORK/bootstrap.log" 2>&1

mkdir -p "$WORK/chromium"
cd "$WORK/chromium"
gclient config --name src https://chromium.googlesource.com/chromium/src.git
# LiteRT's mirror has no LFS service. The macOS GN build uses its sources,
# not its prebuilt libraries. Keep this setting scoped to dependency checkout.
GIT_LFS_SKIP_SMUDGE=1 gclient sync --no-history --nohooks --revision "src@$REV" --jobs 8 > "$WORK/sync.log" 2>&1
# Stop on failure. Resume with this same sync command after fixing its cause.
test "$(git -C src rev-parse HEAD)" = "$REV"
test "$(git -C src/third_party/litert/src rev-parse HEAD)" = "86ae38de889076209e2eb71d9ac43f243226cb3a"
gclient runhooks > "$WORK/hooks.log" 2>&1
cd src
mkdir -p out/MyarcM0
cp "$REPO/browser/args.gn" out/MyarcM0/args.gn
gn gen out/MyarcM0 > "$WORK/generate.log" 2>&1
autoninja -C out/MyarcM0 -j 8 chrome > "$WORK/build.log" 2>&1
```

Execute each stage only after the preceding command succeeds. The checked-in [args.gn](args.gn) requests an arm64 release component build without debug symbols or remote execution. It is a development configuration, not signing/update configuration.

Monitor free disk during checkout, hooks and compilation. The first local attempt used a 40 GiB free-space stop reserve, checked every five seconds; this is a local safety policy, not an upstream capacity guarantee. Do not delete unrelated files or disable browser security to finish a build. The successful resumed baseline segment took 2h 22m 37s. A later snapshot measured 37.6 GiB for source plus build output and 0.7 GiB for depot_tools. Fresh-build total time, peak storage/RAM and recurring runner cost remain unmeasured.

Once compilation succeeds, launch with a new disposable profile outside the repository:

```sh
out/MyarcM0/Chromium.app/Contents/MacOS/Chromium \
  --user-data-dir="$WORK/test-profile"
```

Do not point the experiment at an existing Chrome, Arc or Helium profile. Installation of an existing browser is not evidence that this source build works.

## Build the Rust/native experiment

From this repository, standalone checks need no Chromium checkout:

```sh
cargo fmt --all -- --check
rustfmt --check --edition 2024 browser/foundation/bridge.rs
cargo clippy --workspace --all-targets --locked -- -D warnings
cargo test --workspace --locked
python3 tools/check_foundation_patch.py
```

Cargo uses pinned Rust 1.96.1. Chromium GN uses its fetched toolchain. The same dependency-free [core source](../crates/workspace/src/lib.rs) is packaged in the patch; the freshness check rejects drift between that payload and the canonical repository files. The GitHub workflow covers standalone checks, not the external Chromium build.

After the unmodified build, use a clean external checkout at the pinned base. Preserve unrelated changes and existing branches. The patch includes the native integration, owned source files and MIT notice:

```sh
SRC="$WORK/chromium/src"
git -C "$SRC" config --local user.name bitbonsai
git -C "$SRC" config --local user.email 44185+bitbonsai@users.noreply.github.com
git -C "$SRC" switch -c myarc/m0-foundation "$REV"
git -C "$SRC" am "$REPO/browser/patches/0001-workspace-foundation.patch"
cd "$SRC"
gn gen out/MyarcM0
autoninja -C out/MyarcM0 -j 8 chrome myarc:bridge_check myarc:workspace_unittests
out/MyarcM0/bridge_check
out/MyarcM0/workspace_unittests
out/MyarcM0/Chromium.app/Contents/MacOS/Chromium \
  --user-data-dir="$WORK/native-test-profile" \
  --no-first-run --no-default-browser-check --myarc-foundation
```

The flag adds `Test Space: Everyday`. Activating it runs `rename_space` in Rust and changes the label to Research, then back on the next activation. Each toolbar owns one transient workspace; closing it discards that state. Browser Spaces, import and persistence remain unimplemented. The control is omitted without the flag.

For reapplication, select a new branch at `8d208f20744ac6d4ce00cc3f9de6a14a9b8b926d`, apply the same patch, then run dependency sync/hooks, GN generation and the build/check commands again. Keep the root checkout unmanaged during sync, as in the generated `.gclient`, so sync preserves the applied patch. Verify the checked-out commit and identity before compiling. The recorded run reused the output directory; it was an incremental rebuild, not a second fresh-output timing measurement. This immediate-child revision rolls a PHP test dependency and is a weak measure of future browser/security-update porting cost.

## Run record, 2026-10-24

- Host preflight: macOS 26.7, arm64, 48 GiB RAM, 15 logical CPUs, APFS, Xcode 27.0 (`27A266a`), macOS SDK 27.0. Initial available disk was approximately 220 GiB. Signing identities and credentials were not inspected.
- Candidate: Chromium `158.0.8122.0`, revision in the pin. Source checkout reached that exact revision.
- First sync ran from 06:20:05 to 06:30:05 UTC and failed in LiteRT checkout: `git-lfs: command not found`. No hooks, compilation or launch occurred in that attempt. Partial dependencies were retained.
- Installed Git LFS 3.8.1 and resumed the same pinned sync. Dependency sync and upstream hooks passed. First GN invocation failed because pinned depot_tools lacked bootstrapped Python (`python3_bin_reldir.txt not found`). Ran its supported `ensure_bootstrap` without updating the pin, then generation passed.
- First build scheduled at 06:36:24 UTC and failed before compilation because LiteRT's earlier failed checkout had no valid HEAD and lacked `tflite/converter/schema/schema.fbs`. A direct repair then reached the mirror's LFS endpoint, which returned HTTP 405. Restored only this generated dependency at its pinned revision with process-scoped `GIT_LFS_SKIP_SMUDGE=1`; its Git tree is clean, required source exists, and generated Ninja files contain no references to its prebuilt-library paths. No global LFS configuration or upstream source was changed.
- Second build completed 1,155 steps before ANGLE failed on missing Metal Toolchain. Apple's `xcodebuild -downloadComponent MetalToolchain` installed component `27A266a`; `xcrun --find metal` then succeeded. Incremental build resumed at 06:40 UTC with eight jobs.
- The resumed unmodified build passed at 09:03:03 UTC after starting at 06:40:26 UTC. Launched the resulting Chrome 158 binary and exercised a synthetic local form. A saved localStorage value survived reload and was absent from a separate user-data directory. This is a narrow smoke check, not browser-Profile/private-mode isolation evidence.
- Initial native integration passed six core tests under Cargo and GN, seven C++/Rust checks including 1,000 construction/rename/destruction cycles, and an accessibility click followed by focused Space-key activation of the toolbar control. Keyboard access needed explicit `FocusBehavior::ALWAYS`. No sanitizer or cross-thread violation test was run.
- The packaged patch applied without conflicts at both pins. GN generation, `chrome`, six GN Rust tests and seven bridge checks passed on each. Dependency sync/hooks passed after the second application. The native UI sequence has not yet been repeated after that rebuild. Exact source commits, hashes and limits are in [the evidence record](evidence/foundation.json).
- Chrome Web Store installation of uBlock Origin Lite `2026.1021.1354`, ID `ddkjiahejlhfcafbddmgiahcphecmpfh`, was observed. Functional/lifecycle acceptance remains unverified: driver reconnection first selected stock Chrome 151, invalidating those results; later the disposable profile lacked extension files while native management still reported ENABLED. Its dashboard returned `net::ERR_FILE_NOT_FOUND`. Cause is unresolved; retest with a fresh native-launched profile before concluding anything about compatibility.
- Launch emits a duplicate `ANGLESwapCGLLayer` Objective-C class warning from two component libraries. No safety conclusion follows from successful launch. Private-mode/profile boundaries, renderer recovery, native messaging and signed updates remain untested. Maintenance owner, backup runner and budget remain unspecified.

## Rust and page-control findings

The pinned [Rust guide](https://chromium.googlesource.com/chromium/src/+/7ae71de816ba32868ff9f974c2716f04ebd9b860/docs/rust/README.md) documents `rust_static_library` and Cxx. The experiment now uses that route with upstream Cxx 1.0.202. Safe core code forbids unsafe. Cxx-generated interoperation is isolated in the GN bridge target.

C++ owns a `rust::Box` for the control's lifetime. Borrowed strings last only through the synchronous call; results own their strings. Names are limited to 128 UTF-8 bytes, with explicit validation errors and no mutation on rejection. Views callbacks stay on the UI sequence, with debug sequence assertions. There is no asynchronous work or cancellation queue in this bounded command. It has no explicit panic path; this is not a general thread-safety, sanitizer or durability proof.

For MA-011, inspected these upstream interfaces at the same revision:

- [`DevToolsAgentHost`](https://chromium.googlesource.com/chromium/src/+/7ae71de816ba32868ff9f974c2716f04ebd9b860/content/public/browser/devtools_agent_host.h) supports attaching an in-process client to selected `WebContents` and dispatching protocol messages without starting a remote debugging server.
- [`DevToolsAgentHostClient`](https://chromium.googlesource.com/chromium/src/+/7ae71de816ba32868ff9f974c2716f04ebd9b860/content/public/browser/devtools_agent_host_client.cc) defaults to trusted access, permitted URL/frame attachment and local-file reads/writes. A restricted adapter must override those permissive defaults. Raw protocol forwarding is unsuitable for the product.
- The upstream extension debugger implementation uses this attachment route. Its presence establishes an integration candidate, not sufficient authorization for myarc.

Proposed narrow experiment: browser-owned grant selects one ordinary tab/Profile and origin; a restricted in-process client obtains bounded semantic observation, resolves a current element reference, performs one synthetic-site action, then observes the changed result. Map typed commands to an allowlist, reject arbitrary evaluation/target attachment, and keep document revision and scope checks inside the browser. Recheck redirects/frames; Stop must revoke queued/new actions and detach the client. External IPC still needs authenticated same-user access. Test both permitted and denied contexts before claiming feasibility.

This is source-level investigation only. Agent-browser's CDP-based test driver verifies the motion study; it does not implement or prove myarc's scoped adapter, CLI, MCP, consent UI or Stop behavior. See the [control contract](../docs/CONTROL-PROTOCOL.md).

## Export and maintenance blockers

MA-003.a now has one privately supplied Markdown export and an [invented structural counterpart](../tests/fixtures/arc-markdown/README.md). Space labels, named groups and ordered URL lists are observed. The producing action/build, Favorites ownership and nested-folder representation remain unverified. The sample omits Profile identities and tab titles. Raw data, fingerprints, screenshots and reference media stay outside the repository. The motion study remains separate from export-format evidence. Follow [migration boundaries](../docs/MIGRATION.md).

The [dependency/license inventory](evidence/dependency-licenses.json) contains 326 metadata entries discovered for `//chrome:chrome`, including optional version/revision fields. It is incomplete: eight metadata/license errors cover Catapult, Crashpad's nested zlib, Perfetto proto directories, Rust/toolchain wrappers and SwiftShader dependencies. The discovered graph includes build tooling; exact shipped closure and obligations need review. Chromium's development credits are a sample page by default. Set `generate_about_credits=true` for real notices, inspect collector errors and verify completeness before packaging; successful generation alone can omit metadata failures.

The current component app has an ad-hoc linker signature, no team identifier or sealed resources, and fails strict bundle verification with `code has no resources but signature indicates they must be present`. It is a local development build, unsuitable for distribution. Signing/notarization and authenticated update rehearsal remain open; signing identities were not inspected.

Before accepting the foundation, identify security triage and signing/update owners, primary/backup build capacity, recurring costs and absence coverage. An available workstation establishes neither funding nor ownership. Upstream licenses remain separate from this repository's MIT license. Helium, native messaging, DRM and codecs remain unevaluated.
