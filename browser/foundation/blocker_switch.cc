#include "myarc/blocker_switch.h"

#include <optional>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/json/string_escape.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/scoped_observation.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/controls/rich_controls_container_view.h"
#include "chrome/browser/ui/views/page_info/page_info_view_factory.h"
#include "components/tabs/public/tab_interface.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/reload_type.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/isolated_world_ids.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/offscreen_document_host.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "myarc/blocker.h"
#include "myarc/blocker_popup.h"
#include "myarc/bridge.rs.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace myarc {
namespace {

constexpr char16_t kTitle[] = u"Block ads on this site";
constexpr char16_t kMoreSettings[] = u"More blocker settings…";
constexpr char kTitlePrefix[] = "myarc:";
constexpr base::TimeDelta kTimeout = base::Seconds(10);

base::RepeatingCallback<void(const std::string&)>& SwitchObserver() {
  static base::NoDestructor<base::RepeatingCallback<void(const std::string&)>>
      observer;
  return *observer;
}

const extensions::Extension* UsableBlocker(Profile* profile) {
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(profile)->enabled_extensions().GetByID(
          kBlockerExtensionId);
  if (!extension || (profile->IsOffTheRecord() &&
                     !extensions::util::IsIncognitoEnabled(extension, profile))) {
    return nullptr;
  }
  return extension;
}

// Sends one message to the blocker's service worker from a hidden page of
// the blocker's own origin in `profile`, the way its popup does, and hands
// back the JSON the script leaves in the page title. The page is the
// blocker's LICENSE.txt, which runs none of its scripts; the extension API
// is there because the frame is the blocker's.
//
// Content can't run script in an extension frame through its usual entry
// point, so this uses ExecuteJavaScriptForTests on a page myArc creates and
// owns. The script is built here from a canonical host and integers only.
class BlockerMessage : public content::WebContentsObserver,
                       public ProfileObserver {
 public:
  using Callback = base::OnceCallback<void(std::optional<base::DictValue>)>;

  static void Send(Profile* profile,
                   const extensions::Extension& extension,
                   std::string script,
                   Callback callback) {
    // Owns itself until it answers.
    new BlockerMessage(profile, extension, std::move(script),
                       std::move(callback));
  }

  BlockerMessage(const BlockerMessage&) = delete;
  BlockerMessage& operator=(const BlockerMessage&) = delete;
  ~BlockerMessage() override = default;

 private:
  BlockerMessage(Profile* profile,
                 const extensions::Extension& extension,
                 std::string script,
                 Callback callback)
      : script_(base::UTF8ToUTF16(script)), callback_(std::move(callback)) {
    host_ = std::make_unique<extensions::OffscreenDocumentHost>(
        extension, profile, extension.ResolveExtensionURL("LICENSE.txt"));
    Observe(host_->host_contents());
    profile_observation_.Observe(profile);
    host_->SetCloseHandler(base::BindOnce(
        [](base::WeakPtr<BlockerMessage> message, extensions::ExtensionHost*) {
          if (message) {
            message->Finish(std::nullopt);
          }
        },
        weak_factory_.GetWeakPtr()));
    timeout_.Start(FROM_HERE, kTimeout,
                   base::BindOnce(&BlockerMessage::Finish,
                                  base::Unretained(this), std::nullopt));
    host_->CreateRendererSoon();
  }

  // content::WebContentsObserver:
  void DidFinishLoad(content::RenderFrameHost* frame,
                     const GURL& url) override {
    if (sent_ || !frame->IsInPrimaryMainFrame()) {
      return;
    }
    sent_ = true;
    frame->ExecuteJavaScriptForTests(script_, base::NullCallback(),
                                     content::ISOLATED_WORLD_ID_GLOBAL);
  }

  void TitleWasSet(content::NavigationEntry* entry) override {
    const std::string title = base::UTF16ToUTF8(entry->GetTitle());
    if (!title.starts_with(kTitlePrefix)) {
      return;
    }
    Finish(base::JSONReader::ReadDict(
        std::string_view(title).substr(sizeof(kTitlePrefix) - 1),
        base::JSON_PARSE_RFC));
  }

  // ProfileObserver:
  void OnProfileWillBeDestroyed(Profile* profile) override {
    Finish(std::nullopt);
  }

  void Finish(std::optional<base::DictValue> result) {
    if (!callback_) {
      return;
    }
    timeout_.Stop();
    profile_observation_.Reset();
    Observe(nullptr);
    std::move(callback_).Run(std::move(result));
    // The host may be calling into this object; delete both after it returns.
    base::SequencedTaskRunner::GetCurrentDefault()->DeleteSoon(FROM_HERE, this);
  }

  const std::u16string script_;
  Callback callback_;
  std::unique_ptr<extensions::OffscreenDocumentHost> host_;
  base::OneShotTimer timeout_;
  bool sent_ = false;
  base::ScopedObservation<Profile, ProfileObserver> profile_observation_{this};
  base::WeakPtrFactory<BlockerMessage> weak_factory_{this};
};

// The script around `body`, an async function body that returns the result.
std::string Script(const std::string& body) {
  return base::StrCat(
      {"(async () => {\n"
       "  let out\n"
       "  try {\n"
       "    out = await (async () => {",
       body,
       "})()\n"
       "  } catch (e) {\n"
       "    out = { error: String(e) }\n"
       "  }\n"
       "  document.title = '",
       kTitlePrefix,
       "' + JSON.stringify(out)\n"
       "})()\n"});
}

// popupPanelData is what the popup asks for when it opens; it carries the
// host's level and the reload setting.
std::string ReadScript(const std::string& host) {
  const std::string quoted = base::GetQuotedJSONString(host);
  return Script(base::StrCat(
      {"\n      const [panel, def] = await Promise.all([\n"
       "        chrome.runtime.sendMessage({ what: 'popupPanelData', hostname: ",
       quoted,
       " }),\n"
       "        chrome.runtime.sendMessage({ what: 'getDefaultFilteringMode' }),\n"
       "      ])\n"
       "      return { level: panel.level, def, autoReload: panel.autoReload }\n"
       "    "}));
}

// The popup's own message when a step of its slider is picked.
std::string WriteScript(const std::string& host, int level) {
  return Script(base::StrCat(
      {"\n      const level = await chrome.runtime.sendMessage({ what: "
       "'setFilteringMode', hostname: ",
       base::GetQuotedJSONString(host), ", level: ",
       base::NumberToString(level),
       " })\n"
       "      return { level }\n"
       "    "}));
}

class BlockerSwitchRow : public views::View,
                         public content::WebContentsObserver {
 public:
  BlockerSwitchRow(content::WebContents* contents,
                   const extensions::Extension& extension,
                   std::string host)
      : content::WebContentsObserver(contents),
        profile_(Profile::FromBrowserContext(contents->GetBrowserContext())),
        extension_id_(extension.id()),
        host_(std::move(host)) {
    SetUseDefaultFillLayout(true);
    row_ = AddChildView(std::make_unique<RichControlsContainerView>());
    row_->SetIcon(PageInfoViewFactory::GetImageModel(vector_icons::kShieldIcon));
    row_->SetTitle(kTitle);
    views::StyledLabel* more = row_->AddSecondaryStyledLabel(kMoreSettings);
    more->AddStyleRange(
        gfx::Range(0, std::u16string_view(kMoreSettings).size()),
        views::StyledLabel::RangeStyleInfo::CreateForLink(base::BindRepeating(
            &BlockerSwitchRow::OpenBlockerPopup, base::Unretained(this))));

    auto toggle = std::make_unique<views::ToggleButton>(base::BindRepeating(
        &BlockerSwitchRow::OnPressed, base::Unretained(this)));
    toggle->SetPreferredSize(gfx::Size(toggle->GetPreferredSize().width(),
                                       row_->GetFirstLineHeight()));
    toggle->SetProperty(
        views::kMarginsKey,
        gfx::Insets::VH(0, views::LayoutProvider::Get()->GetDistanceMetric(
                               views::DISTANCE_RELATED_LABEL_HORIZONTAL)));
    toggle->GetViewAccessibility().SetName(std::u16string(kTitle));
    toggle->SetEnabled(false);
    toggle_ = row_->AddControl(std::move(toggle));
    SetProperty(views::kFlexBehaviorKey,
                views::FlexSpecification(
                    base::BindRepeating(&RichControlsContainerView::FlexRule,
                                        base::Unretained(row_))));

    Newest() = this;
    Send(ReadScript(host_), base::BindOnce(&BlockerSwitchRow::OnRead,
                                           weak_factory_.GetWeakPtr()));
  }

  ~BlockerSwitchRow() override {
    if (Newest() == this) {
      Newest() = nullptr;
    }
  }

  static raw_ptr<BlockerSwitchRow>& Newest() {
    static base::NoDestructor<raw_ptr<BlockerSwitchRow>> row;
    return *row;
  }

  bool PressForTesting() {
    if (!toggle_->GetEnabled()) {
      return false;
    }
    toggle_->SetIsOn(!toggle_->GetIsOn());
    OnPressed();
    return true;
  }

 private:
  void Send(std::string script, BlockerMessage::Callback callback) {
    const extensions::Extension* extension = UsableBlocker(profile_);
    if (!extension || extension->id() != extension_id_) {
      std::move(callback).Run(std::nullopt);
      return;
    }
    BlockerMessage::Send(profile_, *extension, std::move(script),
                         std::move(callback));
  }

  void OnRead(std::optional<base::DictValue> result) {
    const std::optional<int> level =
        result ? result->FindInt("level") : std::nullopt;
    const std::optional<int> default_level =
        result ? result->FindInt("def") : std::nullopt;
    if (!level || !default_level) {
      Show(-1);
      return;
    }
    default_level_ = *default_level;
    auto_reload_ = result->FindBool("autoReload").value_or(true);
    Show(*level);
  }

  void OnPressed() {
    const int target =
        foundation::blocker_switch_level(toggle_->GetIsOn(), default_level_);
    toggle_->SetEnabled(false);
    Send(WriteScript(host_, target),
         base::BindOnce(&BlockerSwitchRow::OnWritten,
                        weak_factory_.GetWeakPtr(), level_));
  }

  void OnWritten(int before, std::optional<base::DictValue> result) {
    const std::optional<int> after =
        result ? result->FindInt("level") : std::nullopt;
    if (!after) {
      Show(before);
      return;
    }
    Show(*after);
    // The popup reloads after a change unless its "reload on change"
    // setting is off; the page then loads under the new rules.
    if (*after != before && auto_reload_ && web_contents()) {
      web_contents()->GetController().Reload(content::ReloadType::NORMAL,
                                             /*check_for_repost=*/true);
    }
  }

  void Show(int level) {
    level_ = level;
    const foundation::BlockerSwitchState state =
        foundation::blocker_switch_state(level);
    toggle_->SetIsOn(state == foundation::BlockerSwitchState::On);
    toggle_->SetEnabled(state != foundation::BlockerSwitchState::Unknown);
    if (SwitchObserver()) {
      SwitchObserver().Run(base::StrCat(
          {host_, " level=", base::NumberToString(level), " ",
           state == foundation::BlockerSwitchState::On    ? "on"
           : state == foundation::BlockerSwitchState::Off ? "off"
                                                          : "unknown"}));
    }
  }

  void OpenBlockerPopup() {
    tabs::TabInterface* tab =
        web_contents() ? tabs::TabInterface::MaybeGetFromContents(web_contents())
                       : nullptr;
    BrowserWindowInterface* browser =
        tab ? tab->GetBrowserWindowInterface() : nullptr;
    if (!browser) {
      return;
    }
    // Close the site info popover first; the blocker's popup opens under the
    // address field in its place.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(
                       [](base::WeakPtr<BrowserWindowInterface> browser) {
                         if (browser) {
                           ShowBlockerPopup(*browser);
                         }
                       },
                       browser->GetWeakPtr()));
    if (GetWidget()) {
      GetWidget()->Close();
    }
  }

  const raw_ptr<Profile> profile_;
  const extensions::ExtensionId extension_id_;
  const std::string host_;
  raw_ptr<RichControlsContainerView> row_ = nullptr;
  raw_ptr<views::ToggleButton> toggle_ = nullptr;
  int level_ = -1;
  int default_level_ = 2;
  bool auto_reload_ = true;
  base::WeakPtrFactory<BlockerSwitchRow> weak_factory_{this};
};

}  // namespace

std::unique_ptr<views::View> CreateBlockerSwitchRow(
    content::WebContents* contents) {
  if (!contents) {
    return nullptr;
  }
  const GURL& url = contents->GetLastCommittedURL();
  if (!url.SchemeIsHTTPOrHTTPS() || url.host().empty()) {
    return nullptr;
  }
  const extensions::Extension* extension = UsableBlocker(
      Profile::FromBrowserContext(contents->GetBrowserContext()));
  if (!extension) {
    return nullptr;
  }
  return std::make_unique<BlockerSwitchRow>(contents, *extension,
                                            std::string(url.host()));
}

bool PressBlockerSwitchForTesting() {
  BlockerSwitchRow* row = BlockerSwitchRow::Newest();
  return row && row->PressForTesting();
}

void SetBlockerSwitchObserverForTesting(
    base::RepeatingCallback<void(const std::string&)> observer) {
  SwitchObserver() = std::move(observer);
}

}  // namespace myarc
