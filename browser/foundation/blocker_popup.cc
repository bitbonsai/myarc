#include "myarc/blocker_popup.h"

#include <string>

#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/extensions/extension_action_runner.h"
#include "chrome/browser/extensions/extension_view_host.h"
#include "chrome/browser/extensions/extension_view_host_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/extensions/extension_popup_types.h"
#include "chrome/browser/ui/views/extensions/extension_popup.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_specification.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_view.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_action.h"
#include "extensions/browser/extension_action_manager.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_util.h"
#include "myarc/app_activation.h"
#include "myarc/blocker.h"
#include "myarc/blocker_switch.h"
#include "myarc/test_buildflags.h"
#include "ui/views/bubble/bubble_anchor.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/view.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace myarc {
namespace {

constexpr char kCaptureBlockerPopup[] = "myarc-capture-blocker-popup";

#if BUILDFLAG(MYARC_TEST_HOOKS)
constexpr char kTestBlockerHook[] = "myarc-test-blocker-hook";

std::string TakeHook(const base::FilePath& path) {
  std::string text;
  if (!base::ReadFileToStringWithMaxSize(path, &text, 64)) {
    return {};
  }
  base::DeleteFile(path);
  return std::string(base::TrimWhitespaceASCII(text, base::TRIM_ALL));
}

base::FilePath& HookLog() {
  static base::NoDestructor<base::FilePath> log;
  return *log;
}

void AppendHookLog(const std::string& line) {
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(
          [](const base::FilePath& log, const std::string& line) {
            base::File file(log, base::File::FLAG_OPEN_ALWAYS |
                                     base::File::FLAG_APPEND);
            file.WriteAtCurrentPos(base::as_byte_span(line + "\n"));
          },
          HookLog(), line));
}

void RunHook(base::WeakPtr<BrowserWindowInterface> browser,
             const std::string& hook) {
  if (hook.empty() || !browser) {
    return;
  }
  LOG(WARNING) << "myarc-blocker test hook: " << hook;
  std::string outcome = "done";
  if (hook == "switch") {
    outcome = ShowSiteInfo(*browser) ? "popover" : "no popover";
    if (outcome != "popover") {
      ShowBlockerPopup(*browser);
    }
  } else if (hook == "toggle") {
    outcome = PressBlockerSwitchForTesting() ? "pressed" : "no switch";
  } else if (hook == "popup") {
    ShowBlockerPopup(*browser);
  }
  AppendHookLog("hook " + hook + " " + outcome);
}

void StartHook(BrowserWindowInterface& browser, const base::FilePath& path) {
  HookLog() = path.AddExtensionASCII("log");
  SetBlockerSwitchObserverForTesting(base::BindRepeating(&AppendHookLog));
  static base::NoDestructor<base::RepeatingTimer> poll;
  poll->Start(FROM_HERE, base::Milliseconds(300),
              base::BindRepeating(
                  [](const base::FilePath& path,
                     base::WeakPtr<BrowserWindowInterface> browser) {
                    base::ThreadPool::PostTaskAndReplyWithResult(
                        FROM_HERE, {base::MayBlock()},
                        base::BindOnce(&TakeHook, path),
                        base::BindOnce(&RunHook, browser));
                  },
                  path, browser.GetWeakPtr()));
}
#endif  // BUILDFLAG(MYARC_TEST_HOOKS)

}  // namespace

