# M0 foundation experiment

Status: investigation in progress. [upstream.json](upstream.json) pins a complete Chromium main-branch feasibility candidate and depot_tools. This is neither an accepted foundation nor a supported release baseline. No myarc browser binary or Rust/native bridge has been verified.

## Bounded outcomes

MA-002.a first builds the unmodified `chrome` target at the pinned revision. Stop at a reproducible result or an actionable build blocker. Only after it runs, integrate one real Rust workspace command with a native UI trigger, then reapply that patch on another upstream revision. The full [MA-002 gate](../plan/EXECUTION.md) also needs extension lifecycle, isolation/recovery, licensing and maintenance evidence.

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

Monitor free disk during checkout, hooks and compilation. The first local attempt used a 40 GiB free-space stop reserve, checked every five seconds; this is a local safety policy, not an upstream capacity guarantee. Do not delete unrelated files or disable browser security to finish a build. Build time, peak storage and recurring runner cost remain unknown until measured.

Once compilation succeeds, launch with a new disposable profile outside the repository:

```sh
out/MyarcM0/Chromium.app/Contents/MacOS/Chromium \
  --user-data-dir="$WORK/test-profile"
```

Do not point the experiment at an existing Chrome, Arc or Helium profile. Installation of an existing browser is not evidence that this source build works.

## Run record, 2026-10-24

- Host preflight: macOS 26.7, arm64, 48 GiB RAM, 15 logical CPUs, APFS, Xcode 27.0 (`27A266a`), macOS SDK 27.0. Initial available disk was approximately 220 GiB. Signing identities and credentials were not inspected.
- Candidate: Chromium `158.0.8122.0`, revision in the pin. Source checkout reached that exact revision.
- First sync ran from 06:20:05 to 06:30:05 UTC and failed in LiteRT checkout: `git-lfs: command not found`. No hooks, compilation or launch occurred in that attempt. Partial dependencies were retained.
- Installed Git LFS 3.8.1 and resumed the same pinned sync. Dependency sync and upstream hooks passed. First GN invocation failed because pinned depot_tools lacked bootstrapped Python (`python3_bin_reldir.txt not found`). Ran its supported `ensure_bootstrap` without updating the pin, then generation passed.
- First build scheduled at 06:36:24 UTC and failed before compilation because LiteRT's earlier failed checkout had no valid HEAD and lacked `tflite/converter/schema/schema.fbs`. A direct repair then reached the mirror's LFS endpoint, which returned HTTP 405. Restored only this generated dependency at its pinned revision with process-scoped `GIT_LFS_SKIP_SMUDGE=1`; its Git tree is clean, required source exists, and generated Ninja files contain no references to its prebuilt-library paths. No global LFS configuration or upstream source was changed.
- Second build completed 1,155 steps before ANGLE failed on missing Metal Toolchain. Apple's `xcodebuild -downloadComponent MetalToolchain` installed component `27A266a`; `xcrun --find metal` then succeeded. Incremental build resumed at 06:40 UTC with eight jobs.
- Completion and peak storage remain unmeasured. Local stage logs remain outside the public tree. Follow the active stage before retrying; never start a second sync/build against this checkout.
- No clean build result, extension install/use/restart/update/remove, native-message case, Rust bridge, rebase, profile/private-mode isolation, renderer recovery or signed update has passed yet. Maintenance owner, backup runner and ongoing budget remain unspecified. Foundation acceptance stays open.

## Rust and page-control findings

The pinned [Rust guide](https://chromium.googlesource.com/chromium/src/+/7ae71de816ba32868ff9f974c2716f04ebd9b860/docs/rust/README.md) documents `rust_static_library` and Cxx; FFI guidance identifies Cxx as the standard bridge and describes current Crubit limitations. Select Cxx for the initial experiment, subject to actual GN/build proof. Keep the core source reusable under Cargo and GN, with FFI isolated and no unwinding across it. No bridge code is claimed here.

For MA-011, inspected these upstream interfaces at the same revision:

- [`DevToolsAgentHost`](https://chromium.googlesource.com/chromium/src/+/7ae71de816ba32868ff9f974c2716f04ebd9b860/content/public/browser/devtools_agent_host.h) supports attaching an in-process client to selected `WebContents` and dispatching protocol messages without starting a remote debugging server.
- [`DevToolsAgentHostClient`](https://chromium.googlesource.com/chromium/src/+/7ae71de816ba32868ff9f974c2716f04ebd9b860/content/public/browser/devtools_agent_host_client.cc) defaults to trusted access, permitted URL/frame attachment and local-file reads/writes. A restricted adapter must override those permissive defaults. Raw protocol forwarding is unsuitable for the product.
- The upstream extension debugger implementation uses this attachment route. Its presence establishes an integration candidate, not sufficient authorization for myarc.

Proposed narrow experiment: browser-owned grant selects one ordinary tab/Profile and origin; a restricted in-process client obtains bounded semantic observation, resolves a current element reference, performs one synthetic-site action, then observes the changed result. Map typed commands to an allowlist, reject arbitrary evaluation/target attachment, and keep document revision and scope checks inside the browser. Recheck redirects/frames; Stop must revoke queued/new actions and detach the client. External IPC still needs authenticated same-user access. Test both permitted and denied contexts before claiming feasibility.

This is source-level investigation only. Agent-browser's CDP-based test driver verifies the motion study; it does not implement or prove myarc's scoped adapter, CLI, MCP, consent UI or Stop behavior. See the [control contract](../docs/CONTROL-PROTOCOL.md).

## Export and maintenance blockers

MA-003 has zero consented official export samples available to this task. No private profile was opened. Platform/version, official export route, file signature, schema version, field mapping and omissions remain unverified. The motion fixtures are invented workspace content, not Arc-format fixtures. Next step: obtain one user-selected export with explicit permission and version/route metadata, inspect locally, then create independent synthetic equivalents. Follow [migration boundaries](../docs/MIGRATION.md).

Before accepting the foundation, identify security triage and signing/update owners, primary/backup build capacity, storage/artifact costs and absence coverage. Do not infer funding or ownership from an available workstation. Inventory Chromium's `LICENSE` and applicable third-party notices after dependency sync; permissive licensing of this small repository does not license the whole browser. Helium, store access, native messaging, DRM and codecs remain unevaluated.
