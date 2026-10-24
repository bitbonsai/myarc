# Research and provenance

Research date: 2026-10-24. These are primary sources and a limited public-document review, not a completed Arc behavioral audit or an Artcraft success study. The review also does not establish how Artcraft's public repositories relate to any commercial AI service. Live branches and help pages can change. Pin exact upstream commits during implementation. No proprietary source, product assets, private shared standards, or personal export files are copied into this repository.

## Evidence ledger

| ID | Primary source | What was supported | Limit / follow-up |
| --- | --- | --- | --- |
| R01 | [Arc farewell](https://arc.net/) | Official desktop sunset date December 22, 2026; migration offering; optional AI in Dia | Does not document third-party export schema or establish myarc's import coverage |
| R02 | [Arc Profiles](https://resources.arc.net/hc/en-us/articles/19227964556183-Profiles-Separate-Work-Personal-Browsing) | Profiles scope browsing state and may be assigned to multiple Spaces | Retrieved documentation may lag the sunset transition; confirm semantics and platform versions |
| R03 | [Arc Organize index](https://resources.arc.net/hc/en-us/sections/19211839689111-Organize) | Named organization features and documentation map | Index establishes inventory, not detailed behavior or completeness |
| R04 | [Arc Spaces](https://resources.arc.net/hc/en-us/articles/19228064149143-Spaces-Distinct-Browsing-Areas), [Split View](https://resources.arc.net/hc/en-us/articles/19335393146775-Split-View-View-Multiple-Tabs-at-Once), [Boosts](https://resources.arc.net/hc/en-us/articles/19212718608151-Boosts-Customize-Any-Website) | Search-index evidence for these features; Boosts described as macOS customization including CSS/JavaScript | Direct retrieval returned 404 during research. Treat detailed behavior as unverified; find current documentation before implementation |
| R05 | [Disable Boosts](https://resources.arc.net/hc/en-us/articles/26681118599191-How-do-you-disable-Boosts), [macOS release notes](https://resources.arc.net/hc/en-us/articles/20498293324823-Arc-for-macOS-2024-2026-Release-Notes) | Indexed notes describe a global Boost toggle and changed cross-device JavaScript activation | Historical security-related behavior is useful context, not a specification for myarc permissions |
| R06 | [Electron extension support](https://www.electronjs.org/docs/latest/api/extensions) | Explicitly limited Chrome extension API support; arbitrary store-extension compatibility is not a goal | Rules out assuming Chromium embedding equals Chrome extension support |
| R07 | [Chromium Mac build instructions](https://chromium.googlesource.com/chromium/src/+/main/docs/mac_build_instructions.md) | Official native toolchain/build workflow | Unmodified build/launch and two-revision native interoperation now recorded in [foundation experiment](../browser/README.md); full compatibility and maintenance acceptance remain open |
| R08 | [Helium README](https://github.com/imputnet/helium/blob/main/README.md) | A Chromium-derived candidate with public packaging/tooling; distinct licenses apply to original/imported material | No local build, audit, compatibility verification, or permission to reuse branding is inferred |
| R09 | [Servo](https://servo.org/) | Rust engine with an embedding focus | This alone does not establish a complete browser or Chrome extension platform |
| R10 | [PhotoCraft AGENTS](https://github.com/storytold/photocraft/blob/main/AGENTS.md), [raw](https://raw.githubusercontent.com/storytold/photocraft/main/AGENTS.md) | Public orientation, layering, commands, reliability rules, documented evidence and control surfaces | Private shared rules referenced there were not accessed; myarc instructions are independently written |
| R11 | [VectorCraft AGENTS](https://github.com/storytold/vectorcraft/blob/main/AGENTS.md), [raw](https://raw.githubusercontent.com/storytold/vectorcraft/main/AGENTS.md) | Task orientation and provenance rules; different restrictions from sibling projects | Shared methodology must be adapted; rules are not identical across repositories |
| R12 | [DesignCraft AGENTS](https://github.com/storytold/designcraft/blob/main/AGENTS.md), [raw](https://raw.githubusercontent.com/storytold/designcraft/main/AGENTS.md) | Session orientation and recovery emphasis | Some referenced session plans are ignored/private rather than public templates |
| R13 | [PhotoCraft roadmap](https://github.com/storytold/photocraft/blob/main/ROADMAP.md) | Separates breadth, practical readiness, estimates, and evidence | Its numbers are its own assessments, not verified proof of agent productivity or a browser schedule |
| R14 | [Chromium Rust documentation](https://chromium.googlesource.com/chromium/src/+/HEAD/docs/rust/), [Rust FFI](https://chromium.googlesource.com/chromium/src.git/+/main/docs/rust/ffi.md), [API design](https://chromium.googlesource.com/chromium/src/+/main/docs/rust/api_design.md) | Official Rust integration and C++ interoperation guidance; support differs by tool/direction/platform | Pinned Cxx/GN route now builds and invokes a transient Rust command from native UI; this establishes interoperation, not the full workspace or control service |
| R15 | [PhotoCraft readiness assessment](https://github.com/storytold/photocraft/blob/main/docs/target-app-parity.md) | Revised estimates and separation of presence from practical replacement quality | Repository's self-assessment, not independently reproduced measurements |
| R16 | [PhotoCraft development guide](https://github.com/storytold/photocraft/blob/main/docs/development.md), [scorecard](https://github.com/storytold/photocraft/blob/main/docs/scorecard.md) | Separate behavior/native checks, fresh artifacts and actual test selection; structured/generated progress and explicit measurement states | Read through public GitHub file access when raw retrieval failed; implementation remains theirs, not copied tooling |
| R17 | [MCP transports](https://modelcontextprotocol.io/specification/2026-07-28/basic/transports), [MCP tools](https://modelcontextprotocol.io/specification/2026-07-28/server/tools) | Official transport/tool schemas, local stdio transport and user-visible control guidance | Pin actually supported SDK/protocol revisions and verify real clients during MA-011; a specification link is not implemented interoperability |
| R18 | [Arc security FAQ](https://arc.net/security), checked 2026-10-24 | FAQ says Arc includes removable uBlock Origin rather than a custom desktop blocker | Page effective date is 2024-09-27; no current Arc build/platform defaults or Manifest compatibility were tested. myArc default-on blocking is a product requirement, not a decision to bundle that exact extension. |

R19: one consented, privately held Markdown export, inspected 2026-10-24. [Observed structure and synthetic counterpart](../tests/fixtures/arc-markdown/README.md) document Space labels, named groups and bare URL lists. Producing action/build and favorite-versus-pin scope remain unverified. Generic Markdown shape alone cannot identify an Arc producer. Raw content and fingerprints remain private; public fixture labels/URLs are invented.

## What not to infer from Artcraft

An open-source project can coexist with a business that sells hosted AI or other convenience services. The sources reviewed here do not establish the ownership, code sharing, or commercial relationship between Artcraft's repositories and any image-generation service. They also do not show whether open-source releases are intended to generate traffic. Treat that as a hypothesis, not a product fact or a model for myarc.

Repository visibility and marketing can attract attention without proving product quality, sustainability, or user benefit. myarc should adopt useful engineering practices on their evidence, while keeping the browser useful without a paid AI service.

## What to take from Artcraft

Use documented session orientation, clear subsystem boundaries, a common command path, explicit failure handling, and proof attached to completion claims. Separate a feature's presence from its usefulness. Keep a ranked gap list and automate meaningful checks as actual code appears. These are methodological choices; repository popularity, commit counts, or screenshots do not establish dependable replacement quality.

Adapt rather than transplant: a browser must preserve upstream security and extension infrastructure. Our Rust core therefore sits beside Chromium's existing platform. Our public `plan/` contains publishable tasks and decisions; private samples and machine/session data stay out of it. Shared rules stay inside this repository until multiple projects justify extraction. We do not reproduce a private `craftrules` repository or copy a sibling app's constraints blindly.

The follow-up audit and concrete application decisions are in [ARTCRAFT-WORKFLOW.md](ARTCRAFT-WORKFLOW.md). It adds behavior-contract seeds, reproducible work records and a dependency correction; it does not claim that application checks exist before implementation.

## How much the help center can teach us

Use it to build the feature inventory, terminology, shortcuts, common tasks, platform distinctions, and explicit edge cases. Convert each finding into an original behavioral note with source URL/date and confidence. It cannot reliably establish exact animation, latency, timing, obscure interaction rules, storage schema, failure recovery, or extension fidelity. No credible percentage can be assigned from the current review.

For onboarding specifically, inventory permissible publicly available videos/references and ask the project owner which interaction qualities matter. Record observed vs documented vs proposed behavior separately. Review usage rights before preserving any reference media; do not ship it as a product asset. The proposed storyboard is in [ONBOARDING.md](ONBOARDING.md).

## Remaining research

1. Obtain and privately inspect consented Arc exports for relevant platforms/versions; establish what can actually be preserved.
2. Specify pin/reset/archive/multiwindow/Peek rules and onboarding interaction qualities with credible evidence.
3. Prove extension install/update/native-host behavior on the selected foundation; evaluate codecs, DRM, and vendor integrations separately.
4. Measure build/upstream-update effort and fund the ongoing release path.
5. Check final product naming, asset provenance, and selected upstream license obligations before distributing a browser. Independent implementation is a development practice, not a legal guarantee.
