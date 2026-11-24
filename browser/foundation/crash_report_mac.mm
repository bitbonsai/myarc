#include "myarc/crash_report.h"

#import <Foundation/Foundation.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/apple/bundle_locations.h"
#include "base/apple/foundation_util.h"
#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/i18n/time_formatting.h"
#include "base/json/json_reader.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/strings/escape.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/time/time.h"
#include "base/values.h"
#include "base/version_info/version_info.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/navigator/browser_navigator.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "chrome/common/chrome_paths.h"
#include "myarc/control_prompt.h"
#include "myarc/test_buildflags.h"
#include "ui/base/base_window.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace myarc {
namespace {

constexpr char kIssueUrl[] = "https://github.com/bitbonsai/myarc/issues/new";
// Remembers the crash log last offered, so a log is never offered twice.
constexpr char kMarkerName[] = "myArc Crash Report";
// The top of a log, as the user reads it in the issue: about 30 lines.
constexpr size_t kLogLines = 30;
constexpr size_t kFrames = 25;
constexpr size_t kLineBytes = 240;
// GitHub answers longer issue URLs with an error page.
constexpr size_t kMaxUrlBytes = 7000;
// Reports run to a few hundred KB; a longer one falls back to its first
// lines.
constexpr size_t kLogReadBytes = 4 * 1024 * 1024;
// A log older than this belongs to some other crash.
constexpr base::TimeDelta kLogMaxAge = base::Days(3);
// How often, and how long apart, the card looks for a window to sit on.
constexpr int kMaxAttempts = 10;
constexpr base::TimeDelta kRetryDelay = base::Seconds(1);
// Keys in a .ips header that identify the Mac rather than the crash.
constexpr const char* kDroppedKeys[] = {
    "\"crashReporterKey\"", "\"deviceIdentifierForVendor\"",
    "\"userID\"",           "\"incident\"",
    "\"storeInfo\"",
};

#if BUILDFLAG(MYARC_TEST_HOOKS)
constexpr char kTestCrashLogs[] = "myarc-test-crash-logs";
constexpr char kTestCrashReportUrl[] = "myarc-test-crash-report-url";
constexpr char kTestCrashReportPress[] = "myarc-test-crash-report-press";
#endif

bool g_offered = false;

struct Report {
  std::string log_name;
  std::string log_text;
  // Chromium's own record when there's no macOS log.
  std::string dump_note;
};

base::FilePath CrashLogDir() {
#if BUILDFLAG(MYARC_TEST_HOOKS)
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(kTestCrashLogs)) {
    return command_line.GetSwitchValuePath(kTestCrashLogs);
  }
#endif
  return base::GetHomeDir().Append("Library/Logs/DiagnosticReports");
}

// Chromium's crash dumps. Test hooks read a Crashpad folder beside the test
// logs, so a test never lists the real one.
base::FilePath CrashDumpDir() {
  base::FilePath dir;
#if BUILDFLAG(MYARC_TEST_HOOKS)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(kTestCrashLogs)) {
    return CrashLogDir().Append("Crashpad");
  }
#endif
  base::PathService::Get(chrome::DIR_CRASH_DUMPS, &dir);
  return dir;
}

// The newest file matching `pattern` in `dir` modified after `since`.
std::optional<base::FileEnumerator::FileInfo> Newest(
    const base::FilePath& dir,
    const base::FilePath::StringType& pattern,
    base::Time since) {
  std::optional<base::FileEnumerator::FileInfo> newest;
  base::FileEnumerator files(dir, /*recursive=*/false,
                             base::FileEnumerator::FILES, pattern);
  for (base::FilePath path = files.Next(); !path.empty(); path = files.Next()) {
    const base::FileEnumerator::FileInfo info = files.GetInfo();
    if (info.GetLastModifiedTime() > since &&
        (!newest ||
         info.GetLastModifiedTime() > newest->GetLastModifiedTime())) {
      newest = info;
    }
  }
  return newest;
}

