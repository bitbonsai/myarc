#ifndef MYARC_SIDEBAR_VIEW_H_
#define MYARC_SIDEBAR_VIEW_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/sequence_checker.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/view.h"

namespace views {
class MenuRunner;
class ScrollView;
}  // namespace views

namespace myarc::foundation {
struct ItemRecord;
struct SidebarResult;
}  // namespace myarc::foundation

namespace myarc {

// Presentation only, on the UI sequence. Delegate outlives this view. The
// parent owns Profile/window scope, Rust mutations, browser tabs, dialogs and
// failures.
class SidebarView final : public views::View,
                          public views::ContextMenuController,
                          public ui::SimpleMenuModel::Delegate {
  METADATA_HEADER(SidebarView, views::View)

 public:
  struct ItemPresentation {
    std::u16string title;
    ui::ImageModel icon;  // Parent supplies a favicon, preferably 16 or 20 DIP.
    bool is_live = false;
  };

  class Delegate {
   public:
    virtual ~Delegate() = default;
    // Lookup only: never open a page or fetch a favicon while rendering.
    virtual ItemPresentation GetItemPresentation(
        const foundation::ItemRecord& item) const = 0;
    // IDs are Rust IDs in this view's parent-owned window, not native tab IDs.
    // Resolve the saved canonical URL in the controller for dormant items.
    virtual void ActivateItem(uint64_t item_id) = 0;
    virtual void CloseLiveItem(uint64_t item_id) = 0;
    // Parent resolves the canonical URL and reports errors. Unsave is explicit
    // removal of saved data, distinct from closing a live page.
    virtual void PinItem(uint64_t item_id) = 0;
    virtual void FavoriteItem(uint64_t item_id) = 0;
    virtual void UnsaveItem(uint64_t item_id) = 0;
    virtual void SelectSpace(uint64_t space_id) = 0;
    virtual void CreateSpace() = 0;
    virtual void RenameSpace(uint64_t space_id) = 0;
    virtual void NewTab() = 0;  // Parent opens its real New Tab/palette flow.
  };

  explicit SidebarView(Delegate& delegate);
  ~SidebarView() override;

  // Copies display values/IDs, retains no Rust borrows. Sections and order come
  // directly from Rust. Call again when model or native title/favicon changes.
  // Optional status explains storage failures without claiming durable success.
  void SetModel(const foundation::SidebarResult& model,
                const std::u16string& status = {});
  // Drops stale controls while loading or after a read/snapshot failure. Parent
  // owns recovery/retry UI; SetModel restores controls once usable.
  void SetUnavailable(const std::u16string& reason);

 private:
  enum class FocusAction {
    kActivate,
    kClose,
    kRename,
    kNewTab,
    kSpaces,
    kCreate
  };
  struct ItemMenuState {
    bool is_pin;
    bool is_favorite;
    bool is_live;
    bool operator==(const ItemMenuState&) const = default;
  };
  struct FocusTarget {
    FocusAction action;
    uint64_t id;
    raw_ptr<views::View> view;
    std::optional<ItemMenuState> item_menu;
  };

  void TrackFocus(views::View* view,
                  FocusAction action,
                  uint64_t id = 0,
                  std::optional<ItemMenuState> item_menu = std::nullopt);
  void ShowContextMenuForViewImpl(
      views::View* source,
      const gfx::Point& point,
      ui::mojom::MenuSourceType source_type) override;
  void ExecuteCommand(int command_id, int event_flags) override;
  std::unique_ptr<views::View> MakeItem(const foundation::ItemRecord& item,
                                        bool tile,
                                        bool selected);

  const raw_ref<Delegate> delegate_;
  std::vector<FocusTarget> focus_targets_;
  raw_ptr<views::ScrollView> spaces_scroll_ = nullptr;
  uint64_t current_space_id_ = 0;
  uint64_t menu_space_id_ = 0;
  uint64_t menu_item_id_ = 0;
  ItemMenuState menu_state_ = {};
  // Stable sidebar owns both; no menu callback or anchor retains a child row.
  // Runner must be destroyed before its model and before children.
  std::unique_ptr<ui::SimpleMenuModel> menu_model_;
  std::unique_ptr<views::MenuRunner> menu_runner_;
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace myarc

#endif  // MYARC_SIDEBAR_VIEW_H_
