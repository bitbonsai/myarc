#include <iostream>
#include <limits>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "myarc/bridge.rs.h"
#include "myarc/library_logic.h"
#include "myarc/spaces_index.h"
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
      // The Library's Space cards list Pins of Spaces a window isn't showing.
      !sidebar_snapshot(*model, window.id).pins.empty() ||
      sidebar_snapshot(*model, window.id).space_pins.size() != 1 ||
      sidebar_snapshot(*model, window.id).space_pins[0].owner_id != 1 ||
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
      !sidebar.space_pins.empty() ||
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
  // A newer build's optional keys, at the root or in a record, are ignored.
  {
    auto future = root->Clone();
    future.Set("future_field", true);
    (*future.FindList("spaces"))[0].GetDict().Set("future_field", 1);
    (*future.FindList("items"))[0].GetDict().Set("future_field", "x");
    (*future.FindList("windows"))[0].GetDict().Set("future_field", false);
    const auto json = base::WriteJson(future);
    auto loaded = json ? DecodeWorkspaceMetadata(*json)
                       : myarc::internal::MetadataResult{};
    if (!loaded.snapshot || !loaded.error.empty() ||
        loaded.snapshot->items[0].id != item_id ||
        !restore_snapshot(*new_workspace(), std::move(*loaded.snapshot)).ok) {
      std::cerr << "Unknown metadata keys rejected: " << loaded.error << '\n';
      return false;
    }
  }
  // Known optional keys still need their type.
  (*root->FindList("items"))[0].GetDict().Set("title", 7);
  if (!rejects(*root))
    return false;
  (*root->FindList("items"))[0].GetDict().Remove("title");
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
// A Space's icon survives the bridge, the metadata file and restore, and a
// bad icon fails without changing the stored one.
bool CheckSpaceIcons() {
  using namespace myarc::foundation;
  using myarc::internal::DecodeWorkspaceMetadata;
  using myarc::internal::EncodeWorkspaceMetadata;
  const auto icon_of = [](const Workspace& model, uint64_t id) {
    for (const auto& space : sidebar_snapshot(model, 1).spaces) {
      if (space.id == id) {
        return std::pair{space.icon_kind, std::string(space.icon)};
      }
    }
    return std::pair{SpaceIconKind::Unset, std::string("missing")};
  };
  const std::string books = "\xf0\x9f\x93\x9a";
  auto model = new_workspace();
  auto space =
      create_styled_space(*model, "Reading", SpaceIconKind::Emoji, books, {});
  if (!space.ok ||
      icon_of(*model, space.id) != std::pair{SpaceIconKind::Emoji, books} ||
      create_styled_space(*model, "Bad", SpaceIconKind::Unset, books, {}).ok ||
      create_styled_space(*model, "Bad", SpaceIconKind::Emoji, "a", {}).ok ||
      icon_of(*model, 1) != std::pair{SpaceIconKind::Unset, std::string()}) {
    return false;
  }
  auto named =
      set_space_icon(*model, space.id, SpaceIconKind::Named, "book-open");
  auto same =
      set_space_icon(*model, space.id, SpaceIconKind::Named, "book-open");
  if (!named.ok || named.id != 1 || !same.ok || same.id != 0 ||
      set_space_icon(*model, space.id, SpaceIconKind::Named,
                     std::string(bridge_limits().icon_bytes + 1, 'a'))
          .ok ||
      set_space_icon(*model, space.id, SpaceIconKind::Named, "Book").ok ||
      icon_of(*model, space.id) !=
          std::pair{SpaceIconKind::Named, std::string("book-open")}) {
    return false;
  }
  const auto encoded = EncodeWorkspaceMetadata(export_snapshot(*model));
  if (!encoded) {
    return false;
  }
  auto decoded = DecodeWorkspaceMetadata(*encoded);
  auto restored = new_workspace();
  if (!decoded.snapshot ||
      !restore_snapshot(*restored, std::move(*decoded.snapshot)).ok ||
      icon_of(*restored, space.id) !=
          std::pair{SpaceIconKind::Named, std::string("book-open")} ||
      icon_of(*restored, 1) != std::pair{SpaceIconKind::Unset, std::string()}) {
    std::cerr << "Space icon round trip failed: " << decoded.error << '\n';
    return false;
  }
  // Unset restores the default glyph and leaves no icon keys in the file.
  if (!set_space_icon(*restored, space.id, SpaceIconKind::Unset, "").ok ||
      icon_of(*restored, space.id) !=
          std::pair{SpaceIconKind::Unset, std::string()}) {
    return false;
  }
  const auto cleared = EncodeWorkspaceMetadata(export_snapshot(*restored));
  return cleared && cleared->find("icon_kind") == std::string::npos;
}

