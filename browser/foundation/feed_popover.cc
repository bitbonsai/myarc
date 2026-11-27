#include "myarc/feed_popover.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "myarc/feed_viewer.h"
#include "myarc/feeds_service.h"
#include "myarc/icons.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/color/color_id.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace myarc {
namespace {

struct ActivePopover {
  std::unique_ptr<views::Widget> widget;
  std::unique_ptr<views::BubbleDialogDelegate> delegate;
};

ActivePopover& PopoverSlot() {
  static base::NoDestructor<ActivePopover> slot;
  return *slot;
}

void ReleasePopover() {
  ActivePopover& slot = PopoverSlot();
  if (slot.delegate) {
    slot.delegate->SetAnchorView(nullptr);
  }
  slot.widget.reset();
  slot.delegate.reset();
}

std::u16string FormatFeedSubtitle(const GURL& url) {
  std::string out = url.GetHost();
  std::string path = url.GetPath();
  if (!path.empty() && path != "/") {
    out += path;
  }
  return base::UTF8ToUTF16(out);
}

class FeedPopoverContent final : public views::View {
  METADATA_HEADER(FeedPopoverContent, views::View)

 public:
  FeedPopoverContent(content::WebContents* web_contents,
                     std::vector<DiscoveredFeed> feeds)
      : web_contents_(web_contents->GetWeakPtr()), feeds_(std::move(feeds)) {
    SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical, gfx::Insets(), 14));

