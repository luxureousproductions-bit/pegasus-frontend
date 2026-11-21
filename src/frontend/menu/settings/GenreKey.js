// Pegasus Frontend
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

.pragma library

// Pegasus X: how Library > Genres tells genres apart. Spellings that only
// differ in ways you can't see (or barely) count as one genre: letter case,
// the apostrophe (' ’ ‘ ‛ ʼ ` ´ ′), runs of spaces (non-breaking ones too),
// spaces at the ends, and the spacing around "/". A genre with a "/" stays
// one genre ("Beat'em Up / Fantasy" isn't split). Only for showing: the
// files keep what they have.

var apostrophes = new RegExp("[‘’‛ʼ`´′]", "g");
var spaces = new RegExp("[\\s     　]+", "g");
var slash = new RegExp(" ?/ ?", "g");

// the key a genre is counted under
function key(name) {
    var s = String(name).replace(apostrophes, "'").replace(spaces, " ").trim();
    return s.replace(slash, " / ").toLowerCase();
}

// the genres of the games (one list of names per game) as rows A to Z:
// [{ key, name, count, spellings: [{ name, count }] }]. `name` is the spelling
// most of its games have (ties: A to Z); a game counts once per genre.
// `skip(key)`: genres left out.
function group(gameGenres, skip) {
    var byKey = {};
    for (var i = 0; i < gameGenres.length; i++) {
        var names = gameGenres[i] || [];
        var seen = {};
        for (var n = 0; n < names.length; n++) {
            var name = String(names[n]).trim();
            if (name === "")
                continue;
            var k = key(name);
            if (seen[k] || (skip && skip(k)))
                continue;
            seen[k] = true;
            var g = byKey[k] || (byKey[k] = { "key": k, "count": 0, "by": {} });
            g.count++;
            g.by[name] = (g.by[name] || 0) + 1;
        }
    }
    var out = [];
    for (var gk in byKey) {
        var entry = byKey[gk];
        var spellings = [];
        for (var sp in entry.by)
            spellings.push({ "name": sp, "count": entry.by[sp] });
        spellings.sort(function(a, b) {
            if (a.count !== b.count)
                return b.count - a.count;
            return a.name < b.name ? -1 : a.name > b.name ? 1 : 0;
        });
        out.push({ "key": gk, "name": spellings[0].name, "count": entry.count, "spellings": spellings });
    }
    out.sort(function(a, b) {
        var c = a.name.toLowerCase().localeCompare(b.name.toLowerCase());
        return c !== 0 ? c : (a.name < b.name ? -1 : a.name > b.name ? 1 : 0);
    });
    return out;
}

// a spelling with its character codes, for the log ("Beat’em" -> B 42, ’ 2019...)
function codes(name) {
    var out = [];
    var s = String(name);
    for (var i = 0; i < s.length; i++) {
        var hex = s.charCodeAt(i).toString(16).toUpperCase();
        while (hex.length < 4)
            hex = "0" + hex;
        out.push("U+" + hex);
    }
    return out.join(" ");
}
