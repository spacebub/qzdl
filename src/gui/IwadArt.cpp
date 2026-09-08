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

#include "core/Artwork.h"
#include "core/Md5.h"
#include "core/Paths.h"
#include "core/Text.h"
#include "gui/Convert.h"
#include "gui/IwadArt.h"

namespace {

// Drawn for 4:3 out of non-square pixels, so it is stored a fifth short.
constexpr int SQUASHED = 200;
constexpr int STRETCHED = SQUASHED * 6 / 5;

slint::SharedPixelBuffer<slint::Rgb8Pixel> stretch(
    const slint::SharedPixelBuffer<slint::Rgb8Pixel> &from, const uint32_t height) {
    slint::SharedPixelBuffer<slint::Rgb8Pixel> to(from.width(), height);
    const slint::Rgb8Pixel *source = from.begin();
    slint::Rgb8Pixel *target = to.begin();

    for (uint32_t row = 0; row < height; row++) {
        const float at = ((static_cast<float>(row) + 0.5F)
                          * static_cast<float>(from.height()) / static_cast<float>(height))
            - 0.5F;
        const float clamped = at < 0 ? 0 : at;
        const auto above = static_cast<uint32_t>(clamped);
        const uint32_t below = std::min(above + 1, from.height() - 1);
        const float mix = clamped - static_cast<float>(above);

        for (uint32_t column = 0; column < from.width(); column++) {
            const slint::Rgb8Pixel &one = source[(above * from.width()) + column];
            const slint::Rgb8Pixel &other = source[(below * from.width()) + column];

            target[(row * from.width()) + column] = {
                .r = static_cast<uint8_t>((static_cast<float>(one.r) * (1 - mix))
                                          + (static_cast<float>(other.r) * mix)),
                .g = static_cast<uint8_t>((static_cast<float>(one.g) * (1 - mix))
                                          + (static_cast<float>(other.g) * mix)),
                .b = static_cast<uint8_t>((static_cast<float>(one.b) * (1 - mix))
                                          + (static_cast<float>(other.b) * mix)),
            };
        }
    }

    return to;
}

/*
Averaged over the block each pixel came from rather than sampled out of it: a
picture going down to a third of its size has two rows in three to account for,
and taking one of them is how a thumbnail comes out speckled.
*/
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

// Anything nothing has asked for in this long is not being asked for again.
constexpr auto STALE = std::chrono::hours(24 * 30);

/*
A card's picture is drawn into 244 to 487 points across and 138 down, so past
this there is detail nothing can show and a mod shipping a 1920x1080 splash
costs six megabytes of pixels to draw a thumbnail.
*/
constexpr int WIDEST = 768;

// What every card drawn from may take between them. A picture is held as pixels
// and the far end of the order goes when they no longer fit.
constexpr size_t BUDGET = static_cast<size_t>(64) * 1024 * 1024;

// Between the files a card is drawn from, and between the parts of one name.
constexpr char SEPARATOR = '\n';
constexpr char FIELD = '\x1F';

/*
What a name in the cache ends in. A picture that names its own colours keeps its
own suffix, since Slint reads an image by that and nothing else.
*/
constexpr std::string_view PALETTED = ".title";
constexpr std::string_view PICTURE = ".pix";
constexpr std::string_view NOTHING = ".none";
constexpr std::string_view RESOLVED = ".pick";
constexpr std::string_view HALF_WRITTEN = ".part";

// What a paletted picture is kept behind, so a file left by another build or a
// half-finished write is not read as one.
constexpr std::string_view MAGIC = "ZDLT";
constexpr std::string_view PIXELS = "ZDLP";
constexpr size_t HEADER = MAGIC.size() + (sizeof(std::uint32_t) * 2);

// Bumped whenever what is looked for changes, so an answer kept under the old
// rules is passed over rather than believed.
constexpr std::string_view SCHEME = "4";

// Which game an add-on's colours were borrowed from, where the two cases the
// game cannot answer for are told apart by a word rather than a path.
constexpr std::string_view OWN = "own";
constexpr std::string_view NEIGHBOUR = "dir";

// Which names the file was looked through under, so the two passes below do
// not read one another's answers.
constexpr std::string_view ONLY_TITLE = "hi";
constexpr std::string_view ANY_NAME = "lo";

std::filesystem::path titles() {
    return Paths::dataDirectory() / "titles";
}

std::string nameOf(const std::string &of, const std::string_view suffix) {
    return md5Text(of) + std::string(suffix);
}

// What the file was last seen as. Empty for one that is not there at all, which
// is nothing to keep an answer about.
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

    // A folder's own time moves only for what sits right in it, and a mod laid
    // out like a PK3 keeps its pictures a level down.
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

// Whether the cache holds this, and that it was wanted just now: the pruning
// goes by when a file was last written to.
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
    std::ifstream in(where, std::ios::binary);

    if (!in) {
        return std::nullopt;
    }

    std::ostringstream buffer;

    buffer << in.rdbuf();

    return buffer.str();
}

// Written beside and moved into place, so a reader never meets half of one.
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

// The lump and the colours to read it with, which is everything decoding it
// again needs and a fifth of what the picture itself would take.
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

/*
Which file a card's picture comes out of. Every answer is kept under what it was
read out of rather than under the profile that asked, so a file shared by two
profiles is opened for the first of them and no others.
*/
class Sources {
public:
    Sources(std::string game, std::string stamp)
        : _game(std::move(game)), _stamp(std::move(stamp)) {
    }