// One line of the issue's code block: bounded, and unable to end the block.
std::string Clean(std::string_view line, size_t max = kLineBytes) {
  std::string clean(line.substr(0, max));
  base::ReplaceSubstringsAfterOffset(&clean, 0, "```", "'''");
  return clean + "\n";
}

// A .ips file's first line, one JSON object, with its identifiers blanked.
std::string Header(std::string_view line) {
  std::string header(line);
  for (std::string_view key : {"\"incident_id\":\"", "\"slice_uuid\":\""}) {
    const size_t at = header.find(key);
    const size_t end =
        at == std::string::npos ? at : header.find('"', at + key.size());
    if (end != std::string::npos) {
      header.replace(at + key.size(), end - at - key.size(), "…");
    }
  }
  return Clean(header, 2 * kLineBytes);
}

// The crash from a .ips report body: the exception, the termination, any
// abort message and the crashed thread's top frames. Empty when the body
// doesn't parse, as a truncated one doesn't.
std::string CrashSummary(std::string_view body) {
  std::optional<base::DictValue> report =
      base::JSONReader::ReadDict(body, base::JSON_PARSE_RFC);
  if (!report) {
    return std::string();
  }
  std::string text;
  if (const base::DictValue* exception = report->FindDict("exception")) {
    std::string line = "exception:";
    for (const char* key : {"type", "signal", "subtype"}) {
      if (const std::string* value = exception->FindString(key)) {
        line += " " + *value;
      }
    }
    text += Clean(line);
  }
  if (const std::string* indicator =
          report->FindStringByDottedPath("termination.indicator")) {
    text += Clean("termination: " + *indicator);
  }
  // Abort and assertion messages, such as a failed CHECK's, by library.
  if (const base::DictValue* messages = report->FindDict("asi")) {
    for (const auto [library, lines] : *messages) {
      if (!lines.is_list()) {
        continue;
      }
      for (const base::Value& message : lines.GetList()) {
        if (message.is_string()) {
          text += Clean(library + ": " + message.GetString());
        }
      }
    }
  }
  const base::ListValue* threads = report->FindList("threads");
  const base::ListValue* images = report->FindList("usedImages");
  const int faulting = report->FindInt("faultingThread").value_or(0);
  if (!threads || faulting < 0 ||
      static_cast<size_t>(faulting) >= threads->size() ||
      !(*threads)[faulting].is_dict()) {
    return text;
  }
  const base::DictValue& thread = (*threads)[faulting].GetDict();
  const std::string* name = thread.FindString("name");
  const std::string* queue = thread.FindString("queue");
  text += Clean("crashed thread: " +
                (name ? *name : (queue ? *queue : std::string("?"))));
  const base::ListValue* frames = thread.FindList("frames");
  if (!frames) {
    return text;
  }
  size_t shown = 0;
  for (const base::Value& frame : *frames) {
    if (shown == kFrames) {
      break;
    }
    if (!frame.is_dict()) {
      continue;
    }
    const std::string* symbol = frame.GetDict().FindString("symbol");
    const std::optional<int> index = frame.GetDict().FindInt("imageIndex");
    const std::string* image =
        images && index && *index >= 0 &&
                static_cast<size_t>(*index) < images->size() &&
                (*images)[*index].is_dict()
            ? (*images)[*index].GetDict().FindString("name")
            : nullptr;
    text += Clean(base::StringPrintf("%2zu  %s  %s", shown,
                                     image ? image->c_str() : "?",
                                     symbol ? symbol->c_str() : "?"));
    ++shown;
  }
  return text;
}

// The top of a crash log as the issue shows it: the header line, then the
// crash summary, or the report's first lines when it doesn't parse. Lines
// that identify the Mac rather than the crash are dropped.
std::string TopOfLog(const base::FilePath& path) {
  // A longer log fills the buffer with its start, which is all that's read.
  std::string log;
  base::ReadFileToStringWithMaxSize(path, &log, kLogReadBytes);
  const size_t newline = log.find('\n');
  const std::string_view first = std::string_view(log).substr(0, newline);
  const std::string_view body = newline == std::string::npos
                                    ? std::string_view()
                                    : std::string_view(log).substr(newline + 1);
  std::string text = first.starts_with('{') ? Header(first) : Clean(first);
  if (const std::string summary = CrashSummary(body); !summary.empty()) {
    return text + summary;
  }
  size_t kept = 1;
  for (std::string_view line : base::SplitStringPiece(
           body, "\n", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL)) {
    if (kept == kLogLines) {
      break;
    }
    bool dropped = false;
    for (const char* key : kDroppedKeys) {
      dropped |= line.find(key) != std::string_view::npos;
    }
    if (!dropped) {
      text += Clean(line);
      ++kept;
    }
  }
  return text;
}