    auto* header = AddChildView(std::make_unique<views::BoxLayoutView>());
    header->SetOrientation(views::BoxLayout::Orientation::kHorizontal);
    header->SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kCenter);
    header->SetBetweenChildSpacing(8);

    auto* icon = header->AddChildView(std::make_unique<views::ImageView>());
    icon->SetImage(ui::ImageModel::FromVectorIcon(
        kRssIcon, SkColorSetRGB(0x0A, 0x84, 0xFF), 18));

    auto* heading = header->AddChildView(std::make_unique<views::Label>(
        feeds_.size() == 1 ? u"Feed on this site" : u"Feeds on this site"));
    heading->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    heading->SetFontList(heading->font_list().Derive(
        4, gfx::Font::NORMAL, gfx::Font::Weight::SEMIBOLD));

    Profile* profile =
        Profile::FromBrowserContext(web_contents->GetBrowserContext());
    FeedsService* service =
        profile ? &FeedsService::ForProfile(*profile) : nullptr;
    const bool can_subscribe = service && !service->IsEphemeral();

    for (size_t i = 0; i < feeds_.size(); ++i) {
      const DiscoveredFeed& feed = feeds_[i];
      auto* row = AddChildView(std::make_unique<views::View>());
      auto* flex = row->SetLayoutManager(std::make_unique<views::FlexLayout>());
      flex->SetOrientation(views::LayoutOrientation::kHorizontal);
      flex->SetCrossAxisAlignment(views::LayoutAlignment::kCenter);
      flex->SetDefault(views::kMarginsKey, gfx::Insets::TLBR(0, 0, 0, 6));

      auto* info = row->AddChildView(std::make_unique<views::BoxLayoutView>());
      info->SetOrientation(views::BoxLayout::Orientation::kVertical);
      info->SetBetweenChildSpacing(2);
      info->SetProperty(
          views::kFlexBehaviorKey,
          views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                                   views::MaximumFlexSizeRule::kUnbounded));

      const std::string fallback_title =
          feed.title.empty() ? feed.url.GetHost() : feed.title;
      auto* title_label = info->AddChildView(
          std::make_unique<views::Label>(base::UTF8ToUTF16(fallback_title)));
      title_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
      title_label->SetElideBehavior(gfx::ELIDE_TAIL);
      title_label->SetFontList(title_label->font_list().DeriveWithWeight(
          gfx::Font::Weight::MEDIUM));

      auto* sub_label = info->AddChildView(
          std::make_unique<views::Label>(FormatFeedSubtitle(feed.url)));
      sub_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
      sub_label->SetElideBehavior(gfx::ELIDE_TAIL);
      sub_label->SetEnabledColor(ui::kColorSysOnSurfaceSubtle);
      sub_label->SetFontList(sub_label->font_list().DeriveWithSizeDelta(-1));

      auto* view_btn = row->AddChildView(std::make_unique<views::MdTextButton>(
          base::BindRepeating(&FeedPopoverContent::OnViewFeed,
                              base::Unretained(this), i),
          u"View"));
      view_btn->SetStyle(ui::ButtonStyle::kTonal);
      view_btn->SetMinSize(gfx::Size(0, 28));

      if (can_subscribe) {
        const bool subscribed = service->IsSubscribed(feed.url.spec());
        auto* sub_btn = row->AddChildView(std::make_unique<views::MdTextButton>(
            base::BindRepeating(&FeedPopoverContent::OnToggleSubscribe,
                                base::Unretained(this), i),
            subscribed ? u"Subscribed" : u"Subscribe"));
        sub_btn->SetStyle(subscribed ? ui::ButtonStyle::kDefault
                                     : ui::ButtonStyle::kProminent);
        sub_btn->SetMinSize(gfx::Size(0, 28));
        sub_btn->SetProperty(views::kMarginsKey, gfx::Insets());
        subscribe_buttons_.push_back(sub_btn);
      } else {
        view_btn->SetProperty(views::kMarginsKey, gfx::Insets());
        subscribe_buttons_.push_back(nullptr);
      }
    }
  }

  ~FeedPopoverContent() override = default;

 private:
  void OnViewFeed(size_t index) {
    if (!web_contents_ || index >= feeds_.size()) {
      return;
    }
    const GURL feed_url = feeds_[index].url;
    content::WebContents* contents = web_contents_.get();
    if (views::Widget* widget = GetWidget()) {
      widget->Close();
    }
    content::OpenURLParams params(
        feed_url, content::Referrer(), WindowOpenDisposition::kCurrentTab,
        ui::PAGE_TRANSITION_LINK, /*is_renderer_initiated=*/false);
    contents->OpenURL(params, /*navigation_handle_callback=*/{});
  }

  void OnToggleSubscribe(size_t index) {
    if (!web_contents_ || index >= feeds_.size()) {
      return;
    }
    Profile* profile =
        Profile::FromBrowserContext(web_contents_->GetBrowserContext());
    if (!profile) {
      return;
    }
    const DiscoveredFeed& feed = feeds_[index];
    const bool currently_subscribed =
        FeedsService::ForProfile(*profile).IsSubscribed(feed.url.spec());
    const bool next_subscribed = !currently_subscribed;
    TriggerFeedSubscribe(web_contents_.get(), feed.url, feed.title,
                         next_subscribed);
    if (index < subscribe_buttons_.size() && subscribe_buttons_[index]) {
      subscribe_buttons_[index]->SetText(next_subscribed ? u"Subscribed"
                                                         : u"Subscribe");
      subscribe_buttons_[index]->SetStyle(next_subscribed
                                              ? ui::ButtonStyle::kDefault
                                              : ui::ButtonStyle::kProminent);
    }
  }

  base::WeakPtr<content::WebContents> web_contents_;
  std::vector<DiscoveredFeed> feeds_;
  std::vector<raw_ptr<views::MdTextButton>> subscribe_buttons_;
};

BEGIN_METADATA(FeedPopoverContent)
END_METADATA

}  // namespace

void ShowFeedPopover(views::View* anchor, content::WebContents* web_contents) {
  if (PopoverSlot().widget) {
    ReleasePopover();
    return;
  }
  if (!anchor || !web_contents) {
    return;
  }
  const auto& feeds = GetDiscoveredFeeds(web_contents);
  if (feeds.empty()) {
    return;
  }
  auto delegate = std::make_unique<views::BubbleDialogDelegate>(
      anchor, views::BubbleBorder::TOP_LEFT);
  delegate->SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  delegate->SetShowCloseButton(false);
  delegate->set_margins(gfx::Insets::VH(14, 16));
  delegate->set_fixed_width(340);
  delegate->set_corner_radius(14);
  delegate->set_close_on_deactivate(true);
  delegate->RegisterWindowClosingCallback(base::BindOnce([]() {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&ReleasePopover));
  }));
  delegate->SetContentsView(
      std::make_unique<FeedPopoverContent>(web_contents, feeds));
  ActivePopover& slot = PopoverSlot();
  slot.delegate = std::move(delegate);
  slot.widget = views::BubbleDialogDelegate::CreateBubble(slot.delegate.get());
  slot.widget->Show();
}

}  // namespace myarc