// A Space's theme survives the bridge, the metadata file and restore. A bad
// theme fails without changing the stored one, and one in the file reads as
// none.
bool CheckSpaceThemes() {
  using namespace myarc::foundation;
  using myarc::internal::DecodeWorkspaceMetadata;
  using myarc::internal::EncodeWorkspaceMetadata;
  const auto theme_of = [](const Workspace& model, uint64_t id) {
    for (const auto& space : sidebar_snapshot(model, 1).spaces) {
      if (space.id == id) {
        return std::tuple{space.theme.set, space.theme.color,
                          space.theme.opacity};
      }
    }
    return std::tuple{true, uint32_t{0xFFFFFFFF}, uint8_t{0}};
  };
  const SpaceThemeRecord pink{true, 0xF29BBB, 60};
  auto model = new_workspace();
  auto space =
      create_styled_space(*model, "Pink", SpaceIconKind::Unset, "", pink);
  if (!space.ok ||
      theme_of(*model, space.id) != std::tuple{true, 0xF29BBBu, uint8_t{60}} ||
      create_styled_space(*model, "Bad", SpaceIconKind::Unset, "",
                          {true, 0x1000000, 50})
          .ok ||
      theme_of(*model, 1) != std::tuple{false, 0u, uint8_t{0}}) {
    return false;
  }
  auto blue = set_space_theme(*model, 1, {true, 0x6DBAD9, 100});
  auto same = set_space_theme(*model, 1, {true, 0x6DBAD9, 100});
  if (!blue.ok || blue.id != 1 || !same.ok || same.id != 0 ||
      set_space_theme(*model, 1, {true, 0x6DBAD9, 101}).ok ||
      theme_of(*model, 1) != std::tuple{true, 0x6DBAD9u, uint8_t{100}}) {
    return false;
  }
  const auto encoded = EncodeWorkspaceMetadata(export_snapshot(*model));
  if (!encoded) {
    return false;
  }
  auto decoded = DecodeWorkspaceMetadata(*encoded);
  auto restored = new_workspace();
  if (!decoded.snapshot ||
      !restore_snapshot(*restored, std::move(*decoded.snapshot)).ok ||
      theme_of(*restored, space.id) !=
          std::tuple{true, 0xF29BBBu, uint8_t{60}} ||
      theme_of(*restored, 1) != std::tuple{true, 0x6DBAD9u, uint8_t{100}}) {
    std::cerr << "Space theme round trip failed: " << decoded.error << '\n';
    return false;
  }
  // Unset restores the default tint and leaves no theme keys in the file.
  if (!set_space_theme(*restored, 1, {}).ok ||
      !set_space_theme(*restored, space.id, {}).ok ||
      theme_of(*restored, space.id) != std::tuple{false, 0u, uint8_t{0}}) {
    return false;
  }
  const auto cleared = EncodeWorkspaceMetadata(export_snapshot(*restored));
  if (!cleared || cleared->find("theme_") != std::string::npos) {
    return false;
  }
  // A bad theme reads as none and leaves the rest of the file.
  std::string damaged = *encoded;
  const size_t at = damaged.find("\"theme_opacity\":60");
  if (at == std::string::npos) {
    return false;
  }
  damaged.replace(at, 18, "\"theme_opacity\":160");
  auto bad = DecodeWorkspaceMetadata(damaged);
  auto loaded = new_workspace();
  return bad.snapshot &&
         restore_snapshot(*loaded, std::move(*bad.snapshot)).ok &&
         theme_of(*loaded, space.id) == std::tuple{false, 0u, uint8_t{0}} &&
         theme_of(*loaded, 1) == std::tuple{true, 0x6DBAD9u, uint8_t{100}};
}

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