    // The name the picture for this file is kept under. Empty for a file with
    // nothing to draw under the names asked about, which is most of them.
    std::string entryFor(const std::string &file, bool over, Artwork::Under under);

    // Whether every answer was written down. One that was not is looked for
    // again next time rather than remembered as nothing.
    [[nodiscard]] bool settled() const { return _settled; }

private:
    std::string madeFor(const std::string &file, const std::string &identity,
                        const std::string &colours, const std::string &tag,
                        Artwork::Under under);

    bool put(const std::string &name, std::string_view bytes);

    // Read the once, and only if an add-on turns out to need it.
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

    // Looked through once already, and unchanged since.
    if (kept(nameOf(identity, NOTHING))) {
        return {};
    }

    for (const std::string_view suffix : Artwork::suffixes()) {
        if (const std::string name = nameOf(identity, suffix); kept(name)) {
            return name;
        }
    }

    // Drawn against colours of its own, so what was kept of it holds whatever
    // game it is loaded over.
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

    // Nothing under any of the names, or something that is not a picture,
    // which is as much of an answer.
    if (width == 0) {
        put(nameOf(identity, NOTHING), {});

        return {};
    }

    if (title.image) {
        /*
        Small enough to draw as it is, and the file it came in is smaller than
        its own pixels would be, so it is kept exactly as it lay.
        */
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

    /*
    A picture and no colours to draw it with is nothing to keep: the game this
    is loaded over may well carry them by the time it is asked for again. Nor
    is one the colours it has cannot draw.
    */
    if (title.palette.empty() || Artwork::decode(title).empty()) {
        return {};
    }

    const std::string name =
        nameOf(identity + FIELD + (title.own ? std::string(OWN) : tag), PALETTED);

    return put(name, blobOf(title)) ? name : std::string();
}

// Every file the card is drawn from and what each was last seen as: change any
// one of them and the answer is looked for again rather than remembered.
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

/*
The last add-on carrying a title screen wins it, since the port is handed them
in order and draws the last one it was given. A real title screen anywhere in
the list beats one drawn under a lesser name, though: a map pack replaces
INTERPIC and CREDIT as a matter of course, and one loaded after a conversion
would otherwise take the card off it. The game answers for the rest.
*/
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

    // An answer reached past a failed write is not one to keep.
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

slint::Image IwadArt::of(const std::string &key) {
    if (key.empty()) {
        return {};
    }

    _seen = _revision;

    if (const auto found = _cards.find(key); found != _cards.end()) {
        // Asked for just now, so it is the last of them that should be let go.
        found->second.seen = _revision;
        _order.splice(_order.begin(), _order, found->second.at);

        return imageOf(found->second.name);
    }

    std::string standing;

    /*
    The game's own picture stands in while the add-ons are looked through, so a
    card that had one a moment ago still has one now. It is also the answer
    whenever no add-on carries a picture of its own, which is the common way.
    */
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

slint::Image IwadArt::imageOf(const std::string &name) const {
    const auto found = _pictures.find(name);

    return found == _pictures.end() ? slint::Image() : found->second.image;
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

// From the far end of the order, sparing what is on screen and what is still
// being read: either would only be asked for again.
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

    // One thread: reading is all disk, and several would queue on the same one.
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

        // Nothing read here is worth the process for: a lump too big to copy
        // is one card left blank rather than a launcher that ends.
        try {
            done = read(key);
        } catch (...) {
            done.key = key;
        }

        // Back to the drawing thread, the only one that may make a picture.
        slint::invoke_from_event_loop([this, done = std::move(done)] { deliver(done); });
    }
}

void IwadArt::deliver(const Read &done) {
    const auto found = _cards.find(done.key);

    if (found == _cards.end()) {
        return;
    }

    Card &card = found->second;

    if (!done.name.empty() && !_pictures.contains(done.name)) {
        slint::Image made;

        if (!done.kept.empty()) {
            made = slint::Image::load_from_path(Convert::fromPath(done.kept));

            if (made.size().height == SQUASHED) {
                // Stretching reads the pixels back, which only this build's renderer
                // may have been able to decode.
                if (std::optional<slint::SharedPixelBuffer<slint::Rgb8Pixel>> pixels = made.to_rgb8()) {
                    made = slint::Image(stretch(*pixels, STRETCHED));
                }
            }
        } else if (!done.pixels.empty()) {
            const slint::SharedPixelBuffer<slint::Rgb8Pixel> pixels(
                static_cast<uint32_t>(done.width), static_cast<uint32_t>(done.height),
                reinterpret_cast<const slint::Rgb8Pixel *>(done.pixels.data()));

            made = done.height == SQUASHED
                ? slint::Image(stretch(pixels, STRETCHED))
                : slint::Image(pixels);
        }

        const slint::Size<uint32_t> size = made.size();
        const size_t bytes = static_cast<size_t>(size.width) * size.height * 4;

        _bytes += bytes;
        _pictures.emplace(done.name, Picture{.image = std::move(made), .bytes = bytes});
    }

    card.pending = false;

    // The stand-in turning out to be the answer is nothing to redraw for.
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

    // Nothing to be drawn from, which only an empty key would give.
    if (parts.empty()) {
        return done;
    }

    const std::string pick = nameOf(identityOf(parts), RESOLVED);
    std::optional<std::string> named = kept(pick) ? readWhole(titles() / pick) : std::nullopt;

    // Looking again costs one file open per add-on at worst, and the answer
    // being gone is the pruning having taken the picture with it.
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

    // Not what was put there, so it is taken away and made again next time.
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
