#ifndef MYARC_CRASH_REPORT_H_
#define MYARC_CRASH_REPORT_H_

class BrowserWindowInterface;

namespace myarc {

// After a crashed session, offers "myArc quit unexpectedly. Report it?" at
// the top-right of the most recently used main window. Report opens a new
// GitHub issue in a tab, prefilled with the myArc and macOS versions and the
// top of the newest myArc crash log, so the user reads it before sending
// anything. Nothing is uploaded. Call where Chromium shows its crashed-session
// UI; regular Profiles only. Offers at most once per process and never twice
// for the same crash log. UI thread only.
//
// Test-hook builds take --myarc-test-crash-logs=<dir>, read instead of
// ~/Library/Logs/DiagnosticReports, --myarc-test-crash-report-url=<file>,
// which writes the issue URL there when the prompt shows, and
// --myarc-test-crash-report-press, which presses Report two seconds later.
void MaybeOfferCrashReport(BrowserWindowInterface& browser);

}  // namespace myarc

#endif  // MYARC_CRASH_REPORT_H_