// Spaces reorder in the snapshot order the footer and Ctrl+1..9 read, and
// unknown IDs fail without changing it.
bool CheckMoveSpace() {
  using namespace myarc::foundation;
  auto model = new_workspace();
  auto second = create_space(*model, "Second");
  auto third = create_space(*model, "Third");
  const auto order = [&model] {
    std::vector<uint64_t> ids;
    for (const auto& space : sidebar_snapshot(*model, 1).spaces) {
      ids.push_back(space.id);
    }
    return ids;
  };
  using Ids = std::vector<uint64_t>;
  if (!second.ok || !third.ok || !move_space_before(*model, third.id, 1).ok ||
      order() != Ids{third.id, 1, second.id} ||
      !move_space_before(*model, third.id, 0).ok ||
      order() != Ids{1, second.id, third.id} ||
      !move_space_before(*model, 1, 1).ok ||
      move_space_before(*model, 99, 0).ok ||
      move_space_before(*model, 1, 99).ok ||
      order() != Ids{1, second.id, third.id}) {
    return false;
  }
  // The Library's drag and Move Left/Right helpers.
  using myarc::library::SpaceDropSlot;
  using myarc::library::SpaceMoveBefore;
  const Ids ids = {1, second.id, third.id};
  if (SpaceDropSlot(1, 3, 0, 264) != 1 || SpaceDropSlot(1, 3, 131, 264) != 1 ||
      SpaceDropSlot(1, 3, 133, 264) != 2 ||
      SpaceDropSlot(1, 3, -140, 264) != 0 ||
      SpaceDropSlot(0, 3, -900, 264) != 0 ||
      SpaceDropSlot(0, 3, 9000, 264) != 2 ||
      SpaceDropSlot(2, 0, 50, 264) != 2 || SpaceMoveBefore(ids, 2, 0) != 1 ||
      SpaceMoveBefore(ids, 0, 1) != third.id ||
      SpaceMoveBefore(ids, 0, 2) != 0 ||
      SpaceMoveBefore(ids, 1, 1) != second.id ||
      SpaceMoveBefore(ids, 3, 0) != 0) {
    return false;
  }
  auto restored = new_workspace();
  return move_space_before(*model, second.id, 1).ok &&
         restore_snapshot(*restored, export_snapshot(*model)).ok &&
         sidebar_snapshot(*restored, 1).spaces[0].id == second.id;
}

