#include "myarc/spaces_index.h"

#include <optional>
#include <utility>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace myarc {
namespace {

bool ValidText(std::string_view text, size_t limit) {
  return text.size() <= limit && base::IsStringUTF8(text);
}

bool ValidIcon(IndexedSpace::IconKind kind, std::string_view icon) {
  switch (kind) {
    case IndexedSpace::IconKind::kUnset:
      return icon.empty();
    case IndexedSpace::IconKind::kEmoji:
    case IndexedSpace::IconKind::kNamed:
      return !icon.empty() && ValidText(icon, kMaxIndexedIconBytes);
  }
  return false;
}

base::ListValue Encode(base::span<const IndexedSpace> spaces) {
  base::ListValue list;
  for (const auto& space : spaces) {
    if (list.size() == kMaxIndexedSpaces) {
      break;
    }
    if (!space.space_id || space.name.empty() ||
        !ValidText(space.name, kMaxIndexedNameBytes)) {
      continue;
    }
    base::DictValue record;
    record.Set("id", base::NumberToString(space.space_id));
    record.Set("name", space.name);
    if (space.icon_kind != IndexedSpace::IconKind::kUnset &&
        ValidIcon(space.icon_kind, space.icon)) {
      record.Set("icon_kind", static_cast<int>(space.icon_kind));
      record.Set("icon", space.icon);
    }
    list.Append(std::move(record));
  }
  return list;
}

}  // namespace

void RegisterSpacesIndexPref(PrefRegistrySimple& registry) {
  registry.RegisterDictionaryPref(kSpacesIndexPref);
}

bool IsIndexableProfileDir(std::string_view dir) {
  return !dir.empty() && ValidText(dir, kMaxProfileDirBytes) && dir != "." &&
         dir != ".." && dir.find_first_of("/\\") == std::string_view::npos;
}

bool MergeIndexedSpaces(base::DictValue& index,
                        std::string_view profile_dir,
                        base::span<const IndexedSpace> spaces) {
  if (!IsIndexableProfileDir(profile_dir)) {
    return false;
  }
  base::ListValue list = Encode(spaces);
  const base::Value* old = index.Find(profile_dir);
  if (list.empty()) {
    return index.Remove(profile_dir);
  }
  if (old && old->is_list() && old->GetList() == list) {
    return false;
  }
  if (!old && index.size() >= kMaxIndexedProfiles) {
    return false;
  }
  index.Set(profile_dir, std::move(list));
  return true;
}

std::vector<IndexedSpace> DecodeSpacesIndex(const base::DictValue& index) {
  std::vector<IndexedSpace> result;
  size_t profiles = 0;
  for (const auto [dir, value] : index) {
    if (profiles == kMaxIndexedProfiles) {
      break;
    }
    const base::ListValue* list = value.GetIfList();
    if (!list || !IsIndexableProfileDir(dir)) {
      continue;
    }
    ++profiles;
    size_t order = 0;
    for (const base::Value& entry : *list) {
      if (order == kMaxIndexedSpaces) {
        break;
      }
      const base::DictValue* record = entry.GetIfDict();
      const std::string* id = record ? record->FindString("id") : nullptr;
      const std::string* name = record ? record->FindString("name") : nullptr;
      IndexedSpace space;
      // Canonical decimal only: no sign, padding or overflow.
      if (!id || id->size() > 20 ||
          !base::StringToUint64(*id, &space.space_id) || !space.space_id ||
          base::NumberToString(space.space_id) != *id || !name ||
          name->empty() || !ValidText(*name, kMaxIndexedNameBytes)) {
        continue;
      }
      space.profile_dir = dir;
      space.name = *name;
      // A bad icon reads as none, as in the workspace file.
      const std::optional<int> kind = record->FindInt("icon_kind");
      const std::string* icon = record->FindString("icon");
      if (kind && icon &&
          (*kind == static_cast<int>(IndexedSpace::IconKind::kEmoji) ||
           *kind == static_cast<int>(IndexedSpace::IconKind::kNamed)) &&
          ValidIcon(static_cast<IndexedSpace::IconKind>(*kind), *icon)) {
        space.icon_kind = static_cast<IndexedSpace::IconKind>(*kind);
        space.icon = *icon;
      }
      space.order = order++;
      result.push_back(std::move(space));
    }
  }
  return result;
}

void WriteIndexedSpaces(PrefService& local_state,
                        std::string_view profile_dir,
                        base::span<const IndexedSpace> spaces) {
  // Merge into a copy first, so an unchanged index schedules no write.
  base::DictValue index = local_state.GetDict(kSpacesIndexPref).Clone();
  if (MergeIndexedSpaces(index, profile_dir, spaces)) {
    local_state.SetDict(kSpacesIndexPref, std::move(index));
  }
}

std::vector<IndexedSpace> ReadSpacesIndex(const PrefService& local_state) {
  return DecodeSpacesIndex(local_state.GetDict(kSpacesIndexPref));
}

}  // namespace myarc
