#include <iostream>
#include <limits>
#include <string>
#include <utility>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "myarc/bridge.rs.h"
#include "myarc/workspace_service.h"

namespace {

bool CheckWorkspaceRecords() {
  using namespace myarc::foundation;
  auto model = new_workspace();
  auto space = create_space(*model, "Research");
  auto window = create_window(*model, space.id);
  auto item = add_temporary_item(*model, 1);
  if (!space.ok || !window.ok || !item.ok ||
      select_item(*model, window.id, item.id).ok ||
      !select_item(*model, 1, item.id).ok) {
    return false;
  }
  if (!set_placement(*model, item.id, PlacementKind::Pin, 1, true,
                     "https://example.test/saved")
           .ok ||
      !live_tab_closed(*model, item.id).ok ||
      sidebar_snapshot(*model, 1).pins.size() != 1 ||
      set_placement(*model, item.id, PlacementKind::Pin, 1, true,
                    "javascript:alert(1)")
          .ok ||
      !set_placement(*model, item.id, PlacementKind::Favorite, 1, false, "")
           .ok ||
      !select_item(*model, window.id, item.id).ok) {
    return false;
  }
  auto sidebar = sidebar_snapshot(*model, window.id);
  if (!sidebar.ok || sidebar.current_space.id != space.id ||
      sidebar.favorites.size() != 1 || sidebar.selected_item_id != item.id ||
      !sidebar.pins.empty() || !sidebar.temporary.empty() ||
      std::string(sidebar.favorites[0].canonical_url) !=
          "https://example.test/saved") {
    return false;
  }
  auto restored = new_workspace();
  if (!restore_snapshot(*restored, export_snapshot(*model)).ok ||
      sidebar_snapshot(*restored, window.id).selected_item_id != item.id) {
    return false;
  }
  auto invalid = export_snapshot(*model);
  invalid.items[0].placement = static_cast<PlacementKind>(255);
  if (restore_snapshot(*restored, std::move(invalid)).ok ||
      sidebar_snapshot(*restored, window.id).favorites.size() != 1) {
    return false;
  }
  invalid = export_snapshot(*model);
  invalid.version++;
  if (restore_snapshot(*restored, std::move(invalid)).ok)
    return false;
  invalid = export_snapshot(*model);
  invalid.spaces[0].name = std::string(bridge_limits().name_bytes + 1, 'x');
  if (restore_snapshot(*restored, std::move(invalid)).ok)
    return false;
  invalid = export_snapshot(*model);
  for (size_t i = invalid.items.size(); i <= bridge_limits().items; ++i) {
    invalid.items.push_back(ItemRecord{});
  }
  if (restore_snapshot(*restored, std::move(invalid)).ok)
    return false;
  if (!remove_saved_item(*model, item.id).ok ||
      sidebar_snapshot(*model, window.id).selected_item_id != 0 ||
      !select_space(*model, window.id, 1).ok ||
      !close_window(*model, window.id).ok ||
      sidebar_snapshot(*model, window.id).ok) {
    return false;
  }
  auto temporary = add_temporary_item(*model, 1);
  return temporary.ok && !remove_saved_item(*model, temporary.id).ok &&
         live_tab_closed(*model, temporary.id).ok &&
         sidebar_snapshot(*model, 1).temporary.empty();
}

bool CheckR2Records() {
  using namespace myarc::foundation;
  auto model = new_workspace();
  auto space = create_space(*model, "Other");
  auto first = add_temporary_item(*model, space.id);
  auto second = add_temporary_item(*model, space.id);
  auto orphan = add_temporary_item(*model, 1);
  const uint64_t live[] = {first.id, second.id};
  if (!space.ok || !first.ok || !second.ok || !orphan.ok ||
      !reconcile_live_items(*model, rust::Slice<const uint64_t>(live, 2)).ok ||
      !sidebar_snapshot(*model, 1).temporary.empty() ||
      !reveal_item(*model, 1, first.id).ok ||
      sidebar_snapshot(*model, 1).current_space.id != space.id ||
      !move_item_before(*model, second.id, first.id).ok ||
      sidebar_snapshot(*model, 1).temporary[0].id != second.id ||
      !set_placement(*model, first.id, PlacementKind::Favorite, 1, true,
                     "https://example.test/saved")
           .ok ||
      !unsave_item(*model, first.id, space.id).ok ||
      sidebar_snapshot(*model, 1).temporary[1].has_url ||
      !set_placement(*model, first.id, PlacementKind::Pin, space.id, true,
                     "about:blank")
           .ok ||
      !unsave_item(*model, first.id, 0).ok) {
    return false;
  }
  return !reveal_item(*model, 1, first.id).ok &&
         sidebar_snapshot(*model, 1).temporary.size() == 1;
}

bool CheckNativeSnapshotBounds() {
  using namespace myarc::foundation;
  using myarc::internal::CanonicalizeUrls;
  auto model = new_workspace();
  auto state = export_snapshot(*model);
  state.items.push_back(
      {1, PlacementKind::Pin, 1, true, rust::String("HTTPS://EXAMPLE.TEST")});
  const auto limits = bridge_limits();
  const auto exceed = [](auto& records, size_t limit) {
    while (records.size() <= limit)
      records.push_back({});
  };
  for (int field = 0; field < 3; ++field) {
    auto oversized = state;
    if (field == 0)
      exceed(oversized.spaces, limits.spaces);
    else if (field == 1)
      exceed(oversized.items, limits.items);
    else
      exceed(oversized.windows, limits.windows);
    if (CanonicalizeUrls(oversized) ||
        std::string(oversized.items[0].canonical_url) !=
            "HTTPS://EXAMPLE.TEST") {
      return false;  // Reject counts before traversing/canonicalizing items.
    }
  }
  if (!CanonicalizeUrls(state) ||
      std::string(state.items[0].canonical_url) != "https://example.test/")
    return false;
  state.items[0].has_url = false;
  if (CanonicalizeUrls(state) || state.items[0].canonical_url.empty())
    return false;
  state.items[0].canonical_url = "";
  return CanonicalizeUrls(state);
}

bool CheckMetadataRecords() {
  using namespace myarc::foundation;
  using myarc::internal::DecodeWorkspaceMetadata;
  using myarc::internal::EncodeWorkspaceMetadata;
  auto model = new_workspace();
  // Exercise exact u64 counters/IDs beyond JSON's safe double range.
  auto state = export_snapshot(*model);
  const uint64_t item_id = (uint64_t{1} << 53) + 17;
  state.items.push_back({item_id, PlacementKind::Favorite, 1, true,
                         rust::String("https://example.test/\xc3\xa9")});
  state.next_item_id = std::numeric_limits<uint64_t>::max();
  state.windows[0].selected_item_id = item_id;
  if (!restore_snapshot(*model, state).ok)
    return false;
  const auto encoded = EncodeWorkspaceMetadata(state);
  if (!encoded)
    return false;
  auto decoded = DecodeWorkspaceMetadata(*encoded);
  if (!decoded.snapshot || !decoded.error.empty() ||
      decoded.snapshot->next_item_id != state.next_item_id ||
      decoded.snapshot->items[0].id != item_id ||
      !restore_snapshot(*model, std::move(*decoded.snapshot)).ok) {
    std::cerr << "Metadata round trip failed: " << decoded.error << '\n';
    return false;
  }
  auto rejects = [](const base::DictValue& root) {
    const auto json = base::WriteJson(root);
    if (!json)
      return false;
    auto result = DecodeWorkspaceMetadata(*json);
    const bool rejected = !result.snapshot && !result.error.empty();
    if (!rejected) {
      std::cerr << "Invalid metadata fixture accepted: " << *json << '\n';
      if (result.snapshot && !result.snapshot->items.empty()) {
        std::cerr << "Decoded URL: "
                  << std::string(result.snapshot->items[0].canonical_url)
                  << '\n';
      }
    }
    return rejected;
  };
  auto root = base::JSONReader::ReadDict(*encoded, base::JSON_PARSE_RFC);
  if (!root)
    return false;
  for (const auto& [field, limit] :
       {std::pair{"spaces", bridge_limits().spaces},
        std::pair{"items", bridge_limits().items},
        std::pair{"windows", bridge_limits().windows}}) {
    auto oversized = root->Clone();
    auto* records = oversized.FindList(field);
    while (records->size() <= limit)
      records->Append(base::DictValue());
    if (!rejects(oversized))
      return false;
  }
  // Raw invalid UTF-8 must fail before construction of any rust::String.
  for (const auto* text : {"Everyday", "https://example.test/"}) {
    for (const auto* invalid_utf8 : {"\xff", "\xc3\x28"}) {
      auto invalid_json = *encoded;
      const auto position = invalid_json.find(text);
      if (position == std::string::npos) {
        std::cerr << "Missing UTF-8 fixture text: " << text << '\n';
        return false;
      }
      invalid_json.replace(position, std::string(text).size(), invalid_utf8);
      const auto invalid = DecodeWorkspaceMetadata(invalid_json);
      if (invalid.snapshot || invalid.error.empty()) {
        std::cerr << "Invalid UTF-8 fixture accepted\n";
        return false;
      }
    }
  }
  auto* spaces = root->FindList("spaces");
  (*spaces)[0].GetDict().Set("name",
                             std::string(bridge_limits().name_bytes + 1, 'x'));
  if (!rejects(*root))
    return false;
  (*spaces)[0].GetDict().Set("name", "Everyday");
  root->Set("version", 999);
  if (!rejects(*root))
    return false;
  root->Set("version", static_cast<int>(bridge_limits().version));
  root->Set("future_field", true);
  if (!rejects(*root))
    return false;
  root->Remove("future_field");
  root->Set("next_item_id", 12);  // Numeric IDs must be rejected.
  if (!rejects(*root))
    return false;
  root->Set("next_item_id", "18446744073709551616");  // u64 overflow.
  if (!rejects(*root))
    return false;
  root->Set("next_item_id", "01");
  if (!rejects(*root))
    return false;
  root->Set("next_item_id", "18446744073709551615");
  auto* items = root->FindList("items");
  (*items)[0].GetDict().Set("canonical_url",
                            std::string(bridge_limits().url_bytes + 1, 'x'));
  if (!rejects(*root))
    return false;
  for (const auto* bad_url : {"https://:80/", "https://user:pass@example.test/",
                              "javascript:alert(1)", "https://[broken/"}) {
    (*items)[0].GetDict().Set("canonical_url", bad_url);
    if (!rejects(*root))
      return false;
  }
  // Follow Chromium canonicalization rather than inventing DNS-name policy.
  for (const auto& [input, expected] :
       {std::pair{"HTTPS://EXAMPLE.TEST", "https://example.test/"},
        std::pair{"https://bad host.test/", "https://bad%20host.test/"}}) {
    (*items)[0].GetDict().Set("canonical_url", input);
    const auto canonical_json = base::WriteJson(*root);
    auto canonical = canonical_json ? DecodeWorkspaceMetadata(*canonical_json)
                                    : myarc::internal::MetadataResult{};
    if (!canonical.snapshot ||
        std::string(canonical.snapshot->items[0].canonical_url) != expected) {
      return false;
    }
  }
  (*items)[0].GetDict().Set("canonical_url", "https://example.test/");
  (*items)[0].GetDict().Set("placement", 77);
  if (!rejects(*root))
    return false;
  (*items)[0].GetDict().Set("placement", 2);
  (*items)[0].GetDict().Set("has_url", false);
  if (!rejects(*root))
    return false;
  (*items)[0].GetDict().Set("has_url", true);
  for (size_t i = items->size(); i <= bridge_limits().items; ++i) {
    items->Append(base::DictValue());
  }
  if (!rejects(*root))
    return false;
  auto malformed = DecodeWorkspaceMetadata("{broken");
  auto oversized =
      DecodeWorkspaceMetadata(std::string(20 * 1024 * 1024 + 1, ' '));
  auto deep = DecodeWorkspaceMetadata("[[[[[[[[[[]]]]]]]]]]");
  if (malformed.snapshot || malformed.error.empty() || oversized.snapshot ||
      oversized.error.empty() || deep.snapshot || deep.error.empty()) {
    return false;
  }
  // Syntactically valid records still need Rust semantic validation.
  state.windows[0].selected_item_id = 42;
  const auto semantically_invalid = EncodeWorkspaceMetadata(state);
  if (!semantically_invalid)
    return false;
  decoded = DecodeWorkspaceMetadata(*semantically_invalid);
  return decoded.snapshot &&
         !restore_snapshot(*model, std::move(*decoded.snapshot)).ok &&
         sidebar_snapshot(*model, 1).selected_item_id == item_id;
}

// Space 1 is not special: after it is deleted the sidebar, selection and
// rename follow the remaining Space, and the last Space cannot be deleted.
bool CheckDeleteFirstSpace() {
  using namespace myarc::foundation;
  auto model = new_workspace();
  auto space = create_space(*model, "Second");
  auto item = add_temporary_item(*model, space.id);
  if (!space.ok || !item.ok || !select_space(*model, 1, space.id).ok ||
      !select_item(*model, 1, item.id).ok) {
    return false;
  }
  auto deleted = delete_space(*model, 1);
  auto sidebar = sidebar_snapshot(*model, 1);
  if (!deleted.ok || deleted.id != space.id || !sidebar.ok ||
      sidebar.current_space.id != space.id || sidebar.spaces.size() != 1 ||
      sidebar.temporary.size() != 1 || sidebar.selected_item_id != item.id ||
      std::string(current_name(*model)) != "Second") {
    return false;
  }
  auto renamed = rename_space(*model, space.id, "Renamed");
  if (!renamed.ok || std::string(renamed.name) != "Renamed" ||
      std::string(sidebar_snapshot(*model, 1).current_space.name) !=
          "Renamed" ||
      rename_space(*model, 1, "Gone").ok) {
    return false;
  }
  auto last = delete_space(*model, space.id);
  return !last.ok && !last.error.empty() &&
         sidebar_snapshot(*model, 1).current_space.id == space.id;
}

// Saved titles survive the JSON codec, files without them still load, and
// an unchanged title reports no change.
bool CheckSavedTitles() {
  using namespace myarc::foundation;
  using myarc::internal::DecodeWorkspaceMetadata;
  using myarc::internal::EncodeWorkspaceMetadata;
  auto model = new_workspace();
  auto item = add_temporary_item(*model, 1);
  if (!item.ok || set_title(*model, item.id, "Temporary").ok ||
      !set_placement(*model, item.id, PlacementKind::Favorite, 1, true,
                     "https://example.test/")
           .ok) {
    return false;
  }
  auto changed = set_title(*model, item.id, "Example");
  auto same = set_title(*model, item.id, "Example");
  if (!changed.ok || changed.id != 1 || !same.ok || same.id != 0)
    return false;
  const auto encoded = EncodeWorkspaceMetadata(export_snapshot(*model));
  auto decoded = encoded ? DecodeWorkspaceMetadata(*encoded)
                         : myarc::internal::MetadataResult{};
  if (!decoded.snapshot ||
      std::string(decoded.snapshot->items[0].title) != "Example") {
    return false;
  }
  auto root = base::JSONReader::ReadDict(*encoded, base::JSON_PARSE_RFC);
  auto* items = root ? root->FindList("items") : nullptr;
  if (!items)
    return false;
  const auto decode = [&root] {
    const auto json = base::WriteJson(*root);
    return json ? DecodeWorkspaceMetadata(*json)
                : myarc::internal::MetadataResult{};
  };
  (*items)[0].GetDict().Remove("title");
  auto legacy = decode();
  if (!legacy.snapshot || !legacy.snapshot->items[0].title.empty() ||
      !restore_snapshot(*model, std::move(*legacy.snapshot)).ok) {
    return false;
  }
  (*items)[0].GetDict().Set("title", 7);
  if (decode().snapshot)
    return false;
  (*items)[0].GetDict().Set("title",
                            std::string(bridge_limits().title_bytes + 1, 'x'));
  return !decode().snapshot;
}

}  // namespace

