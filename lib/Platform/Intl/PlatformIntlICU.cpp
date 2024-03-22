/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "hermes/Platform/Intl/PlatformIntl.h"

#include <deque>
#include <string>
#include <optional>
#include <unordered_map>
#include "llvh/Support/ConvertUTF.h"
#include "unicode/uloc.h"

using namespace ::facebook;
using namespace ::hermes;

namespace hermes {
namespace platform_intl {

namespace {
// https://402.ecma-international.org/8.0/#sec-bestavailablelocale
std::optional<std::u16string> bestAvailableLocale(
    const std::vector<std::u16string> &availableLocales,
    const std::u16string &locale) {
  // 1. Let candidate be locale
  std::u16string candidate = locale;

  // 2. Repeat
  while (true) {
    // a. If availableLocales contains an element equal to candidate, return
    // candidate.
    if (llvh::find(availableLocales, candidate) != availableLocales.end())
      return candidate;
    // b. Let pos be the character index of the last occurrence of "-" (U+002D)
    // within candidate.
    size_t pos = candidate.rfind(u'-');

    // ...If that character does not occur, return undefined.
    if (pos == std::u16string::npos)
      return std::nullopt;

    // c. If pos ≥ 2 and the character "-" occurs at index pos-2 of candidate,
    // decrease pos by 2.
    if (pos >= 2 && candidate[pos - 2] == '-')
      pos -= 2;

    // d. Let candidate be the substring of candidate from position 0,
    // inclusive, to position pos, exclusive.
    candidate.resize(pos);
  }
}

// https://402.ecma-international.org/8.0/#sec-lookupsupportedlocales
std::vector<std::u16string> lookupSupportedLocales(
    const std::vector<std::u16string> &availableLocales,
    const std::vector<std::u16string> &requestedLocales) {
  // 1. Let subset be a new empty List.
  std::vector<std::u16string> subset;
  // 2. For each element locale of requestedLocales in List order, do
  for (const std::u16string &locale : requestedLocales) {
    // a. Let noExtensionsLocale be the String value that is locale with all
    // Unicode locale extension sequences removed.
    // We can skip this step, see the comment in lookupMatcher.
    // b. Let availableLocale be BestAvailableLocale(availableLocales,
    // noExtensionsLocale).
    std::optional<std::u16string> availableLocale =
        bestAvailableLocale(availableLocales, locale);
    // c. If availableLocale is not undefined, append locale to the end of
    // subset.
    if (availableLocale) {
      subset.push_back(locale);
    }
  }
  // 3. Return subset.
  return subset;
}

std::optional<bool> getOptionBool(
    vm::Runtime &runtime,
    const Options &options,
    const std::u16string &property,
    std::optional<bool> fallback) {
  //  1. Assert: Type(options) is Object.
  //  2. Let value be ? Get(options, property).
  auto value = options.find(property);
  //  3. If value is undefined, return fallback.
  if (value == options.end()) {
    return fallback;
  }
  //  8. Return value.
  return value->second.getBool();
}

// Implementation of
// https://402.ecma-international.org/8.0/#sec-todatetimeoptions
vm::CallResult<Options> toDateTimeOptions(
    vm::Runtime &runtime,
    Options options,
    std::u16string_view required,
    std::u16string_view defaults) {
  // 1. If options is undefined, let options be null; otherwise let options be ?
  // ToObject(options).
  // 2. Let options be OrdinaryObjectCreate(options).
  // 3. Let needDefaults be true.
  bool needDefaults = true;
  // 4. If required is "date" or "any", then
  if (required == u"date" || required == u"any") {
    // a. For each property name prop of « "weekday", "year", "month", "day" »,
    // do
    // TODO(T116352920): Make this a std::u16string props[] once we have
    // constexpr std::u16string.
    static constexpr std::u16string_view props[] = {
        u"weekday", u"year", u"month", u"day"};
    for (const auto &prop : props) {
      // i. Let value be ? Get(options, prop).
      if (options.find(std::u16string(prop)) != options.end()) {
        // ii. If value is not undefined, let needDefaults be false.
        needDefaults = false;
      }
    }
  }
  // 5. If required is "time" or "any", then
  if (required == u"time" || required == u"any") {
    // a. For each property name prop of « "dayPeriod", "hour", "minute",
    // "second", "fractionalSecondDigits" », do
    static constexpr std::u16string_view props[] = {
        u"dayPeriod", u"hour", u"minute", u"second", u"fractionalSecondDigits"};
    for (const auto &prop : props) {
      // i. Let value be ? Get(options, prop).
      if (options.find(std::u16string(prop)) != options.end()) {
        // ii. If value is not undefined, let needDefaults be false.
        needDefaults = false;
      }
    }
  }
  // 6. Let dateStyle be ? Get(options, "dateStyle").
  auto dateStyle = options.find(u"dateStyle");
  // 7. Let timeStyle be ? Get(options, "timeStyle").
  auto timeStyle = options.find(u"timeStyle");
  // 8. If dateStyle is not undefined or timeStyle is not undefined, let
  // needDefaults be false.
  if (dateStyle != options.end() || timeStyle != options.end()) {
    needDefaults = false;
  }
  // 9. If required is "date" and timeStyle is not undefined, then
  if (required == u"date" && timeStyle != options.end()) {
    // a. Throw a TypeError exception.
    return runtime.raiseTypeError(
        "Unexpectedly found timeStyle option for \"date\" property");
  }
  // 10. If required is "time" and dateStyle is not undefined, then
  if (required == u"time" && dateStyle != options.end()) {
    // a. Throw a TypeError exception.
    return runtime.raiseTypeError(
        "Unexpectedly found dateStyle option for \"time\" property");
  }
  // 11. If needDefaults is true and defaults is either "date" or "all", then
  if (needDefaults && (defaults == u"date" || defaults == u"all")) {
    // a. For each property name prop of « "year", "month", "day" », do
    static constexpr std::u16string_view props[] = {u"year", u"month", u"day"};
    for (const auto &prop : props) {
      // i. Perform ? CreateDataPropertyOrThrow(options, prop, "numeric").
      options.emplace(prop, Option(std::u16string(u"numeric")));
    }
  }
  // 12. If needDefaults is true and defaults is either "time" or "all", then
  if (needDefaults && (defaults == u"time" || defaults == u"all")) {
    // a. For each property name prop of « "hour", "minute", "second" », do
    static constexpr std::u16string_view props[] = {
        u"hour", u"minute", u"second"};
    for (const auto &prop : props) {
      // i. Perform ? CreateDataPropertyOrThrow(options, prop, "numeric").
      options.emplace(prop, Option(std::u16string(u"numeric")));
    }
  }
  // 13. return options
  return options;
}

vm::CallResult<std::u16string> UTF8toUTF16(
    vm::Runtime &runtime,
    std::string_view in) {
  std::u16string out;
  size_t length = in.length();
  out.resize(length);
  const llvh::UTF8 *sourceStart = reinterpret_cast<const llvh::UTF8 *>(&in[0]);
  const llvh::UTF8 *sourceEnd = sourceStart + length;
  llvh::UTF16 *targetStart = reinterpret_cast<llvh::UTF16 *>(&out[0]);
  llvh::UTF16 *targetEnd = targetStart + out.size();
  llvh::ConversionResult convRes = ConvertUTF8toUTF16(
      &sourceStart,
      sourceEnd,
      &targetStart,
      targetEnd,
      llvh::lenientConversion);
  if (convRes != llvh::ConversionResult::conversionOK) {
    return runtime.raiseRangeError("utf8 to utf16 conversion failed");
  }
  out.resize(reinterpret_cast<char16_t *>(targetStart) - &out[0]);
  return out;
}

vm::CallResult<std::string> UTF16toUTF8(
    vm::Runtime &runtime,
    std::u16string in) {
  std::string out;
  size_t length = in.length();
  out.resize(length);
  const llvh::UTF16 *sourceStart =
      reinterpret_cast<const llvh::UTF16 *>(&in[0]);
  const llvh::UTF16 *sourceEnd = sourceStart + length;
  llvh::UTF8 *targetStart = reinterpret_cast<llvh::UTF8 *>(&out[0]);
  llvh::UTF8 *targetEnd = targetStart + out.size();
  llvh::ConversionResult convRes = ConvertUTF16toUTF8(
      &sourceStart,
      sourceEnd,
      &targetStart,
      targetEnd,
      llvh::lenientConversion);
  if (convRes != llvh::ConversionResult::conversionOK) {
    return runtime.raiseRangeError("utf16 to utf8 conversion failed");
  }
  out.resize(reinterpret_cast<char *>(targetStart) - &out[0]);
  return out;
}

// roughly translates to
// https://tc39.es/ecma402/#sec-canonicalizeunicodelocaleid while doing some
// minimal tag validation
vm::CallResult<std::u16string> canonicalizeUnicodeLocaleID(
    vm::Runtime &runtime,
    const std::u16string &locale) {
  if (locale.length() == 0) {
    return runtime.raiseRangeError("RangeError: Invalid language tag");
  }

  auto conversion = UTF16toUTF8(runtime, locale);
  const char *locale8 = conversion.getValue().c_str();

  UErrorCode status = U_ZERO_ERROR;
  int32_t parsedLength = 0;
  char localeID[ULOC_FULLNAME_CAPACITY] = {0};
  char languageTag[ULOC_FULLNAME_CAPACITY] = {0};

  int32_t forLangTagResultLength = uloc_forLanguageTag(
      locale8, localeID, ULOC_FULLNAME_CAPACITY, &parsedLength, &status);
  if (forLangTagResultLength < 0 ||
      parsedLength < static_cast<int32_t>(locale.length()) ||
      status == U_ILLEGAL_ARGUMENT_ERROR) {
    return runtime.raiseRangeError(
        vm::TwineChar16("Invalid language tag: ") + vm::TwineChar16(locale8));
  }

  int32_t toLangTagResultLength = uloc_toLanguageTag(
      localeID, languageTag, ULOC_FULLNAME_CAPACITY, true, &status);
  if (toLangTagResultLength <= 0) {
    return runtime.raiseRangeError(
        vm::TwineChar16("Invalid language tag: ") + vm::TwineChar16(locale8));
  }

  return UTF8toUTF16(runtime, languageTag);
}

// https://tc39.es/ecma402/#sec-canonicalizelocalelist
vm::CallResult<std::vector<std::u16string>> canonicalizeLocaleList(
    vm::Runtime &runtime,
    const std::vector<std::u16string> &locales) {
  // 1. If locales is undefined, then a. Return a new empty list
  if (locales.empty()) {
    return std::vector<std::u16string>{};
  }
  // 2. Let seen be a new empty List
  std::vector<std::u16string> seen = std::vector<std::u16string>{};

  // 3. If Type(locales) is String or Type(locales) is Object and locales has an
  // [[InitializedLocale]] internal slot, then
  // 4. Else
  //  > Windows/Apple don't support Locale object -
  //  https://tc39.es/ecma402/#locale-objects > As of now, 'locales' can only be
  //  a string list/array. Validation occurs in NormalizeLangugeTag for windows.
  //  > This function just takes a vector of strings.
  // 5-7. Let len be ? ToLength(? Get(O, "length")). Let k be 0. Repeat, while k
  // < len
  for (size_t k = 0; k < locales.size(); k++) {
    // minimal tag validation is done with ICU, ChakraCore\V8 does not do tag
    // validation with ICU, may be missing needed API 7.c.iii.1 Let tag be
    // kValue[[locale]]
    std::u16string tag = locales[k];
    // 7.c.vi Let canonicalizedTag be CanonicalizeUnicodeLocaleID(tag)
    auto canonicalizedTag = canonicalizeUnicodeLocaleID(runtime, tag);
    if (LLVM_UNLIKELY(canonicalizedTag == vm::ExecutionStatus::EXCEPTION)) {
      return vm::ExecutionStatus::EXCEPTION;
    }
    // 7.c.vii. If canonicalizedTag is not an element of seen, append
    // canonicalizedTag as the last element of seen.
    if (std::find(seen.begin(), seen.end(), canonicalizedTag.getValue()) ==
        seen.end()) {
      seen.push_back(std::move(canonicalizedTag.getValue()));
    }
  }
  return seen;
}
} // namespace

// https://tc39.es/ecma402/#sec-intl.getcanonicallocales
vm::CallResult<std::vector<std::u16string>> getCanonicalLocales(
    vm::Runtime &runtime,
    const std::vector<std::u16string> &locales) {
  return canonicalizeLocaleList(runtime, locales);
}

// Not yet implemented.
vm::CallResult<std::u16string> toLocaleLowerCase(
    vm::Runtime &runtime,
    const std::vector<std::u16string> &locales,
    const std::u16string &str) {
  return std::u16string(u"lowered");
}
// Not yet implemented.
vm::CallResult<std::u16string> toLocaleUpperCase(
    vm::Runtime &runtime,
    const std::vector<std::u16string> &locales,
    const std::u16string &str) {
  return std::u16string(u"uppered");
}

namespace {
struct CollatorDummy : Collator {
  CollatorDummy(const char16_t *l) : locale(l) {}
  std::u16string locale;
};
} // namespace

Collator::Collator() = default;
Collator::~Collator() = default;

vm::CallResult<std::vector<std::u16string>> Collator::supportedLocalesOf(
    vm::Runtime &runtime,
    const std::vector<std::u16string> &locales,
    const Options &options) noexcept {
  return std::vector<std::u16string>{u"en-CA", u"de-DE"};
}

vm::CallResult<std::unique_ptr<Collator>> Collator::create(
    vm::Runtime &runtime,
    const std::vector<std::u16string> &locales,
    const Options &options) noexcept {
  return std::make_unique<CollatorDummy>(u"en-US");
}

Options Collator::resolvedOptions() noexcept {
  Options options;
  options.emplace(
      u"locale", Option(static_cast<CollatorDummy *>(this)->locale));
  options.emplace(u"numeric", Option(false));
  return options;
}

double Collator::compare(
    const std::u16string &x,
    const std::u16string &y) noexcept {
  return x.compare(y);
}

namespace {
struct DateTimeFormatDummy : DateTimeFormat {
  DateTimeFormatDummy(const char16_t *l) : locale(l) {}
  std::u16string locale;
};
} // namespace

DateTimeFormat::DateTimeFormat() = default;
DateTimeFormat::~DateTimeFormat() = default;

vm::CallResult<std::vector<std::u16string>> DateTimeFormat::supportedLocalesOf(
    vm::Runtime &runtime,
    const std::vector<std::u16string> &locales,
    const Options &options) noexcept {
  return std::vector<std::u16string>{u"en-CA", u"de-DE"};
}

vm::CallResult<std::unique_ptr<DateTimeFormat>> DateTimeFormat::create(
    vm::Runtime &runtime,
    const std::vector<std::u16string> &locales,
    const Options &options) noexcept {
  return std::make_unique<DateTimeFormatDummy>(u"en-US");
}

Options DateTimeFormat::resolvedOptions() noexcept {
  Options options;
  options.emplace(
      u"locale", Option(static_cast<DateTimeFormatDummy *>(this)->locale));
  options.emplace(u"numeric", Option(false));
  return options;
}

std::u16string DateTimeFormat::format(double jsTimeValue) noexcept {
  auto s = std::to_string(jsTimeValue);
  return std::u16string(s.begin(), s.end());
}

std::vector<std::unordered_map<std::u16string, std::u16string>>
DateTimeFormat::formatToParts(double jsTimeValue) noexcept {
  std::unordered_map<std::u16string, std::u16string> part;
  part[u"type"] = u"integer";
  // This isn't right, but I didn't want to do more work for a stub.
  std::string s = std::to_string(jsTimeValue);
  part[u"value"] = {s.begin(), s.end()};
  return std::vector<std::unordered_map<std::u16string, std::u16string>>{part};
}

namespace {
struct NumberFormatDummy : NumberFormat {
  NumberFormatDummy(const char16_t *l) : locale(l) {}
  std::u16string locale;
};
} // namespace

NumberFormat::NumberFormat() = default;
NumberFormat::~NumberFormat() = default;

vm::CallResult<std::vector<std::u16string>> NumberFormat::supportedLocalesOf(
    vm::Runtime &runtime,
    const std::vector<std::u16string> &locales,
    const Options &options) noexcept {
  return std::vector<std::u16string>{u"en-CA", u"de-DE"};
}

vm::CallResult<std::unique_ptr<NumberFormat>> NumberFormat::create(
    vm::Runtime &runtime,
    const std::vector<std::u16string> &locales,
    const Options &options) noexcept {
  return std::make_unique<NumberFormatDummy>(u"en-US");
}

Options NumberFormat::resolvedOptions() noexcept {
  Options options;
  options.emplace(
      u"locale", Option(static_cast<NumberFormatDummy *>(this)->locale));
  options.emplace(u"numeric", Option(false));
  return options;
}

std::u16string NumberFormat::format(double number) noexcept {
  auto s = std::to_string(number);
  return std::u16string(s.begin(), s.end());
}

std::vector<std::unordered_map<std::u16string, std::u16string>>
NumberFormat::formatToParts(double number) noexcept {
  std::unordered_map<std::u16string, std::u16string> part;
  part[u"type"] = u"integer";
  // This isn't right, but I didn't want to do more work for a stub.
  std::string s = std::to_string(number);
  part[u"value"] = {s.begin(), s.end()};
  return std::vector<std::unordered_map<std::u16string, std::u16string>>{part};
}

} // namespace platform_intl
} // namespace hermes
