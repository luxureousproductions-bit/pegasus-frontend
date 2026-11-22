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

// -- Change a genre in its games (a genre's page) ------------------------------------------

// a game's genres with every spelling of the genre `fromKey` replaced by
// `to`; its other genres stay as they are, in their order. The genre `to`
// ends up once: where it was first (if the game had it already, in that
// spelling, unless every spelling of it is being replaced: `to` is then a
// spelling of `fromKey` itself)
function replace(names, fromKey, to) {
    var target = String(to).trim();
    var toKey = key(target);
    var out = [];
    var placed = false;
    for (var i = 0; i < names.length; i++) {
        var name = String(names[i]);
        var k = key(name);
        if (k === fromKey)
            name = target;
        else if (k !== toKey) {
            out.push(name);
            continue;
        }
        if (placed)
            continue;
        placed = true;
        out.push(name);
    }
    return out;
}

// the same list (same texts, same order)
function same(a, b) {
    if (a.length !== b.length)
        return false;
    for (var i = 0; i < a.length; i++) {
        if (String(a[i]) !== String(b[i]))
            return false;
    }
    return true;
}

// -- Spellings side by side (a genre's page) ------------------------------------------

function code(ch) {
    var hex = ch.charCodeAt(0).toString(16).toUpperCase();
    while (hex.length < 4)
        hex = "0" + hex;
    return "U+" + hex;
}

// how spelling `a` differs from spelling `b`, character by character: the
// characters both have in the same order (letter case ignored) line up;
// `a` has the others (`onlyA`, positions in a) and lacks `onlyB` (in b);
// `caseA`: lined up, but in the other case
function diff(a, b) {
    a = String(a);
    b = String(b);
    var n = a.length, m = b.length;
    var la = a.toLowerCase(), lb = b.toLowerCase();
    // longest common subsequence (genres are short)
    var len = [];
    for (var i = 0; i <= n; i++) {
        len.push([]);
        for (var j = 0; j <= m; j++)
            len[i].push(0);
    }
    for (i = n - 1; i >= 0; i--) {
        for (j = m - 1; j >= 0; j--)
            len[i][j] = la[i] === lb[j] ? len[i + 1][j + 1] + 1 : Math.max(len[i + 1][j], len[i][j + 1]);
    }
    var out = { "onlyA": [], "onlyB": [], "caseA": [] };
    i = 0;
    j = 0;
    while (i < n && j < m) {
        if (la[i] === lb[j]) {
            if (a[i] !== b[j])
                out.caseA.push(i);
            i++;
            j++;
        }
        else if (len[i + 1][j] >= len[i][j + 1])
            out.onlyA.push(i++);
        else
            out.onlyB.push(j++);
    }
    while (i < n)
        out.onlyA.push(i++);
    while (j < m)
        out.onlyB.push(j++);
    return out;
}

// a character as shown: spaces you can't see (at the ends, doubled, or not a
// plain space) get a sign of their own
function shownChar(ch) {
    if (ch === " ")
        return "·";
    if (ch === "\t")
        return "⇥";
    if (/\s/.test(ch) || ch === "　")
        return "⍽";
    return ch;
}
function escapeHtml(text) {
    return String(text).replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");
}
function unusualSpace(ch) {
    return ch !== " " && (/\s/.test(ch) || ch === "　");
}

// a spelling for a Text in StyledText, between ‹ ›: the characters that
// differ from `main` (another spelling of the genre; "" for none) in amber
// and underlined, a space as · where it differs or isn't a plain space
function markup(name, main) {
    var s = String(name);
    var d = main === "" || main === undefined || main === null ? { "onlyA": [], "caseA": [] } : diff(s, main);
    var marked = {};
    for (var i = 0; i < d.onlyA.length; i++)
        marked[d.onlyA[i]] = true;
    for (i = 0; i < d.caseA.length; i++)
        marked[d.caseA[i]] = true;
    var out = "";
    for (i = 0; i < s.length; i++) {
        var ch = s[i];
        var hot = marked[i] || unusualSpace(ch);
        var shown = hot ? shownChar(ch) : ch;
        // (a plain space as &nbsp;: styled text would fold two into one)
        var text = shown === " " ? "&nbsp;" : escapeHtml(shown);
        out += hot ? "<font color=\"#e8b06a\"><u>" + text + "</u></font>" : text;
    }
    return "‹" + out + "›";
}

// in words, how `name` differs from `main`: the characters it has extra and
// the ones it's missing (with their codes, ×n when more than one), and letter
// case ("" when it's the same text)
function note(name, main) {
    var s = String(name), t = String(main);
    if (s === t)
        return "";
    var d = diff(s, t);
    var parts = [];
    var count = function(list, text) {
        var order = [], n = {};
        for (var i = 0; i < list.length; i++) {
            var ch = text[list[i]];
            if (!(ch in n)) {
                n[ch] = 0;
                order.push(ch);
            }
            n[ch]++;
        }
        return { "order": order, "n": n };
    };
    var more = count(d.onlyA, s), less = count(d.onlyB, t);
    for (var i = 0; i < more.order.length; i++) {
        var ch = more.order[i];
        parts.push("extra " + shownChar(ch) + " " + code(ch) + (more.n[ch] > 1 ? " ×" + more.n[ch] : ""));
    }
    for (i = 0; i < less.order.length; i++) {
        var other = less.order[i];
        parts.push("missing " + shownChar(other) + " " + code(other) + (less.n[other] > 1 ? " ×" + less.n[other] : ""));
    }
    if (d.caseA.length > 0)
        parts.push("letter case");
    return parts.join(" · ");
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