bool ShowSiteInfo(BrowserWindowInterface& browser) {
  tabs::TabInterface* tab = browser.GetActiveTabInterface();
  LocationBarView* location_bar = views::AsViewClass<LocationBarView>(
      BrowserElementsViews::From(&browser)->GetView(kLocationBarElementId));
  // The site info popover has the row only for an http(s) page.
  if (!tab || !location_bar || !location_bar->GetWidget() ||
      !tab->GetContents()->GetLastCommittedURL().SchemeIsHTTPOrHTTPS()) {
    return false;
  }
  if (IsAppActive()) {
    return location_bar->ShowPageInfoDialog();
  }
  // LocationBarView::ShowPageInfoDialog() shows the popover with activation,
  // which would bring a background app forward, so build it here as it does
  // and show it inactive. A background app runs this only from a capture or
  // a test hook.
  content::WebContents* contents = tab->GetContents();
  content::NavigationEntry* entry = contents->GetController().GetVisibleEntry();
  if (!entry || entry->IsInitialEntry()) {
    return false;
  }
  views::BubbleDialogDelegateView* bubble =
      PageInfoBubbleView::CreatePageInfoBubble(
          PageInfoBubbleSpecification::Builder(
              views::BubbleAnchor(location_bar),
              location_bar->GetWidget()->GetNativeWindow(), contents,
              entry->GetVirtualURL())
              .Build());
  bubble->SetHighlightedElement(kLocationIconElementId);
  bubble->GetWidget()->ShowInactive();
  return true;
}

void ShowBlockerSwitch(BrowserWindowInterface& browser) {
  if (!ShowSiteInfo(browser)) {
    ShowBlockerPopup(browser);
  }
}

void ShowBlockerPopup(BrowserWindowInterface& browser) {
  Profile* profile = browser.GetProfile();
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(profile)->enabled_extensions().GetByID(
          kBlockerExtensionId);
  tabs::TabInterface* tab = browser.GetActiveTabInterface();
  if (!extension || !tab ||
      (profile->IsOffTheRecord() &&
       !extensions::util::IsIncognitoEnabled(extension, profile))) {
    chrome::ShowExtensions(&browser, kBlockerExtensionId);
    return;
  }
  content::WebContents* contents = tab->GetContents();
  extensions::ExtensionAction* action =
      extensions::ExtensionActionManager::Get(profile)->GetExtensionAction(
          *extension);
  const GURL popup_url =
      action ? action->GetPopupUrl(
                   sessions::SessionTabHelper::IdForTab(contents).id())
             : GURL();
  auto* runner = extensions::ExtensionActionRunner::GetForWebContents(contents);
  // The same call a toolbar click makes: grants activeTab for this tab and
  // reports whether the action shows a popup.
  if (!popup_url.is_valid() || !runner ||
      runner->RunAction(extension, /*grant_tab_permissions=*/true) !=
          extensions::ExtensionAction::ShowAction::kShowPopup) {
    chrome::ShowExtensions(&browser, kBlockerExtensionId);
    return;
  }
  BrowserElementsViews* elements = BrowserElementsViews::From(&browser);
  views::View* anchor =
      elements->GetView(kLocationBarElementId, /*require_visible=*/true);
  if (!anchor) {
    anchor = elements->GetView(kBrowserViewElementId);
  }
  ExtensionPopup::ShowPopup(
      &browser,
      extensions::ExtensionViewHostFactory::CreatePopupHost(
          *extension, popup_url, &browser),
      views::BubbleAnchor(anchor), views::BubbleBorder::TOP_LEFT,
      PopupShowAction::kShow, base::DoNothing());
}

void MaybeScheduleBlockerPopupForCapture(BrowserWindowInterface& browser) {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
#if BUILDFLAG(MYARC_TEST_HOOKS)
  static bool hooked = false;
  if (!hooked && command_line.HasSwitch(kTestBlockerHook)) {
    hooked = true;
    StartHook(browser, command_line.GetSwitchValuePath(kTestBlockerHook));
  }
#endif
  static bool scheduled = false;
  if (scheduled || !command_line.HasSwitch(kCaptureBlockerPopup)) {
    return;
  }
  scheduled = true;
  int delay_ms = 4000;
  base::StringToInt(command_line.GetSwitchValueASCII(kCaptureBlockerPopup),
                    &delay_ms);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<BrowserWindowInterface> browser) {
            if (browser) {
              ShowBlockerPopup(*browser);
            }
          },
          browser.GetWeakPtr()),
      base::Milliseconds(delay_ms));
}

}  // namespace myarc
