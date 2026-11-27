// Pegasus X: ESRB content descriptors from IGDB, for games that already have
// an ESRB letter (x-esrb). A Cloudflare Worker with a D1 database (free tier).
//
// One endpoint: POST /v1/esrb with
//   { "games": [ { "igdbId": 1234, "esrb": "M" },
//                { "name": "GoldenEye 007", "platform": "n64", "year": 1997, "esrb": "T" } ] }
// (platform: an ES-DE system name, as Pegasus X has it, or an IGDB platform id)
// It answers, in the same order:
//   { "results": [ { "status": "ok", "igdbId": 1234, "esrb": "M",
//                    "descriptors": ["Blood and Gore", ...], "interactive": ["In-Game Purchases"] },
//                  { "status": "none" }, ... ] }
// status: ok          the game, its ESRB letter the same as the one sent: its descriptors
//         none        no ESRB rating on IGDB, no game with exactly that name, platform and
//                     year, or more than one (never a guess)
//         mismatch    IGDB's ESRB letter isn't the one sent ("igdbEsrb"): nothing returned
//         invalid     the game's entry isn't one this takes ("reason")
//         later       IGDB or Cloudflare's free tier is busy: ask again after "retryAfter" s
// Nothing else of IGDB's goes out: only these fixed lookups, only the ESRB
// organization's rating, only its descriptions. Every answer is kept in D1,
// "checked, none found" too, so a game is asked of IGDB once.
//
// Secrets (Worker > Settings > Variables and Secrets, never in this repo):
//   TWITCH_CLIENT_ID, TWITCH_CLIENT_SECRET (a Twitch app's, dev.twitch.tv)
// Binding: DB, the D1 database (schema.sql).

const ENDPOINT = "/v1/esrb";
const MAX_GAMES = 10;            // per request (one IGDB multiquery takes 10 queries)
const MAX_BODY = 16 * 1024;      // bytes
const IP_PER_MINUTE = 30;        // requests per address per minute
const IGDB_PER_SECOND = 4;       // IGDB's limits
const IGDB_OPEN = 8;
const IGDB_TIMEOUT_MS = 15000;
const LETTERS = ["EC", "E", "E10+", "T", "M", "AO", "RP"];

// ES-DE system -> IGDB platform id (from Pegasus X's IGDB scraper, before Oct 28 2026)
const PLATFORMS = {
    "3do": 50, "amiga": 16, "amiga1200": 16, "amiga600": 16, "amigacd32": 114,
    "amstradcpc": 25, "android": 34, "androidapps": 34, "androidgames": 34,
    "apple2": 75, "apple2gs": 115, "arcade": 52, "atomiswave": 52, "cps": 52,
    "cps1": 52, "cps2": 52, "cps3": 52, "fba": 52, "fbneo": 52, "mame": 52,
    "mame-advmame": 52, "model2": 52, "model3": 52, "naomi": 52, "naomi2": 52,
    "naomigd": 52, "atari2600": 59, "atari5200": 66, "atari7800": 60,
    "atari800": 65, "atarixe": 65, "atarist": 63, "atarijaguar": 62,
    "atarilynx": 61, "c64": 15, "colecovision": 68, "dos": 13, "dreamcast": 23,
    "famicom": 99, "fds": 51, "gamegear": 35, "gb": 33, "gba": 24, "gbc": 22,
    "gc": 21, "genesis": 29, "megadrive": 29, "megadrivejp": 29, "intellivision": 67,
    "macintosh": 14, "mark3": 64, "mastersystem": 64, "megacd": 78, "megacdjp": 78,
    "segacd": 78, "msx": 27, "msx1": 27, "msx2": 53, "n3ds": 37, "n64": 4,
    "n64dd": 416, "nds": 20, "neogeo": 80, "neogeocd": 136, "neogeocdjp": 136,
    "nes": 18, "ngage": 42, "ngp": 119, "ngpc": 120, "odyssey2": 133,
    "videopac": 133, "pc": 6, "windows": 6, "steam": 6, "pc88": 125,
    "pc98": 149, "pcengine": 86, "tg16": 86, "pcenginecd": 150, "tg-cd": 150,
    "pcfx": 274, "ps2": 8, "ps3": 9, "ps4": 48, "psp": 38, "psvita": 46,
    "psx": 7, "saturn": 32, "saturnjp": 32, "sega32x": 30, "sega32xjp": 30,
    "sega32xna": 30, "sfc": 58, "sg-1000": 84, "snes": 19, "snesna": 19,
    "supergrafx": 128, "switch": 130, "vectrex": 70, "vic20": 71,
    "virtualboy": 87, "wii": 5, "wiiu": 41, "wonderswan": 57,
    "wonderswancolor": 123, "x68000": 121, "xbox": 11, "xbox360": 12,
    "xboxone": 49, "zxspectrum": 26,
    // (newer systems, by IGDB's ids)
    "ps5": 167, "xboxseries": 169, "switch2": 508,
};