// Saved titles survive the JSON codec, untitled records omit the key, files
// without it still load, and an unchanged title reports no change.
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
  // Without a title the record keeps the 5 keys older builds require.
  const auto untitled = EncodeWorkspaceMetadata(export_snapshot(*model));
  auto untitled_root =
      untitled ? base::JSONReader::ReadDict(*untitled, base::JSON_PARSE_RFC)
               : std::nullopt;
  auto* untitled_items =
      untitled_root ? untitled_root->FindList("items") : nullptr;
  if (!untitled_items || untitled_items->size() != 1 ||
      (*untitled_items)[0].GetDict().size() != 5 ||
      (*untitled_items)[0].GetDict().contains("title")) {
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

// A temporary item that remembers its page survives the JSON codec and a
// relaunch where no tab matched it; one with nothing to reopen goes.
bool CheckDormantTemporary() {
  using namespace myarc::foundation;
  using myarc::internal::DecodeWorkspaceMetadata;
  using myarc::internal::EncodeWorkspaceMetadata;
  auto model = new_workspace();
  auto kept = add_temporary_item(*model, 1);
  auto blank = add_temporary_item(*model, 1);
  auto saved = add_temporary_item(*model, 1);
  if (!kept.ok || !blank.ok || !saved.ok ||
      !set_placement(*model, saved.id, PlacementKind::Pin, 1, true,
                     "https://example.test/pin")
           .ok ||
      set_temporary_page(*model, saved.id, "https://example.test/", "Pin").ok ||
      set_temporary_page(*model, blank.id, "chrome://newtab/", "New Tab").ok) {
    return false;
  }
  // The full URL is kept, like Chromium's session files. The adapter skips
  // this call when the Profile saves no history.
  constexpr char kPage[] = "https://example.test/a?v=1#t";
  auto changed = set_temporary_page(*model, kept.id, kPage, "Alpha");
  auto same = set_temporary_page(*model, kept.id, kPage, "Alpha");
  if (!changed.ok || changed.id != 1 || !same.ok || same.id != 0)
    return false;
  const auto encoded = EncodeWorkspaceMetadata(export_snapshot(*model));
  auto decoded = encoded ? DecodeWorkspaceMetadata(*encoded)
                         : myarc::internal::MetadataResult{};
  auto relaunched = new_workspace();
  if (!decoded.snapshot ||
      !restore_snapshot(*relaunched, std::move(*decoded.snapshot)).ok ||
      !reconcile_live_items(*relaunched, rust::Slice<const uint64_t>()).ok) {
    return false;
  }
  // A restored tab of that page with a lost binding finds the dormant row.
  const uint64_t live[] = {kept.id};
  if (dormant_temporary_item(*relaunched, 1, "https://example.test/a?v=1",
                             rust::Slice<const uint64_t>()) != kept.id ||
      dormant_temporary_item(*relaunched, 1, "https://example.test/a?v=2",
                             rust::Slice<const uint64_t>()) != 0 ||
      dormant_temporary_item(*relaunched, 1, kPage,
                             rust::Slice<const uint64_t>(live, 1)) != 0) {
    return false;
  }
  const auto temporary = sidebar_snapshot(*relaunched, 1).temporary;
  return temporary.size() == 1 && temporary[0].id == kept.id &&
         temporary[0].has_url &&
         std::string(temporary[0].canonical_url) == kPage &&
         std::string(temporary[0].title) == "Alpha" &&
         sidebar_snapshot(*relaunched, 1).pins.size() == 1;
}

// Archived Tabs survive the JSON codec; an empty archive omits the key so older
// builds still load the file, and a malformed entry is dropped while the rest
// of the file loads. Expired and history-deleted entries go.
bool CheckArchivedTabs() {
  using namespace myarc::foundation;
  using myarc::internal::DecodeWorkspaceMetadata;
  using myarc::internal::EncodeWorkspaceMetadata;
  auto model = new_workspace();
  const auto empty = EncodeWorkspaceMetadata(export_snapshot(*model));
  auto empty_root =
      empty ? base::JSONReader::ReadDict(*empty, base::JSON_PARSE_RFC)
            : std::nullopt;
  if (!empty_root || empty_root->contains("archived"))
    return false;
  auto item = add_temporary_item(*model, 1);
  auto archived = archive_closed_tab(*model, item.id, "https://example.test/",
                                     "Example", 100);
  if (!archived.ok || archived.id != 1)
    return false;
  const auto encoded = EncodeWorkspaceMetadata(export_snapshot(*model));
  auto decoded = encoded ? DecodeWorkspaceMetadata(*encoded)
                         : myarc::internal::MetadataResult{};
  auto restored = new_workspace();
  if (!decoded.snapshot ||
      !restore_snapshot(*restored, std::move(*decoded.snapshot)).ok) {
    return false;
  }
  auto tabs = archived_tabs(*restored, 100);
  if (tabs.size() != 1 || tabs[0].id != item.id || tabs[0].space_id != 1 ||
      std::string(tabs[0].title) != "Example" || tabs[0].archived_secs != 100) {
    return false;
  }
  if (!remove_archived_tab(*restored, item.id).ok ||
      !archived_tabs(*restored, 100).empty() ||
      remove_archived_tab(*restored, item.id).ok) {
    return false;
  }
  auto root = base::JSONReader::ReadDict(*encoded, base::JSON_PARSE_RFC);
  auto* list = root ? root->FindList("archived") : nullptr;
  if (!list || list->size() != 1)
    return false;
  base::DictValue good = (*list)[0].GetDict().Clone();
  (*list)[0].GetDict().Set("url", "javascript:alert(1)");
  base::DictValue malformed;
  malformed.Set("id", "x");
  list->Append(std::move(malformed));
  list->Append(std::move(good));
  const auto bad = base::WriteJson(*root);
  auto lenient = bad ? DecodeWorkspaceMetadata(*bad)
                     : myarc::internal::MetadataResult{};
  if (!lenient.snapshot || lenient.snapshot->archived.size() != 1 ||
      lenient.snapshot->archived[0].id != item.id) {
    return false;
  }
  if (!restore_snapshot(*restored, std::move(*lenient.snapshot)).ok ||
      prune_archived_tabs(*restored, 100).id != 0 ||
      forget_archived_tabs(*restored, {}, true, 0, 99).id != 0) {
    return false;
  }
  rust::Vec<rust::String> urls;
  urls.push_back("https://example.test/");
  if (forget_archived_tabs(*restored, std::move(urls), false, 0, 0).id != 1 ||
      !archived_tabs(*restored, 100).empty()) {
    return false;
  }
  auto expiring = add_temporary_item(*model, 1);
  archive_closed_tab(*model, expiring.id, "https://example.test/b", "", 0);
  return prune_archived_tabs(*model, 100 + 31 * 24 * 60 * 60).id == 2;
}

// Auto Archive's browser-open clock and view times survive the JSON codec, so
// a restart neither resets nor expires them, and an idle item is due after 12
// hours of passes. Bad clock or view values read as unrecorded.
bool CheckAutoArchive() {
  using namespace myarc::foundation;
  using myarc::internal::DecodeWorkspaceMetadata;
  using myarc::internal::EncodeWorkspaceMetadata;
  constexpr uint64_t kPass = 5 * 60;
  constexpr uint64_t kHalfDay = 12 * 60 * 60;
  auto model = new_workspace();
  auto item = add_temporary_item(*model, 1);
  const uint64_t ids[] = {item.id};
  const rust::Slice<const uint64_t> closable(ids, 1);
  auto first = auto_archive(*model, {}, closable, {}, 0);
  auto second = auto_archive(*model, {}, closable, {}, kPass);
  if (!item.ok || !first.changed || !first.due.empty() || !second.changed)
    return false;
  const auto encoded = EncodeWorkspaceMetadata(export_snapshot(*model));
  auto decoded = encoded ? DecodeWorkspaceMetadata(*encoded)
                         : myarc::internal::MetadataResult{};
  auto restored = new_workspace();
  if (!decoded.snapshot || decoded.snapshot->open_secs != 1 + kPass ||
      decoded.snapshot->items.size() != 1 ||
      decoded.snapshot->items[0].last_viewed_secs != 1 ||
      !restore_snapshot(*restored, std::move(*decoded.snapshot)).ok) {
    return false;
  }
  // The first pass after a restart adds nothing for the time it was closed.
  for (uint64_t open = kPass; open < kHalfDay; open += kPass) {
    if (!auto_archive(*restored, {}, closable, {}, open == kPass ? 0 : kPass)
             .due.empty()) {
      return false;
    }
  }
  auto due = auto_archive(*restored, {}, closable, {}, kPass);
  if (due.due.size() != 1 || due.due[0] != item.id)
    return false;
  // A malformed clock or view time does not fail the file: each reads as
  // zero, and the next pass restamps.
  auto root = base::JSONReader::ReadDict(*encoded, base::JSON_PARSE_RFC);
  auto* items = root ? root->FindList("items") : nullptr;
  if (!items || items->size() != 1 ||
      !(*items)[0].GetDict().contains("viewed_secs") ||
      !root->contains("open_secs")) {
    return false;
  }
  (*items)[0].GetDict().Set("viewed_secs", "-1");
  root->Set("open_secs", "x");
  const auto bad = base::WriteJson(*root);
  const auto lenient = bad ? DecodeWorkspaceMetadata(*bad)
                           : myarc::internal::MetadataResult{};
  return lenient.snapshot && lenient.snapshot->open_secs == 0 &&
         lenient.snapshot->items.size() == 1 &&
         lenient.snapshot->items[0].last_viewed_secs == 0;
}

// The "Archive tabs after" choice survives the JSON codec, the 12-hour default
// stays unwritten, a bad value reads as the default, and the pass uses the
// choice. Clear Archive empties the archive and the empty archive persists.
bool CheckArchiveTimerAndClear() {
  using namespace myarc::foundation;
  using myarc::internal::DecodeWorkspaceMetadata;
  using myarc::internal::EncodeWorkspaceMetadata;
  constexpr uint64_t kHour = 60 * 60;
  constexpr uint64_t kWeek = 7 * 24 * kHour;
  auto model = new_workspace();
  const auto plain = EncodeWorkspaceMetadata(export_snapshot(*model));
  auto plain_root =
      plain ? base::JSONReader::ReadDict(*plain, base::JSON_PARSE_RFC)
            : std::nullopt;
  if (archive_interval_secs(*model) != 12 * kHour || !plain_root ||
      plain_root->contains("archive_interval_secs") ||
      set_archive_interval(*model, 6 * kHour).ok ||
      !set_archive_interval(*model, kWeek).ok ||
      archive_interval_secs(*model) != kWeek) {
    return false;
  }
  auto item = add_temporary_item(*model, 1);
  auto gone = add_temporary_item(*model, 1);
  archive_closed_tab(*model, gone.id, "https://example.test/", "", 100);
  const uint64_t ids[] = {item.id};
  const rust::Slice<const uint64_t> closable(ids, 1);
  auto_archive(*model, {}, closable, {}, 0);
  const auto encoded = EncodeWorkspaceMetadata(export_snapshot(*model));
  auto decoded = encoded ? DecodeWorkspaceMetadata(*encoded)
                         : myarc::internal::MetadataResult{};
  auto restored = new_workspace();
  if (!decoded.snapshot || decoded.snapshot->archive_interval_secs != kWeek ||
      !restore_snapshot(*restored, std::move(*decoded.snapshot)).ok ||
      archive_interval_secs(*restored) != kWeek) {
    return false;
  }
  // A day of passes archives nothing on the 7-day timer.
  for (uint64_t open = 0; open < 24 * kHour; open += 5 * 60) {
    if (!auto_archive(*restored, {}, closable, {}, 5 * 60).due.empty())
      return false;
  }
  auto root = base::JSONReader::ReadDict(*encoded, base::JSON_PARSE_RFC);
  if (!root || !root->contains("archive_interval_secs"))
    return false;
  for (const char* bad_value : {"x", "21600"}) {
    root->Set("archive_interval_secs", bad_value);
    const auto bad = base::WriteJson(*root);
    auto lenient = bad ? DecodeWorkspaceMetadata(*bad)
                       : myarc::internal::MetadataResult{};
    auto fallback = new_workspace();
    if (!lenient.snapshot ||
        !restore_snapshot(*fallback, std::move(*lenient.snapshot)).ok ||
        archive_interval_secs(*fallback) != 12 * kHour) {
      return false;
    }
  }
  auto cleared = clear_archived_tabs(*restored);
  if (!cleared.ok || cleared.id != 1 || !archived_tabs(*restored, 100).empty())
    return false;
  const auto empty = EncodeWorkspaceMetadata(export_snapshot(*restored));
  auto again = empty ? DecodeWorkspaceMetadata(*empty)
                     : myarc::internal::MetadataResult{};
  return again.snapshot && again.snapshot->archived.empty() &&
         again.snapshot->items.size() == 1 &&
         clear_archived_tabs(*restored).id == 0;
}

bool CheckDownloadsStack() {
  using myarc::library::AgeLabel;
  const base::Time now = base::Time::UnixEpoch() + base::Days(400);
  for (const auto& [age, label] :
       {std::pair(base::Seconds(-5), u"Just now"),
        std::pair(base::Seconds(59), u"Just now"),
        std::pair(base::Minutes(1), u"1 minute ago"),
        std::pair(base::Minutes(59), u"59 minutes ago"),
        std::pair(base::Hours(1), u"1 hour ago"),
        std::pair(base::Hours(15), u"15 hours ago"),
        std::pair(base::Hours(24), u"1 day ago"),
        std::pair(base::Days(3), u"3 days ago")}) {
    if (AgeLabel(now - age, now) != label) {
      return false;
    }
  }
  struct Download {
    int id;
    bool complete;
    bool operator==(const Download&) const = default;
  };
  // Newest first; 2 is still downloading.
  const std::vector<Download> newest_first = {{1, true}, {2, false}, {3, true},
                                              {4, true}, {5, true},  {6, true}};
  const std::vector<Download> rows = {
      {5, true}, {4, true}, {3, true}, {1, true}};
  return myarc::library::StackRows(newest_first) == rows &&
         myarc::library::StackRows(std::vector<Download>{{1, false}}).empty() &&
         myarc::library::StackRows(std::vector<Download>{{7, true}}) ==
             std::vector<Download>{{7, true}};
}

// The Spaces index round-trips each Profile's Spaces in order, rewrites only
// the named Profile, and reads a tampered Local State dictionary safely.
bool CheckSpacesIndex() {
  using myarc::IndexedSpace;
  using Kind = IndexedSpace::IconKind;
  base::DictValue index;
  const std::vector<IndexedSpace> personal = {
      {.space_id = 1, .name = "Everyday"},
      {.space_id = 3, .name = "Home", .icon_kind = Kind::kEmoji, .icon = "🏠"}};
  const std::vector<IndexedSpace> work = {{.space_id = 2,
                                           .name = "Client",
                                           .icon_kind = Kind::kNamed,
                                           .icon = "book-open"}};
  if (!myarc::MergeIndexedSpaces(index, "Default", personal) ||
      !myarc::MergeIndexedSpaces(index, "Profile 1", work) ||
      // Unchanged entries report no change, so Local State isn't rewritten.
      myarc::MergeIndexedSpaces(index, "Profile 1", work)) {
    return false;
  }
  // Round trip through JSON, as Local State stores it.
  auto json = base::WriteJson(index);
  auto parsed = json ? base::JSONReader::ReadDict(*json, base::JSON_PARSE_RFC)
                     : std::nullopt;
  if (!parsed) {
    return false;
  }
  const std::vector<IndexedSpace> expected = {
      {"Default", 1, "Everyday", Kind::kUnset, "", 0},
      {"Default", 3, "Home", Kind::kEmoji, "🏠", 1},
      {"Profile 1", 2, "Client", Kind::kNamed, "book-open", 0}};
  if (myarc::DecodeSpacesIndex(*parsed) != expected) {
    return false;
  }
  // Reordering one Profile leaves the other alone; emptying it drops it.
  const std::vector<IndexedSpace> reordered = {personal[1], personal[0]};
  if (!myarc::MergeIndexedSpaces(index, "Default", reordered) ||
      myarc::DecodeSpacesIndex(index)[0].space_id != 3 ||
      myarc::DecodeSpacesIndex(index)[2].profile_dir != "Profile 1" ||
      !myarc::MergeIndexedSpaces(index, "Default", {}) ||
      myarc::DecodeSpacesIndex(index).size() != 1) {
    return false;
  }
  // Writers refuse path-like keys and skip invalid Spaces.
  const std::vector<IndexedSpace> bad_spaces = {
      {.space_id = 0, .name = "Zero"},
      {.space_id = 4, .name = ""},
      {.space_id = 5, .name = std::string(129, 'x')},
      {.space_id = 6, .name = "Ok", .icon_kind = Kind::kEmoji, .icon = ""}};
  if (myarc::MergeIndexedSpaces(index, "../Default", personal) ||
      myarc::MergeIndexedSpaces(index, "a/b", personal) ||
      myarc::MergeIndexedSpaces(index, "", personal) ||
      !myarc::MergeIndexedSpaces(index, "Profile 2", bad_spaces)) {
    return false;
  }
  auto decoded = myarc::DecodeSpacesIndex(index);
  if (decoded.size() != 2 || decoded[1].space_id != 6 ||
      decoded[1].icon_kind != Kind::kUnset) {
    return false;
  }
  // A tampered file: wrong types, bad ids, oversized or invalid text, path
  // keys, unknown icon kinds and too many entries.
  auto tampered = base::JSONReader::ReadDict(
      R"({"..": [{"id": "1", "name": "Up"}],
          "x/y": [{"id": "1", "name": "Path"}],
          "Profile 3": "not a list",
          "Profile 4": [7, {"id": 1, "name": "Int id"},
                        {"id": "01", "name": "Padded"},
                        {"id": "-1", "name": "Negative"},
                        {"id": "99999999999999999999", "name": "Overflow"},
                        {"id": "0", "name": "Zero"},
                        {"id": "2"},
                        {"id": "3", "name": ""},
                        {"id": "4", "name": "Icon", "icon_kind": 9,
                         "icon": "x"},
                        {"id": "5", "name": "Good", "icon_kind": 1,
                         "icon": "🚀"}]})",
      base::JSON_PARSE_RFC);
  if (!tampered) {
    return false;
  }
  tampered->Set("Profile 5",
                base::ListValue().Append(base::DictValue().Set("id", "1").Set(
                    "name", std::string(129, 'n'))));
  base::ListValue many;
  for (int i = 1; i <= 70; ++i) {
    many.Append(base::DictValue()
                    .Set("id", base::NumberToString(i))
                    .Set("name", "Space"));
  }
  tampered->Set("Profile 6", std::move(many));
  decoded = myarc::DecodeSpacesIndex(*tampered);
  size_t profile6 = 0;
  for (const auto& space : decoded) {
    profile6 += space.profile_dir == "Profile 6";
  }
  if (decoded.size() != 2 + myarc::kMaxIndexedSpaces ||
      decoded[0].profile_dir != "Profile 4" || decoded[0].space_id != 4 ||
      decoded[0].icon_kind != Kind::kUnset || decoded[1].space_id != 5 ||
      decoded[1].icon != "🚀" || decoded[1].order != 1 ||
      profile6 != myarc::kMaxIndexedSpaces) {
    return false;
  }
  // The writer stops adding Profiles at the cap; readers read no more.
  base::DictValue full;
  for (size_t i = 0; i < myarc::kMaxIndexedProfiles; ++i) {
    if (!myarc::MergeIndexedSpaces(full, "P" + base::NumberToString(i), work)) {
      return false;
    }
  }
  if (myarc::MergeIndexedSpaces(full, "Extra", work) ||
      !myarc::MergeIndexedSpaces(full, "P0", personal)) {
    return false;
  }
  full.Set("Q", base::ListValue().Append(
                    base::DictValue().Set("id", "1").Set("name", "Over")));
  return myarc::DecodeSpacesIndex(full).size() ==
         myarc::kMaxIndexedProfiles + 1;
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
  if (!CheckArchivedTabs()) {
    std::cerr << "Archived Tabs checks failed\n";
    return 14;
  }
  if (!CheckAutoArchive()) {
    std::cerr << "Auto Archive checks failed\n";
    return 15;
  }
  if (!CheckArchiveTimerAndClear()) {
    std::cerr << "Archive timer and Clear Archive checks failed\n";
    return 16;
  }
  if (!CheckDownloadsStack()) {
    std::cerr << "Downloads stack checks failed\n";
    return 17;
  }
  if (!CheckDormantTemporary()) {
    std::cerr << "Dormant temporary item checks failed\n";
    return 18;
  }
  if (!CheckMoveSpace()) {
    std::cerr << "Space reorder checks failed\n";
    return 19;
  }
  if (!CheckSpaceIcons()) {
    std::cerr << "Space icon checks failed\n";
    return 20;
  }
  if (!CheckSpaceThemes()) {
    std::cerr << "Space theme checks failed\n";
    return 22;
  }
  if (!CheckSpacesIndex()) {
    std::cerr << "Spaces index checks failed\n";
    return 21;
  }
  std::cout
      << "7 legacy C++/Rust checks passed, including 1000 ownership "
         "round trips; workspace transitions, Space 1 deletion and "
         "metadata codec, saved title, Archived Tabs, Auto Archive, "
         "archive timer, Clear Archive, downloads stack, dormant "
         "temporary item, Space reorder, Space icon, Space theme and Spaces "
         "index checks "
         "passed\n";
  return 0;
}