// Blocking. Finds the newest myArc crash log not offered before, and records
// it as offered.
Report Gather(base::FilePath user_data) {
  base::ScopedBlockingCall blocking(FROM_HERE, base::BlockingType::MAY_BLOCK);
  Report report;
  const base::FilePath marker = user_data.Append(kMarkerName);
  std::string offered;
  base::ReadFileToStringWithMaxSize(marker, &offered, 4096);
  offered = std::string(base::TrimWhitespaceASCII(offered, base::TRIM_ALL));
  const base::Time since = base::Time::Now() - kLogMaxAge;
  if (const auto log = Newest(CrashLogDir(), "myArc*.ips", since)) {
    const std::string name = log->GetName().BaseName().value();
    if (name != offered) {
      report.log_name = name;
      report.log_text =
          TopOfLog(CrashLogDir().Append(log->GetName().BaseName()));
      base::WriteFile(marker, name);
    }
  }
  if (report.log_name.empty()) {
    const base::FilePath crashpad = CrashDumpDir();
    if (!crashpad.empty()) {
      std::optional<base::FileEnumerator::FileInfo> dump;
      for (const char* dir : {"pending", "completed"}) {
        auto found = Newest(crashpad.Append(dir), "*.dmp", since);
        if (found && (!dump || found->GetLastModifiedTime() >
                                   dump->GetLastModifiedTime())) {
          dump = found;
        }
      }
      if (dump) {
        report.dump_note = "Chromium wrote a crash dump, " +
                           dump->GetName().BaseName().value() + ", at " +
                           base::UTF16ToUTF8(base::TimeFormatShortDateAndTime(
                               dump->GetLastModifiedTime())) +
                           ". It stays on this Mac and isn't attached.";
      }
    }
  }
  return report;
}

std::string MyarcVersion() {
  // Written into Info.plist when the release is packaged; dev builds lack it.
  NSString* version = base::apple::ObjCCast<NSString>(
      [base::apple::OuterBundle() objectForInfoDictionaryKey:@"MyarcVersion"]);
  return version.length ? base::SysNSStringToUTF8(version)
                        : "Development build";
}

GURL IssueUrl(const Report& report) {
  const std::string myarc = MyarcVersion();
  const std::string chromium(version_info::GetVersionNumber());
  const std::string head = "**What were you doing when myArc quit?**\n\n\n"
                           "**myArc:** " +
                           myarc + " (Chromium " + chromium +
                           ")\n"
                           "**macOS:** " +
                           base::SysInfo::OperatingSystemVersion() + "\n\n";
  const std::string prefix =
      std::string(kIssueUrl) + "?title=" +
      base::EscapeQueryParamValue("Crash: myArc " + myarc,
                                  /*use_plus=*/false) +
      "&body=";
  if (report.log_name.empty()) {
    std::string body =
        head +
        "**Crash log:** macOS didn't write one for this crash, so myArc only "
        "knows the last session ended unexpectedly.";
    if (!report.dump_note.empty()) {
      body += " " + report.dump_note;
    }
    return GURL(prefix +
                base::EscapeQueryParamValue(body + "\n", /*use_plus=*/false));
  }
  // GitHub refuses long URLs, so the deepest frames go first.
  std::string log = report.log_text;
  while (true) {
    const std::string url =
        prefix + base::EscapeQueryParamValue(
                     head + "**Crash log** (top of `" + report.log_name +
                         "` in ~/Library/Logs/DiagnosticReports)\n\n```\n" +
                         log + "```\n",
                     /*use_plus=*/false);
    const size_t last =
        log.rfind('\n', log.size() >= 2 ? log.size() - 2 : std::string::npos);
    if (url.size() <= kMaxUrlBytes || last == std::string::npos) {
      return GURL(url);
    }
    log.resize(last + 1);
  }
}