// ESRB's interactive elements, as ESRB names them: kept apart from the content
// descriptors (and only when IGDB lists them on the game's ESRB rating)
const INTERACTIVE = new Set([
    "in-game purchases", "in-game purchases (includes random items)",
    "in-game purchases (includes paid random items)", "users interact",
    "shares location", "shares info", "unrestricted internet", "digital purchases",
]);

// what each lookup asks of a game: its age ratings, as IGDB has them now
// (organization, rating_category, rating_content_descriptions: not the
// deprecated category / rating / content_descriptions)
const AGE_FIELDS = [
    "age_ratings.organization.name",
    "age_ratings.rating_category.rating",
    "age_ratings.rating_content_descriptions.description",
    "age_ratings.rating_content_descriptions.organization",
    "age_ratings.rating_content_descriptions.description_type.name",
    "age_ratings.rating_content_descriptions.description_type.slug",
].join(",");


class Later extends Error {
    constructor(why, after = 2) {
        super(why);
        this.after = after;
    }
}

const now = () => Math.floor(Date.now() / 1000);
const json = (body, status = 200, headers = {}) =>
    new Response(JSON.stringify(body), { status, headers: { "content-type": "application/json", ...headers } });
const later = (after, why) =>
    json({ error: "later", reason: why, retryAfter: after }, 503, { "retry-after": String(after) });


// -- the letters ---------------------------------------------------------------------------

// "e10+", "E10", "ESRB: T" -> "E10+", "T"; anything else -> ""
function letterOf(value) {
    if (typeof value !== "string")
        return "";
    let s = value.toUpperCase().replace(/\s+/g, "");
    if (s.startsWith("ESRB"))
        s = s.slice(4).replace(/^:/, "");
    if (s === "E10")
        s = "E10+";
    return LETTERS.includes(s) ? s : "";
}

// a game's ESRB rating on IGDB: { esrb, descriptors, interactive }, or esrb
// null (none, or two that differ: not sure)
function esrbOf(game) {
    const ratings = (game.age_ratings || []).filter(r =>
        r && r.organization && typeof r.organization.name === "string"
        && r.organization.name.trim().toUpperCase() === "ESRB");
    const letters = new Set(ratings.map(r => letterOf(r.rating_category && r.rating_category.rating)).filter(Boolean));
    if (letters.size !== 1)
        return { esrb: null, descriptors: [], interactive: [] };
    const esrb = [...letters][0];
    const orgId = ratings[0].organization.id;
    const descriptors = [], interactive = [];
    for (const r of ratings) {
        if (letterOf(r.rating_category && r.rating_category.rating) !== esrb)
            continue;
        for (const d of r.rating_content_descriptions || []) {
            if (!d || typeof d.description !== "string" || !d.description.trim())
                continue;
            // (a description of another organization's: not ESRB's)
            const org = typeof d.organization === "object" && d.organization ? d.organization.id : d.organization;
            if (org !== undefined && orgId !== undefined && org !== orgId)
                continue;
            const text = d.description.trim();
            const type = d.description_type ? `${d.description_type.name || ""} ${d.description_type.slug || ""}` : "";
            const isInteractive = /interactive/i.test(type) || INTERACTIVE.has(text.toLowerCase());
            const list = isInteractive ? interactive : descriptors;
            if (!list.includes(text))
                list.push(text);
        }
    }
    return { esrb, descriptors, interactive };
}


// -- what was sent ----------------------------------------------------------------------------

const normName = name => name.normalize("NFC").trim().replace(/\s+/g, " ");

