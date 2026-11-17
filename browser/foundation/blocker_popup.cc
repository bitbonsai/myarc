#include "myarc/blocker_popup.h"

#include <string>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
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
#include "components/sessions/content/session_tab_helper.h"
#include "components/tabs/public/tab_interface.h"
#include "extensions/browser/extension_action.h"
#include "extensions/browser/extension_action_manager.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_util.h"
#include "myarc/blocker.h"
#include "ui/views/bubble/bubble_anchor.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/view.h"

namespace myarc {
namespace {

constexpr char kCaptureBlockerPopup[] = "myarc-capture-blocker-popup";

}  // namespace

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