// The most recently used regular main window, or null.
BrowserWindowInterface* PromptWindow() {
  BrowserWindowInterface* found = nullptr;
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [&](BrowserWindowInterface* browser) {
        // Little Arc is a popup, so this skips it too.
        if (browser->GetProfile()->IsOffTheRecord() ||
            browser->GetType() != BrowserWindowInterface::TYPE_NORMAL) {
          return true;
        }
        found = browser;
        return false;
      });
  return found;
}

std::unique_ptr<ControlPrompt>& Prompt() {
  static base::NoDestructor<std::unique_ptr<ControlPrompt>> prompt;
  return *prompt;
}

void ShowPrompt(GURL url, int attempt);

void OpenIssue(const GURL& url, int attempt, ControlPrompt::Answer answer) {
  // The prompt is done with; drop it after its callback unwinds.
  base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(
      FROM_HERE, Prompt().release());
  // Restoring the crashed session can replace the window the card sat on.
  // Unanswered, it moves to the window in front.
  if (answer == ControlPrompt::Answer::kClosed) {
    if (!browser_shutdown::HasShutdownStarted()) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE, base::BindOnce(&ShowPrompt, url, attempt + 1),
          kRetryDelay);
    }
    return;
  }
  BrowserWindowInterface* browser = PromptWindow();
  if (answer != ControlPrompt::Answer::kAllow || !browser) {
    return;
  }
  NavigateParams params(browser, url, ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
  params.disposition = WindowOpenDisposition::kNewForegroundTab;
  params.window_action = NavigateParams::WindowAction::kShowWindow;
  Navigate(&params);
}

// Shows the card on the window in front, waiting for one to show.
void ShowPrompt(GURL url, int attempt) {
  if (Prompt() || attempt > kMaxAttempts) {
    return;
  }
  BrowserWindowInterface* browser = PromptWindow();
  views::Widget* widget = browser ? views::Widget::GetWidgetForNativeWindow(
                                        browser->GetWindow()->GetNativeWindow())
                                  : nullptr;
  if (!widget || !widget->IsVisible()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, base::BindOnce(&ShowPrompt, std::move(url), attempt + 1),
        kRetryDelay);
    return;
  }
  Prompt() = std::make_unique<ControlPrompt>(
      *widget,
      ControlPrompt::Copy{
          .title = u"myArc quit unexpectedly. Report it?",
          .body = u"Report opens a new GitHub issue with the myArc and macOS "
                  u"versions and the top of the crash log. You read it there "
                  u"before anything is sent.",
          .decline = u"Not Now",
          .accept = u"Report…",
      },
      base::BindOnce(&OpenIssue, url, attempt));
}

void Offer(Report report) {
  const GURL url = IssueUrl(report);
  ShowPrompt(url, 0);
#if BUILDFLAG(MYARC_TEST_HOOKS)
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(kTestCrashReportUrl)) {
    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce([](base::FilePath path,
                          std::string spec) { base::WriteFile(path, spec); },
                       command_line.GetSwitchValuePath(kTestCrashReportUrl),
                       url.spec()));
  }
  if (command_line.HasSwitch(kTestCrashReportPress)) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, base::BindOnce([] {
          if (Prompt()) {
            Prompt()->PressForTesting(/*allow=*/true);
          }
        }),
        base::Seconds(2));
  }
#endif
}

}  // namespace

void MaybeOfferCrashReport(BrowserWindowInterface& browser) {
  if (g_offered || browser.GetProfile()->IsOffTheRecord()) {
    return;
  }
  g_offered = true;
  base::FilePath user_data;
  if (!base::PathService::Get(chrome::DIR_USER_DATA, &user_data)) {
    return;
  }
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&Gather, user_data), base::BindOnce(&Offer));
}

}  // namespace myarc