function readGame(g) {
    if (!g || typeof g !== "object")
        return { invalid: "not an object" };
    const esrb = letterOf(g.esrb);
    if (!esrb)
        return { invalid: "esrb: EC, E, E10+, T, M, AO or RP" };
    if (g.igdbId !== undefined && g.igdbId !== null && g.igdbId !== "") {
        const id = Number(g.igdbId);
        if (!Number.isInteger(id) || id <= 0 || id > 2147483647)
            return { invalid: "igdbId" };
        return { esrb, id };
    }
    if (typeof g.name !== "string")
        return { invalid: "igdbId, or name + platform + year" };
    const name = normName(g.name);
    // (quotes and backslashes can't be in IGDB's query: rare in a name)
    if (!name || name.length > 200 || /["\\\u0000-\u001f]/.test(name))
        return { invalid: "name" };
    const platform = typeof g.platform === "number" ? g.platform
        : /^\d+$/.test(String(g.platform || "")) ? Number(g.platform)
        : PLATFORMS[String(g.platform || "").trim().toLowerCase()];
    if (!Number.isInteger(platform) || platform <= 0 || platform > 100000)
        return { invalid: "platform" };
    const year = Number(g.year);
    if (!Number.isInteger(year) || year < 1950 || year > 2100)
        return { invalid: "year" };
    return { esrb, name, platform, year, key: `${name.toLowerCase()}|${platform}|${year}` };
}


// -- limits (kept in D1: every copy of the worker counts) -------------------------------------

async function ipAllowed(db, ip) {
    const minute = Math.floor(Date.now() / 60000);
    const row = await db.prepare(
        "INSERT INTO ip_minutes (ip, minute, n) VALUES (?1, ?2, 1) " +
        "ON CONFLICT (ip, minute) DO UPDATE SET n = n + 1 RETURNING n").bind(ip, minute).first();
    if (Math.random() < 0.05)
        await db.prepare("DELETE FROM ip_minutes WHERE minute < ?1").bind(minute - 2).run();
    return row.n <= IP_PER_MINUTE;
}

const sleep = ms => new Promise(r => setTimeout(r, ms));

// a place for one IGDB request: its id (release it after), or a Later
async function igdbSlot(db) {
    for (let attempt = 0; attempt < 2; attempt++) {
        const second = now();
        const row = await db.prepare(
            "INSERT INTO igdb_seconds (second, n) VALUES (?1, 1) " +
            "ON CONFLICT (second) DO UPDATE SET n = n + 1 RETURNING n").bind(second).first();
        if (row.n <= IGDB_PER_SECOND) {
            await db.prepare("DELETE FROM igdb_open WHERE started < ?1").bind(Date.now() - 30000).run();
            const id = crypto.randomUUID();
            await db.prepare("INSERT INTO igdb_open (id, started) VALUES (?1, ?2)").bind(id, Date.now()).run();
            const open = await db.prepare("SELECT COUNT(*) AS n FROM igdb_open").first();
            if (open.n <= IGDB_OPEN) {
                if (Math.random() < 0.05)
                    await db.prepare("DELETE FROM igdb_seconds WHERE second < ?1").bind(second - 10).run();
                return id;
            }
            await db.prepare("DELETE FROM igdb_open WHERE id = ?1").bind(id).run();
            throw new Later("IGDB: 8 requests open", 2);
        }
        // this second's 4 are taken: wait for the next one, once
        if (attempt === 0)
            await sleep(1000 - (Date.now() % 1000) + 20);
    }
    throw new Later("IGDB: 4 requests this second", 1);
}

const releaseSlot = (db, id) => db.prepare("DELETE FROM igdb_open WHERE id = ?1").bind(id).run();


// -- Twitch and IGDB --------------------------------------------------------------------------

async function token(env, renew) {
    if (!env.TWITCH_CLIENT_ID || !env.TWITCH_CLIENT_SECRET)
        throw new Later("TWITCH_CLIENT_ID / TWITCH_CLIENT_SECRET not set", 60);
    if (!renew) {
        const row = await env.DB.prepare("SELECT access_token, expires_at FROM token WHERE id = 1").first();
        if (row && row.expires_at > now() + 60)
            return row.access_token;
    }
    const body = new URLSearchParams({
        client_id: env.TWITCH_CLIENT_ID, client_secret: env.TWITCH_CLIENT_SECRET, grant_type: "client_credentials",
    });
    let answer;
    try {
        answer = await fetch("https://id.twitch.tv/oauth2/token", {
            method: "POST", body, headers: { "content-type": "application/x-www-form-urlencoded" },
        });
    } catch (e) {
        throw new Later("Twitch: no answer", 10);
    }
    const data = await answer.json().catch(() => ({}));
    if (!answer.ok || !data.access_token)
        throw new Later(`Twitch: ${answer.status} (are the keys right?)`, 60);
    const expires = now() + Math.max(120, Number(data.expires_in) || 0);
    await env.DB.prepare(
        "INSERT INTO token (id, access_token, expires_at) VALUES (1, ?1, ?2) " +
        "ON CONFLICT (id) DO UPDATE SET access_token = ?1, expires_at = ?2").bind(data.access_token, expires).run();
    return data.access_token;
}

// queries: [{ name, query }] (10 at most): their results by name
async function igdb(env, queries) {
    const body = queries.map(q => `query games "${q.name}" {\n${q.query}\n};`).join("\n");
    let key = await token(env, false);
    for (let tries = 0; tries < 2; tries++) {
        const slot = await igdbSlot(env.DB);
        let answer;
        try {
            answer = await fetch("https://api.igdb.com/v4/multiquery", {
                method: "POST", body,
                headers: { "Client-ID": env.TWITCH_CLIENT_ID, "Authorization": `Bearer ${key}`, "content-type": "text/plain" },
                signal: AbortSignal.timeout(IGDB_TIMEOUT_MS),
            });
        } catch (e) {
            throw new Later("IGDB: no answer", 5);
        } finally {
            await releaseSlot(env.DB, slot);
        }
        // the token ran out or was taken back: a new one, once
        if (answer.status === 401 && tries === 0) {
            key = await token(env, true);
            continue;
        }
        if (answer.status === 429)
            throw new Later("IGDB: too many requests", 2);
        if (!answer.ok)
            throw new Later(`IGDB: ${answer.status}`, 10);
        const out = {};
        for (const part of await answer.json())
            out[part.name] = part.result || [];
        return out;
    }
    throw new Later("IGDB: the token wasn't taken", 60);
}


// -- D1: what's known -------------------------------------------------------------------------

const placeholders = n => Array.from({ length: n }, (_, i) => `?${i + 1}`).join(",");

async function knownGames(db, ids) {
    const out = new Map();
    if (!ids.length)
        return out;
    const { results } = await db.prepare(
        `SELECT igdb_id, esrb, descriptors, interactive FROM games WHERE igdb_id IN (${placeholders(ids.length)})`)
        .bind(...ids).all();
    for (const r of results)
        out.set(r.igdb_id, { esrb: r.esrb, descriptors: JSON.parse(r.descriptors), interactive: JSON.parse(r.interactive) });
    return out;
}

async function knownLookups(db, keys) {
    const out = new Map();
    if (!keys.length)
        return out;
    const { results } = await db.prepare(
        `SELECT lookup_key, igdb_id FROM lookups WHERE lookup_key IN (${placeholders(keys.length)})`)
        .bind(...keys).all();
    for (const r of results)
        out.set(r.lookup_key, r.igdb_id);
    return out;
}

const saveGame = (db, id, name, rating) => db.prepare(
    "INSERT INTO games (igdb_id, name, esrb, descriptors, interactive, checked_at) VALUES (?1, ?2, ?3, ?4, ?5, ?6) " +
    "ON CONFLICT (igdb_id) DO UPDATE SET name = ?2, esrb = ?3, descriptors = ?4, interactive = ?5, checked_at = ?6")
    .bind(id, name, rating.esrb, JSON.stringify(rating.descriptors), JSON.stringify(rating.interactive), now());

const saveLookup = (db, key, id, reason) => db.prepare(
    "INSERT INTO lookups (lookup_key, igdb_id, reason, checked_at) VALUES (?1, ?2, ?3, ?4) " +
    "ON CONFLICT (lookup_key) DO UPDATE SET igdb_id = ?2, reason = ?3, checked_at = ?4")
    .bind(key, id, reason, now());


// -- a batch ----------------------------------------------------------------------------------

const yearOf = r => Number.isInteger(r.y) ? r.y
    : Number.isFinite(r.date) ? new Date(r.date * 1000).getUTCFullYear() : null;

// a name + platform + year lookup's games: the one, or null (none, several)
function pickExact(games, want) {
    const name = want.name.toLowerCase();
    const hits = games.filter(g =>
        typeof g.name === "string" && normName(g.name).toLowerCase() === name
        && (g.release_dates || []).some(r => r && r.platform === want.platform && yearOf(r) === want.year));
    return { game: hits.length === 1 ? hits[0] : null, reason: hits.length === 1 ? "found" : hits.length ? "several" : "none" };
}

async function lookUp(env, wanted) {
    const db = env.DB;
    const lookups = await knownLookups(db, [...new Set(wanted.filter(w => w.key).map(w => w.key))]);
    const ids = new Set(wanted.filter(w => w.id).map(w => w.id));
    for (const w of wanted) {
        if (w.key && lookups.get(w.key))
            ids.add(lookups.get(w.key));
    }
    const games = await knownGames(db, [...ids]);

    // asked of IGDB: the ids not known, the names not looked up (one query each)
    const newIds = [...ids].filter(id => !games.has(id));
    const newKeys = [...new Set(wanted.filter(w => w.key && !lookups.has(w.key)).map(w => w.key))];
    const queries = [];
    if (newIds.length)
        queries.push({ name: "ids", query: `fields id,name,${AGE_FIELDS}; where id = (${newIds.join(",")}); limit ${newIds.length};` });
    const byQuery = {};
    newKeys.forEach((key, i) => {
        const w = wanted.find(x => x.key === key);
        byQuery[`n${i}`] = w;
        queries.push({
            name: `n${i}`,
            query: `fields id,name,release_dates.platform,release_dates.y,release_dates.date,${AGE_FIELDS}; ` +
                   `where name ~ "${w.name}" & platforms = (${w.platform}); limit 20;`,
        });
    });
    let laterWhy = null, laterAfter = 0;
    for (let at = 0; at < queries.length; at += 10) {
        const part = queries.slice(at, at + 10);
        let answer;
        try {
            answer = await igdb(env, part);
        } catch (e) {
            if (!(e instanceof Later))
                throw e;
            laterWhy = e.message;
            laterAfter = Math.max(laterAfter, e.after);
            break;
        }
        const writes = [];
        for (const q of part) {
            const result = answer[q.name] || [];
            if (q.name === "ids") {
                const found = new Map(result.map(g => [g.id, g]));
                for (const id of newIds) {
                    const g = found.get(id);
                    const rating = g ? esrbOf(g) : { esrb: null, descriptors: [], interactive: [] };
                    games.set(id, rating);
                    writes.push(saveGame(db, id, g ? g.name : null, rating));
                }
                continue;
            }
            const w = byQuery[q.name];
            const { game, reason } = pickExact(result, w);
            lookups.set(w.key, game ? game.id : null);
            writes.push(saveLookup(db, w.key, game ? game.id : null, reason));
            if (game) {
                const rating = esrbOf(game);
                games.set(game.id, rating);
                writes.push(saveGame(db, game.id, game.name, rating));
            }
        }
        if (writes.length)
            await db.batch(writes);
    }

    return wanted.map(w => {
        if (w.invalid)
            return { status: "invalid", reason: w.invalid };
        const id = w.id || (w.key ? lookups.get(w.key) : null);
        if (w.key && !lookups.has(w.key))
            return { status: "later", retryAfter: laterAfter || 2 };
        if (!id)
            return { status: "none" };
        const rating = games.get(id);
        if (!rating)
            return { status: "later", igdbId: id, retryAfter: laterAfter || 2 };
        if (!rating.esrb)
            return { status: "none", igdbId: id };
        if (rating.esrb !== w.esrb)
            return { status: "mismatch", igdbId: id, igdbEsrb: rating.esrb };
        return { status: "ok", igdbId: id, esrb: rating.esrb, descriptors: rating.descriptors, interactive: rating.interactive };
    }).map(r => (laterWhy && r.status === "later" ? { ...r, reason: laterWhy } : r));
}


export default {
    async fetch(request, env) {
        const url = new URL(request.url);
        if (url.pathname !== ENDPOINT)
            return json({ error: "not found" }, 404);
        if (request.method !== "POST")
            return json({ error: "POST only" }, 405, { allow: "POST" });
        if (!env.DB)
            return later(60, "the D1 database isn't bound as DB");
        try {
            const ip = request.headers.get("cf-connecting-ip") || "unknown";
            if (!(await ipAllowed(env.DB, ip)))
                return json({ error: "later", reason: "too many requests from this address", retryAfter: 60 }, 429, { "retry-after": "60" });
            const text = await request.text();
            if (text.length > MAX_BODY)
                return json({ error: `at most ${MAX_BODY} bytes` }, 413);
            let body;
            try {
                body = JSON.parse(text);
            } catch (e) {
                return json({ error: "not JSON" }, 400);
            }
            if (!body || !Array.isArray(body.games) || body.games.length === 0)
                return json({ error: "games: a list" }, 400);
            if (body.games.length > MAX_GAMES)
                return json({ error: `at most ${MAX_GAMES} games a request` }, 400);
            const results = await lookUp(env, body.games.map(readGame));
            const waits = results.filter(r => r.status === "later").map(r => r.retryAfter);
            return json(waits.length ? { results, retryAfter: Math.max(...waits) } : { results });
        } catch (e) {
            // Cloudflare's free tier (D1 limits, the worker's own) and anything
            // unexpected: try again later
            return later(60, e instanceof Later ? e.message : "busy (Cloudflare's free tier?)");
        }
    },
};