int main() {
  auto workspace = myarc::foundation::new_workspace();
  auto second = myarc::foundation::new_workspace();
  if (std::string(myarc::foundation::current_name(*workspace)) != "Everyday") {
    return 1;
  }
  const auto renamed =
      myarc::foundation::rename_space(*workspace, 1, "Research");
  if (!renamed.ok || std::string(renamed.name) != "Research") {
    return 2;
  }
  const auto invalid = myarc::foundation::rename_space(*workspace, 1, "");
  if (invalid.ok || std::string(invalid.name) != "Research" ||
      invalid.error.empty()) {
    return 3;
  }
  const auto unknown = myarc::foundation::rename_space(*workspace, 2, "Other");
  if (unknown.ok || !unknown.name.empty()) {
    return 4;
  }
  const auto oversize =
      myarc::foundation::rename_space(*workspace, 1, std::string(129, 'x'));
  if (oversize.ok || std::string(oversize.name) != "Research") {
    return 5;
  }
  if (std::string(myarc::foundation::current_name(*second)) != "Everyday") {
    return 6;
  }
  for (int i = 0; i < 1000; ++i) {
    auto temporary = myarc::foundation::new_workspace();
    if (!myarc::foundation::rename_space(*temporary, 1, "Owned by Rust").ok) {
      return 7;
    }
  }
  if (!CheckWorkspaceRecords()) {
    std::cerr << "Workspace record/transition checks failed\n";
    return 8;
  }
  if (!CheckR2Records()) {
    std::cerr << "R2 transition checks failed\n";
    return 10;
  }
  if (!CheckNativeSnapshotBounds()) {
    std::cerr << "Native snapshot count/URL-presence checks failed\n";
    return 11;
  }
  if (!CheckDeleteFirstSpace()) {
    std::cerr << "Delete-Space-1 checks failed\n";
    return 12;
  }
  if (!CheckMetadataRecords()) {
    std::cerr << "Metadata codec/bounds checks failed\n";
    return 9;
  }
  if (!CheckSavedTitles()) {
    std::cerr << "Saved title checks failed\n";
    return 13;
  }
  std::cout << "7 legacy C++/Rust checks passed, including 1000 ownership "
               "round trips; workspace transitions, Space 1 deletion and "
               "metadata codec and saved title checks passed\n";
  return 0;
}
