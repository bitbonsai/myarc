#ifndef MYARC_SIDEBAR_VIEW_H_
#define MYARC_SIDEBAR_VIEW_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/sequence_checker.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/views/view.h"

namespace myarc::foundation {
struct ItemRecord;
struct SidebarResult;
}  // namespace myarc::foundation

namespace myarc {

// Presentation only, on the UI sequence. Delegate outlives this view. The
// parent owns Profile/window scope, Rust mutations, browser tabs, dialogs and
// failures.
class SidebarView final : public views::View {
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
  struct FocusTarget {
    FocusAction action;
    uint64_t id;
    raw_ptr<views::View> view;
  };

  void TrackFocus(views::View* view, FocusAction action, uint64_t id = 0);
  std::unique_ptr<views::View> MakeItem(const foundation::ItemRecord& item,
                                        bool tile,
                                        bool selected);

  const raw_ref<Delegate> delegate_;
  std::vector<FocusTarget> focus_targets_;
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace myarc

#endif  // MYARC_SIDEBAR_VIEW_H_
