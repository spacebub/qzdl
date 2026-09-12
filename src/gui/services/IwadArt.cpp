/*
 * This file is part of qZDL
 * Copyright (C) 2026  spacebub
 *
 * qZDL is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <optional>
#include <sstream>
#include <string_view>
#include <utility>

#include "core/system/Paths.h"
#include "core/util/Md5.h"
#include "core/util/Text.h"
#include "core/wad/Artwork.h"
#include "gui/app/Shell.h"
#include "gui/services/IwadArt.h"

namespace {

// A 320x200 screen is drawn at 4:3, so it is stretched by a fifth.
constexpr int SQUASHED = 200;
constexpr int STRETCHED = SQUASHED * 6 / 5;

// A 320x200 title is drawn at 4:3, so the rows are resampled on the way in.
BLImage stretched(const BLImage &from, const int height) {
    BLImageData source{};

    if (from.get_data(&source) != BL_SUCCESS || source.size.h <= 0) {
        return from;
    }

    BLImage to;

    if (to.create(source.size.w, height, BL_FORMAT_XRGB32) != BL_SUCCESS) {
        return from;
    }

    BLImageData target{};

    if (to.make_mutable(&target) != BL_SUCCESS) {
        return from;
    }

    const auto *read = static_cast<const uint8_t *>(source.pixel_data);
    auto *write = static_cast<uint8_t *>(target.pixel_data);

    for (int row = 0; row < height; row++) {
        const float at = ((static_cast<float>(row) + 0.5F) * static_cast<float>(source.size.h)
                          / static_cast<float>(height)) - 0.5F;
        const float clamped = at < 0 ? 0 : at;
        const auto above = static_cast<int>(clamped);
        const int below = std::min(above + 1, source.size.h - 1);
        const float mix = clamped - static_cast<float>(above);

        const auto *one = reinterpret_cast<const uint32_t *>(read + (above * source.stride));
        const auto *other = reinterpret_cast<const uint32_t *>(read + (below * source.stride));
        auto *out = reinterpret_cast<uint32_t *>(write + (row * target.stride));

        for (int column = 0; column < source.size.w; column++) {
            const uint32_t left = one[column];
            const uint32_t right = other[column];
            uint32_t blended = 0xff000000;

            for (int shift = 0; shift < 24; shift += 8) {
                const auto below8 = static_cast<float>((left >> shift) & 0xff);
                const auto above8 = static_cast<float>((right >> shift) & 0xff);

                blended |= static_cast<uint32_t>((below8 * (1 - mix)) + (above8 * mix)) << shift;
            }

            out[column] = blended;
        }
    }

    return to;
}

// Three bytes a pixel as the WAD reader hands them over, into what Blend2D draws.
BLImage imageOfPixels(const int width, const int height,
                      const std::vector<std::uint8_t> &pixels) {
    BLImage made;

    if (width <= 0 || height <= 0
        || pixels.size() < static_cast<size_t>(width) * height * 3
        || made.create(width, height, BL_FORMAT_XRGB32) != BL_SUCCESS) {
        return {};
    }

    BLImageData data{};

    if (made.make_mutable(&data) != BL_SUCCESS) {
        return {};
    }

    auto *write = static_cast<uint8_t *>(data.pixel_data);

    for (int row = 0; row < height; row++) {
        auto *out = reinterpret_cast<uint32_t *>(write + (row * data.stride));

        for (int column = 0; column < width; column++) {
            const size_t at = ((static_cast<size_t>(row) * width) + column) * 3;

            out[column] = 0xff000000U | (static_cast<uint32_t>(pixels[at]) << 16)
                | (static_cast<uint32_t>(pixels[at + 1]) << 8)
                | static_cast<uint32_t>(pixels[at + 2]);
        }
    }

    return made;
}

// Box filter, so a downscaled thumbnail does not speckle.
Artwork::Picture fit(Artwork::Picture from, const int longest) {
    const int side = std::max(from.width, from.height);

    if (from.empty() || side <= longest) {
        return from;
    }

    const int width = std::max(1, from.width * longest / side);
    const int height = std::max(1, from.height * longest / side);
    std::vector<std::uint8_t> pixels(static_cast<size_t>(width) * height * 3);

    for (int row = 0; row < height; ++row) {
        const int top = row * from.height / height;
        const int bottom = std::max(top + 1, (row + 1) * from.height / height);

        for (int column = 0; column < width; ++column) {
            const int left = column * from.width / width;
            const int right = std::max(left + 1, (column + 1) * from.width / width);
            unsigned red = 0;
            unsigned green = 0;
            unsigned blue = 0;
            unsigned counted = 0;

            for (int y = top; y < bottom; ++y) {
                for (int x = left; x < right; ++x) {
                    const size_t at = ((static_cast<size_t>(y) * from.width) + x) * 3;

                    red += from.pixels[at];
                    green += from.pixels[at + 1];
                    blue += from.pixels[at + 2];
                    ++counted;
                }
            }

            const size_t to = ((static_cast<size_t>(row) * width) + column) * 3;

            pixels[to] = static_cast<std::uint8_t>(red / counted);
            pixels[to + 1] = static_cast<std::uint8_t>(green / counted);
            pixels[to + 2] = static_cast<std::uint8_t>(blue / counted);
        }
    }

    return {.width = width, .height = height, .pixels = std::move(pixels)};
}

// Cache files untouched this long are pruned.
constexpr auto STALE = std::chrono::hours(24 * 30);

// Cards are at most 487 points across; larger pictures are downscaled before caching.
constexpr int WIDEST = 768;

// In-memory pixel budget; the least recently used go past it.
constexpr size_t BUDGET = static_cast<size_t>(64) * 1024 * 1024;

// Between the files of a key, and between the parts of a cache name.
constexpr char SEPARATOR = '\n';
constexpr char FIELD = '\x1F';

// Cache name suffixes. An image file keeps its own, since it is loaded by suffix.
constexpr std::string_view PALETTED = ".title";
constexpr std::string_view PICTURE = ".pix";
constexpr std::string_view NOTHING = ".none";
constexpr std::string_view RESOLVED = ".pick";
constexpr std::string_view HALF_WRITTEN = ".part";

// Guards against half-written or foreign files.
constexpr std::string_view MAGIC = "ZDLT";
constexpr std::string_view PIXELS = "ZDLP";
constexpr size_t HEADER = MAGIC.size() + (sizeof(std::uint32_t) * 2);

// Bumped when the lookup rules change, invalidating old answers.
constexpr std::string_view SCHEME = "4";

// Where an add-on's palette came from, when not from the game.
constexpr std::string_view OWN = "own";
constexpr std::string_view NEIGHBOUR = "dir";

// Which names were looked for, so the two passes in resolve() stay apart.
constexpr std::string_view ONLY_TITLE = "hi";
constexpr std::string_view ANY_NAME = "lo";

std::filesystem::path titles() {
    return Paths::dataDirectory() / "titles";
}

std::string nameOf(const std::string &of, const std::string_view suffix) {
    return md5Text(of) + std::string(suffix);
}

// Empty for a missing file.
std::string stampOf(const std::filesystem::path &file) {
    std::error_code code;
    const std::filesystem::file_time_type when = std::filesystem::last_write_time(file, code);

    if (code) {
        return {};
    }

    std::string stamp = std::to_string(when.time_since_epoch().count());

    if (!std::filesystem::is_directory(file, code)) {
        const std::uintmax_t size = std::filesystem::file_size(file, code);

        return stamp + FIELD + std::to_string(code ? 0 : size);
    }

    // A folder's own time ignores what is a level down.
    std::vector<std::filesystem::file_time_type::rep> within;

    for (const auto &entry : std::filesystem::directory_iterator(file, code)) {
        within.push_back(entry.last_write_time(code).time_since_epoch().count());
    }

    std::ranges::sort(within);

    for (const auto each : within) {
        stamp += FIELD;
        stamp += std::to_string(each);
    }

    return stamp;
}

// Also touches the file, since pruning goes by write time.
bool kept(const std::string &name) {
    std::error_code code;
    const std::filesystem::path where = titles() / name;

    if (!std::filesystem::exists(where, code) || code) {
        return false;
    }

    std::filesystem::last_write_time(where, std::filesystem::file_time_type::clock::now(), code);

    return true;
}

std::optional<std::string> readWhole(const std::filesystem::path &where) {
    std::ifstream const in(where, std::ios::binary);

    if (!in) {
        return std::nullopt;
    }

    std::ostringstream buffer;

    buffer << in.rdbuf();

    return buffer.str();
}

// Written beside and renamed, so a reader never sees half a file.
bool write(const std::string &name, const std::string_view bytes) {
    const std::filesystem::path where = titles();
    std::error_code code;

    std::filesystem::create_directories(where, code);

    if (code) {
        return false;
    }

    const std::filesystem::path part = where / (name + std::string(HALF_WRITTEN));

    {
        std::ofstream out(part, std::ios::binary | std::ios::trunc);

        if (!out) {
            return false;
        }

        if (!bytes.empty()) {
            out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        }

        if (!out) {
            std::filesystem::remove(part, code);

            return false;
        }
    }

    std::filesystem::rename(part, where / name, code);

    if (code) {
        std::filesystem::remove(part, code);

        return false;
    }

    return true;
}

// Lump plus palette: a fifth of the decoded picture.
std::string blobOf(const Artwork::Title &title) {
    const auto palette = static_cast<std::uint32_t>(title.palette.size());
    const auto lump = static_cast<std::uint32_t>(title.lump.size());
    std::string blob(MAGIC);

    blob.append(reinterpret_cast<const char *>(&palette), sizeof(palette));
    blob.append(reinterpret_cast<const char *>(&lump), sizeof(lump));
    blob += title.palette;
    blob += title.lump;

    return blob;
}

Artwork::Title titleOfBlob(const std::string &blob) {
    std::uint32_t palette = 0;
    std::uint32_t lump = 0;

    if (blob.size() < HEADER || !blob.starts_with(MAGIC)) {
        return {};
    }

    std::memcpy(&palette, blob.data() + MAGIC.size(), sizeof(palette));
    std::memcpy(&lump, blob.data() + MAGIC.size() + sizeof(palette), sizeof(lump));

    if (blob.size() != HEADER + palette + lump) {
        return {};
    }

    return {.lump = blob.substr(HEADER + palette, lump),
            .palette = blob.substr(HEADER, palette)};
}

std::string blobOf(const Artwork::Picture &picture) {
    const auto width = static_cast<std::uint32_t>(picture.width);
    const auto height = static_cast<std::uint32_t>(picture.height);
    std::string blob(PIXELS);

    blob.append(reinterpret_cast<const char *>(&width), sizeof(width));
    blob.append(reinterpret_cast<const char *>(&height), sizeof(height));
    blob.append(reinterpret_cast<const char *>(picture.pixels.data()), picture.pixels.size());

    return blob;
}

Artwork::Picture pictureOfBlob(const std::string &blob) {
    std::uint32_t width = 0;
    std::uint32_t height = 0;

    if (blob.size() < HEADER || !blob.starts_with(PIXELS)) {
        return {};
    }

    std::memcpy(&width, blob.data() + PIXELS.size(), sizeof(width));
    std::memcpy(&height, blob.data() + PIXELS.size() + sizeof(width), sizeof(height));

    const size_t span = static_cast<size_t>(width) * height * 3;

    if (width == 0 || height == 0 || blob.size() != HEADER + span) {
        return {};
    }

    const auto *from = reinterpret_cast<const std::uint8_t *>(blob.data() + HEADER);

    return {.width = static_cast<int>(width),
            .height = static_cast<int>(height),
            .pixels = std::vector<std::uint8_t>(from, from + span)};
}

// Answers are cached by source file, not by the profile asking.
class Sources {
public:
    Sources(std::string game, std::string stamp)
        : _game(std::move(game)), _stamp(std::move(stamp)) {
    }

    // The cache name for this file's picture; empty when it has none.
    std::string entryFor(const std::string &file, bool over, Artwork::Under under);

    // False when a write failed, so nothing is remembered as absent.
    [[nodiscard]] bool settled() const { return _settled; }

private:
    std::string madeFor(const std::string &file, const std::string &identity,
                        const std::string &colours, const std::string &tag,
                        Artwork::Under under);

    bool put(const std::string &name, std::string_view bytes);

    // Read once, only if an add-on needs it.
    const std::string &palette();

    std::string _game;
    std::string _stamp;
    std::string _palette;
    bool _asked{false};
    bool _settled{true};
};

bool Sources::put(const std::string &name, const std::string_view bytes) {
    if (write(name, bytes)) {
        return true;
    }

    _settled = false;

    return false;
}

const std::string &Sources::palette() {
    if (!_asked) {
        _asked = true;

        if (!_game.empty()) {
            _palette = Artwork::paletteOf(_game);
        }
    }

    return _palette;
}

std::string Sources::entryFor(const std::string &file, const bool over,
                              const Artwork::Under under) {
    const std::string stamp = stampOf(file);

    if (stamp.empty()) {
        return {};
    }

    const std::string identity = file + FIELD + stamp + FIELD + std::string(SCHEME) + FIELD
        + std::string(under == Artwork::Under::Title ? ONLY_TITLE : ANY_NAME);

    if (kept(nameOf(identity, NOTHING))) {
        return {};
    }

    for (const std::string_view suffix : Artwork::suffixes()) {
        if (const std::string name = nameOf(identity, suffix); kept(name)) {
            return name;
        }
    }

    // Own palette: valid over any game.
    if (const std::string name = nameOf(identity + FIELD + std::string(OWN), PALETTED);
        kept(name)) {
        return name;
    }

    const std::string colours = over ? palette() : std::string();
    const std::string tag = colours.empty()
        ? std::string(NEIGHBOUR)
        : _game + FIELD + _stamp;

    if (const std::string name = nameOf(identity + FIELD + tag, PALETTED); kept(name)) {
        return name;
    }

    return madeFor(file, identity, colours, tag, under);
}

std::string Sources::madeFor(const std::string &file, const std::string &identity,
                             const std::string &colours, const std::string &tag,
                             const Artwork::Under under) {
    const Artwork::Title title = Artwork::titleOf(file, colours, under);
    int width = 0;
    int height = 0;

    Artwork::measure(title, width, height);

    if (width == 0) {
        put(nameOf(identity, NOTHING), {});

        return {};
    }

    if (title.image) {
        // Small enough to keep as it lay.
        if (std::max(width, height) <= WIDEST) {
            const std::string name = nameOf(identity, Artwork::suffixOf(title));

            return put(name, title.lump) ? name : std::string();
        }

        const Artwork::Picture small = fit(Artwork::decode(title), WIDEST);

        if (small.empty()) {
            put(nameOf(identity, NOTHING), {});

            return {};
        }

        const std::string name = nameOf(identity, PICTURE);

        return put(name, blobOf(small)) ? name : std::string();
    }

    // No palette, or an undecodable one, is not cached: the game over it may supply one later.
    if (title.palette.empty() || Artwork::decode(title).empty()) {
        return {};
    }

    const std::string name =
        nameOf(identity + FIELD + (title.own ? std::string(OWN) : tag), PALETTED);

    return put(name, blobOf(title)) ? name : std::string();
}

// Every file with its stamp, so any change invalidates the answer.
std::string identityOf(const std::vector<std::string> &parts) {
    std::string all(SCHEME);

    for (const std::string &file : parts) {
        all += FIELD;
        all += file;
        all += FIELD;
        all += stampOf(file);
    }

    return all;
}

// The last add-on with a title screen wins. A real title screen anywhere beats a
// lesser name, since map packs replace INTERPIC and CREDIT as a matter of course.
// Failing both, the game's own.
std::string resolve(const std::vector<std::string> &parts, const std::string &pick) {
    Sources sources(parts.front(), stampOf(parts.front()));
    std::string name;

    for (const Artwork::Under under : {Artwork::Under::Title, Artwork::Under::Any}) {
        for (size_t at = parts.size(); at-- > 1 && name.empty();) {
            name = sources.entryFor(parts[at], true, under);
        }

        if (!name.empty()) {
            break;
        }
    }

    if (name.empty() && !parts.front().empty()) {
        name = sources.entryFor(parts.front(), false, Artwork::Under::Any);
    }

    if (sources.settled()) {
        write(pick, name);
    }

    return name;
}

}

void IwadArt::prune() {
    std::error_code code;
    const auto now = std::filesystem::file_time_type::clock::now();

    for (std::filesystem::directory_iterator walk(titles(), code), end;
         walk != end && !code; walk.increment(code)) {
        std::error_code each;

        if (const std::filesystem::file_time_type when = walk->last_write_time(each);
            !each && now - when > STALE) {
            std::filesystem::remove(walk->path(), each);
        }
    }
}

IwadArt::~IwadArt() {
    {
        const std::scoped_lock hold(_guard);

        _stopping = true;
    }

    _wake.notify_one();

    if (_reader.joinable()) {
        _reader.join();
    }
}

BLImage IwadArt::of(const std::string &key) {
    if (key.empty()) {
        return {};
    }

    _seen = _revision;

    if (const auto found = _cards.find(key); found != _cards.end()) {
        found->second.seen = _revision;
        _order.splice(_order.begin(), _order, found->second.at);

        return imageOf(found->second.name);
    }

    std::string standing;

    // The game's picture stands in while the add-ons are read.
    if (const size_t end = key.find(SEPARATOR); end != std::string::npos) {
        if (const auto game = _cards.find(key.substr(0, end)); game != _cards.end()) {
            standing = game->second.name;
        }
    }

    _order.push_front(key);
    _cards.emplace(key, Card{.name = standing, .seen = _revision, .pending = true,
                             .at = _order.begin()});
    take(standing);
    want(key);

    return imageOf(standing);
}

BLImage IwadArt::imageOf(const std::string &name) const {
    const auto found = _pictures.find(name);

    return found == _pictures.end() ? BLImage() : found->second.image;
}

void IwadArt::take(const std::string &name) {
    if (const auto found = _pictures.find(name); found != _pictures.end()) {
        ++found->second.cards;
    }
}

void IwadArt::drop(const std::string &name) {
    const auto found = _pictures.find(name);

    if (found != _pictures.end() && --found->second.cards == 0) {
        _bytes -= found->second.bytes;
        _pictures.erase(found);
    }
}

// Spares what is on screen or still being read.
void IwadArt::evict() {
    auto at = _order.end();

    while (_bytes > BUDGET && at != _order.begin()) {
        --at;

        const auto found = _cards.find(*at);

        if (found != _cards.end()) {
            if (found->second.pending || found->second.seen == _seen) {
                continue;
            }

            drop(found->second.name);
            _cards.erase(found);
        }

        at = _order.erase(at);
    }
}

void IwadArt::want(const std::string &key) {
    {
        const std::scoped_lock hold(_guard);

        _wanted.push_back(key);
    }

    if (!_reader.joinable()) {
        _reader = std::thread([this] { work(); });
    }

    _wake.notify_one();
}

void IwadArt::work() {
    for (;;) {
        std::string key;

        {
            std::unique_lock hold(_guard);

            _wake.wait(hold, [this] { return _stopping || !_wanted.empty(); });

            if (_stopping) {
                return;
            }

            key = std::move(_wanted.front());
            _wanted.pop_front();
        }

        Read done;

        // A bad file blanks one card rather than ending the launcher.
        try {
            done = read(key);
        } catch (...) {
            done.key = key;
        }

        // Decoding and eviction belong to the interface thread; the read does not.
        _shell->post([this, done = std::move(done)] { deliver(done); });
    }
}

void IwadArt::deliver(const Read &done) {
    const auto found = _cards.find(done.key);

    if (found == _cards.end()) {
        return;
    }

    Card &card = found->second;

    if (!done.name.empty() && !_pictures.contains(done.name)) {
        BLImage made;

        if (!done.kept.empty()) {
            if (made.read_from_file(done.kept.string().c_str()) != BL_SUCCESS) {
                made.reset();
            }

            if (made.height() == SQUASHED) {
                made = stretched(made, STRETCHED);
            }
        } else if (!done.pixels.empty()) {
            made = imageOfPixels(done.width, done.height, done.pixels);

            if (done.height == SQUASHED) {
                made = stretched(made, STRETCHED);
            }
        }

        const size_t bytes = static_cast<size_t>(made.width()) * made.height() * 4;

        _bytes += bytes;
        _pictures.emplace(done.name, Picture{.image = std::move(made), .bytes = bytes});
    }

    card.pending = false;

    if (card.name == done.name) {
        return;
    }

    drop(card.name);
    card.name = done.name;
    take(done.name);
    evict();
    ++_revision;

    if (arrived) {
        arrived();
    }
}

IwadArt::Read IwadArt::read(const std::string &key) {
    Read done;

    done.key = key;

    const std::vector<std::string> parts = Text::split(key, SEPARATOR);

    if (parts.empty()) {
        return done;
    }

    const std::string pick = nameOf(identityOf(parts), RESOLVED);
    std::optional<std::string> named = kept(pick) ? readWhole(titles() / pick) : std::nullopt;

    // A pruned picture leaves a dangling answer; resolve again.
    if (!named || (!named->empty() && !kept(*named))) {
        named = resolve(parts, pick);
    }

    if (named->empty()) {
        return done;
    }

    if (!named->ends_with(PALETTED) && !named->ends_with(PICTURE)) {
        done.name = *named;
        done.kept = titles() / *named;

        return done;
    }

    const std::optional<std::string> blob = readWhole(titles() / *named);
    Artwork::Picture picture;

    if (blob) {
        picture = named->ends_with(PICTURE)
            ? pictureOfBlob(*blob)
            : Artwork::decode(titleOfBlob(*blob));
    }

    // Corrupt; remade next time.
    if (picture.empty()) {
        std::error_code code;

        std::filesystem::remove(titles() / *named, code);

        return done;
    }

    done.name = *named;
    done.width = picture.width;
    done.height = picture.height;
    done.pixels = std::move(picture.pixels);

    return done;
}
