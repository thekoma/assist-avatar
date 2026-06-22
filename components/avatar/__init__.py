import os
from pathlib import Path

import esphome.codegen as cg
import esphome.config_validation as cv
import esphome.final_validate as fv
from esphome.components import light, number, select
from esphome.const import (
    CONF_BLUE,
    CONF_BRIGHTNESS,
    CONF_COLOR_BRIGHTNESS,
    CONF_COLOR_MODE,
    CONF_GREEN,
    CONF_ID,
    CONF_INITIAL_STATE,
    CONF_NAME,
    CONF_OUTPUT_ID,
    CONF_RED,
    CONF_RESTORE_MODE,
    CONF_STATE,
)
from esphome.core import ID
from esphome import yaml_util
from esphome.helpers import write_file_if_changed

CODEOWNERS = ["@thekoma"]
AUTO_LOAD = ["select", "number", "light"]

avatar_ns = cg.esphome_ns.namespace("avatar")
AvatarSelect = avatar_ns.class_("AvatarSelect", select.Select, cg.Component)
AvatarNumber = avatar_ns.class_("AvatarNumber", number.Number, cg.Component)
# RGB LightOutput: new_light() wraps it in a LightState and registers THAT as the
# component; this output var (CONF_OUTPUT_ID) is what the page lambda reads via
# id(<state>_<role>)->get().
AvatarColorOutput = avatar_ns.class_("AvatarColorOutput", light.LightOutput)

# The nine assistant phases. Each may optionally be configured as a sub-schema
# carrying its home `animation` plus minted variation/speed controls. The render
# dispatch uses the minted vars captured from new_select/new_number/new_light via
# register_state(...), NOT the entity ids — so the page lambdas never reference
# these ids and auto-generating them is safe.
STATES = ["idle", "listening", "thinking", "replying", "error",
          "muted", "booting", "no_wifi", "no_ha"]

# state -> human display name used as the prefix for every auto-generated control
# name ("<DisplayName> animation", "<DisplayName> animation speed", ...). Kept
# explicit (rather than state.capitalize()) so "no_wifi"->"No Wi-Fi" etc. read
# correctly in Home Assistant.
STATE_DISPLAY = {
    "idle": "Idle",
    "listening": "Listening",
    "thinking": "Thinking",
    "replying": "Replying",
    "error": "Error",
    "muted": "Muted",
    "booting": "Booting",
    "no_wifi": "No Wi-Fi",
    "no_ha": "No HA",
}

CONF_ANIMATION = "animation"
CONF_VARIATION = "variation"
CONF_SPEED = "speed"
CONF_ACCENT = "accent"
# Internal: list of validated RGB-light configs (one per home-anim colour role),
# injected by the per-state validator so their IDs are declared during config
# validation (lambda id() resolution only sees validation-phase declared IDs —
# minting in to_code alone is too late).
CONF_COLORS = "colors"
# Internal: the validated number.number_schema dict for the speed NUMBER entity.
# `speed:` is now a user-facing FLOAT (the first-boot value); the entity config
# (id/name) is auto-seeded into this separate key by _pre so the entity is always
# minted regardless of whether the user set a speed value.
CONF_SPEED_ENTITY = "speed_entity"

# Generated/copied headers are written into THIS component's package dir (NOT the
# build src). copy_src_tree() enumerates a component's resources by scanning its
# package dir on disk after to_code, then copies them prune-safe into the build.
_COMPONENT_DIR = Path(__file__).resolve().parent  # components/avatar/
_ANIM_DIR = _COMPONENT_DIR / "animations"
# Base headers every module needs (source-of-truth in base/, shipped flat, once).
# base/ and animations/ are the source-of-truth subdirs (not flat-copy artifacts).
_BASE = ["base/avatar_math.h", "base/avatar_draw.h", "base/avatar_module.h"]

# esphome.h auto-includes the component headers ALPHABETICALLY. Module headers
# (amber_pulse.h, breathing_ring.h, ...) sort before avatar_dispatch.h, so they
# are parsed before dispatch can pull in color.h/display.h, and esphome.h itself
# is only mid-parse at that point (esphome::Color/display not yet declared). Every
# module includes avatar_module.h, so we prepend the concrete esphome deps to the
# SHIPPED COPY of avatar_module.h (the base/ source is untouched) to guarantee the declarations
# exist regardless of which module header is parsed first.
_MODULE_HEADER = "avatar_module.h"
_FORCE_ESPHOME_DEPS = (
    '#include "esphome/core/color.h"\n'
    '#include "esphome/components/display/display.h"\n'
)

_DEFAULT_SPEED = {"min": 0.3, "max": 10.0, "default": 1.0}


def _accent_hex(value):
    """Validate a first-boot accent colour as a hex string and normalise it to
    '#RRGGBB' (uppercase). Accepts '#FF00AA' or 'FF00AA'. Used for the per-state
    `accent:` override key."""
    if not isinstance(value, str):
        raise cv.Invalid("accent must be a hex colour string like \"#FF00AA\"")
    h = value.strip().lstrip("#")
    if len(h) != 6:
        raise cv.Invalid(
            "accent must be a 6-digit hex colour like \"#FF00AA\" (got %r)" % value)
    try:
        int(h, 16)
    except ValueError as exc:
        raise cv.Invalid(
            "accent must be hexadecimal like \"#FF00AA\" (got %r)" % value) from exc
    return "#" + h.upper()


def _hex_to_rgb01(hex_str):
    """'#00FFD9' -> (0.0, 1.0, 0.851) percentages for the light initial_state."""
    h = str(hex_str).lstrip("#")
    r = int(h[0:2], 16) / 255.0
    g = int(h[2:4], 16) / 255.0
    b = int(h[4:6], 16) / 255.0
    return r, g, b


def _discover():
    """Return modules sorted by id: list of dicts
    {id, name, header, function, src, variations, speed, colors, phases}."""
    mods = []
    for mf in sorted(_ANIM_DIR.glob("*/manifest.yaml")):
        # clear_secrets=False: manifests carry no !secret, and we must not clear
        # the device config's secret cache mid-codegen.
        m = yaml_util.load_yaml(mf, clear_secrets=False)
        entry = m["entry"]
        header_basename = os.path.basename(str(entry["header"]))
        speed = m.get("speed") or _DEFAULT_SPEED
        colors = []
        for c in (m.get("colors") or []):
            colors.append({
                "role": str(c["role"]),
                "name": str(c.get("name", c["role"])),
                "default": str(c.get("default", "#00FFD9")),
            })
        mods.append({
            "id": str(m["id"]),
            "name": str(m["name"]),
            # entry.header is "animations/<id>/<id>.h"; we ship it flat, so just
            # the basename is used in the #include.
            "header": header_basename,
            "function": str(entry["function"]),
            "src": _ANIM_DIR / str(m["id"]) / header_basename,
            "variations": [str(v) for v in (m.get("variations") or [])],
            "speed": {
                "min": float(speed.get("min", _DEFAULT_SPEED["min"])),
                "max": float(speed.get("max", _DEFAULT_SPEED["max"])),
                "default": float(speed.get("default", _DEFAULT_SPEED["default"])),
            },
            # Declared colour roles (manifest colors[]); each becomes a minted RGB
            # light whose latest colour the dispatch reads into a ColorSet slot.
            "colors": colors,
            # The assistant phases this module is the DEFAULT animation for. The
            # state->default-anim map (STATE_DEFAULT_ANIM) is built from these:
            # the first module (in sorted-glob order) claiming a phase wins.
            "phases": [str(p) for p in (m.get("phases") or [])],
        })
    return mods


# Discover once at import so CONFIG_SCHEMA can validate `animation` against the
# known catalogue. to_code re-discovers to pick up the full per-module dicts.
_MODS = _discover()
_MOD_BY_ID = {m["id"]: m for m in _MODS}
_MOD_IDS = [m["id"] for m in _MODS]


def _build_state_default_anim():
    """state id -> default module id, derived from manifests' `phases:` lists.

    Scan _MODS in discovery (sorted-glob) order; the FIRST module declaring a
    phase claims it as that state's default animation. Every one of the 9 STATES
    must resolve to exactly one module — if any state has no claimer, the
    catalogue is incomplete and we fail loudly at import (a config-time bug, not
    a device runtime surprise)."""
    mapping = {}
    for m in _MODS:  # sorted by id; first claimant wins
        for phase in m["phases"]:
            if phase not in STATES:
                continue  # ignore phases that are not assistant states
            mapping.setdefault(phase, m["id"])
    missing = [s for s in STATES if s not in mapping]
    if missing:
        raise ValueError(
            "avatar: no animation manifest declares a default for state(s): %s. "
            "Add the state to some module's `phases:` list." % ", ".join(missing)
        )
    return mapping


# state -> default animation module id (manifest-driven; first sorted module
# claiming a phase wins). An omitted state (or an absent `animation:` key) is
# minted with this default + the full control set.
STATE_DEFAULT_ANIM = _build_state_default_anim()


def _flatten(mods):
    """Expand the module catalogue into the flat animation×variation list.

    Iterates `mods` in discovery (sorted-glob) order. A module with NO variations
    yields ONE entry (variation_idx 0, label = manifest name). A module WITH
    variations yields ONE entry per variation, label "<name> — <Variation>" with
    the variation id capitalised (siri->Siri). The list index IS the flattened
    catalogue index the generated dispatch switch and the anim select share."""
    out = []
    for m in mods:
        if m["variations"]:
            for vi, vname in enumerate(m["variations"]):
                out.append({
                    "label": "%s — %s" % (m["name"], vname.capitalize()),
                    "function": m["function"],
                    "variation_idx": vi,
                    "module_id": m["id"],
                })
        else:
            out.append({
                "label": m["name"],
                "function": m["function"],
                "variation_idx": 0,
                "module_id": m["id"],
            })
    return out


# The flattened catalogue (16 entries): one per animation×variation permutation.
# Index here == the generated dispatch case index == the anim select option index.
_FLAT = _flatten(_MODS)
_FLAT_LABELS = [e["label"] for e in _FLAT]


def _flat_index(module_id, variation_idx):
    """Flattened catalogue index for (module, variation_idx). Callers reach this
    only after validation has confirmed both are valid, so a miss means _FLAT and
    the validation drifted — fail loudly at codegen rather than silently seed 0."""
    for i, e in enumerate(_FLAT):
        if e["module_id"] == module_id and e["variation_idx"] == variation_idx:
            return i
    raise ValueError(
        "_flat_index: (%s, %d) not in the flattened catalogue" % (module_id, variation_idx))


def _ship_headers(mods):
    for rel in _BASE:
        basename = os.path.basename(rel)
        text = (_COMPONENT_DIR / rel).read_text()
        if basename == _MODULE_HEADER:
            # Inject the esphome deps right after the #pragma once so they are the
            # first thing parsed, before this header's own #include "esphome.h".
            text = text.replace(
                "#pragma once\n", "#pragma once\n" + _FORCE_ESPHOME_DEPS, 1
            )
        write_file_if_changed(_COMPONENT_DIR / basename, text)
    for mod in mods:
        write_file_if_changed(_COMPONENT_DIR / mod["header"], mod["src"].read_text())


def _write_dispatch(mods):
    # One #include per module header (dedup; a module may emit several flat cases
    # but its header must be included once).
    seen, includes = set(), ""
    for m in mods:
        if m["header"] not in seen:
            seen.add(m["header"])
            includes += '#include "%s"\n' % m["header"]
    flat = _flatten(mods)
    cases = ""
    for i, e in enumerate(flat):
        cases += "    case %d: %s(it, now_ms, cs, speed, %d); break;  // %s\n" % (
            i, e["function"], e["variation_idx"], e["label"])
    body = (
        "#pragma once\n"
        # esphome.h auto-includes component headers alphabetically (avatar/ before
        # display/ and color/), so include the concrete deps explicitly here.
        '#include "esphome/core/color.h"\n'
        '#include "esphome/components/display/display.h"\n'
        + includes +
        "namespace avatar {\n"
        "template<typename D>\n"
        "void dispatch(D &it, int idx, uint32_t now_ms, const avatar::ColorSet &cs, "
        "float speed) {\n"
        "  it.fill(esphome::Color(0,0,0));\n"
        "  switch (idx) {\n"
        + cases +
        "    default: %s(it, now_ms, cs, speed, 0); break;\n" % flat[0]["function"] +
        "  }\n}\n}  // namespace avatar\n"
    )
    write_file_if_changed(_COMPONENT_DIR / "avatar_dispatch.h", body)


def _color_light_config(state, role_color, idx, total, accent=None):
    """A validated RGB_LIGHT_SCHEMA dict for one colour role. Output id is the
    stable <state>_<role> the lambda reads; LightState id is <state>_<role>_light.
    Declared HERE (validation phase) so lambda id() resolution finds them; minted
    in to_code. Seeds RESTORE_AND_ON; initial_state is the first-boot default.
    The HA NAME is generic ("<State> accent"[ N]), NOT the role name: the animation
    (hence its role) is user-changeable per state, so a role-based name would
    mislead once a different animation is picked.

    `accent` (a per-state hex override, e.g. "#FF00AA") replaces the manifest
    role default as the first-boot colour when given. It applies to ALL roles of
    the state's animation (a state has at most one accent override key)."""
    role = role_color["role"]
    out_id = "%s_%s" % (state, role)
    r, g, b = _hex_to_rgb01(accent if accent else role_color["default"])
    disp = STATE_DISPLAY[state]
    name = ("%s accent" % disp) if total == 1 \
        else ("%s accent %d" % (disp, idx + 1))
    light_conf = {
        CONF_ID: ID("%s_light" % out_id, is_declaration=True, type=light.LightState),
        CONF_OUTPUT_ID: ID(out_id, is_declaration=True, type=AvatarColorOutput),
        CONF_NAME: name,
        CONF_RESTORE_MODE: "RESTORE_AND_ON",
        CONF_INITIAL_STATE: {
            CONF_COLOR_MODE: "RGB",
            CONF_STATE: True,
            CONF_BRIGHTNESS: 1.0,
            CONF_COLOR_BRIGHTNESS: 1.0,
            CONF_RED: r,
            CONF_GREEN: g,
            CONF_BLUE: b,
        },
    }
    # light_schema (not the bare RGB_LIGHT_SCHEMA) adds CONF_OUTPUT_ID = declare_id
    # of our AvatarColorOutput, so the explicit output_id above is a known key.
    return light.light_schema(AvatarColorOutput, light.LightType.RGB)(light_conf)


def _state_schema(state):
    # The anim select schema is FLATTENED into the state level. id/name are
    # OPTIONAL: when absent a stable id ("<state>_anim") and a consistent display
    # name ("<DisplayName> animation") are auto-generated. EVERY key is optional —
    # an omitted state (or an empty `<state>: {}`) is minted with the manifest
    # default animation + the full control set. The user-facing keys are all
    # first-boot DEFAULTS / OVERRIDES; the live values are tuned from HA.
    base = select.select_schema(AvatarSelect).extend({
        # OPTIONAL now: when absent, defaults to STATE_DEFAULT_ANIM[state] (the
        # manifest-driven default for this phase). Resolved in _post.
        cv.Optional(CONF_ANIMATION): cv.one_of(*_MOD_IDS),
        # Optional first-boot DEFAULT VARIATION name (e.g. "siri"). The variation
        # is baked into the single flattened anim select. When set it picks the
        # seeded flat index for the resolved anim; validated against that module's
        # `variations` in _post. When absent the first variation (index 0) is used.
        cv.Optional(CONF_VARIATION): cv.string,
        # First-boot SPEED VALUE (a float like 1.5), validated against the resolved
        # animation's manifest speed.min..max in _post. NOT the number entity
        # config any more — that is auto-seeded into CONF_SPEED_ENTITY below so a
        # speed NUMBER is always minted. When absent, the manifest default is used.
        cv.Optional(CONF_SPEED): cv.float_,
        # First-boot ACCENT colour (a hex like "#FF00AA"). Valid ONLY if the
        # resolved animation declares a colour role; rejected in _post for a
        # palette animation (colors: []). When absent the manifest role default is
        # used. Applies to every colour role of the animation.
        cv.Optional(CONF_ACCENT): _accent_hex,
        # Internal: the speed NUMBER entity config (id/name), auto-seeded in _pre.
        cv.Optional(CONF_SPEED_ENTITY): number.number_schema(AvatarNumber),
        # Internal: filled by _post below (the device YAML never sets it).
        # Declared so the injected per-role light configs are a known key and their
        # IDs are walked by iter_ids during validation.
        cv.Optional(CONF_COLORS): [
            light.light_schema(AvatarColorOutput, light.LightType.RGB)
        ],
    })

    disp = STATE_DISPLAY[state]

    def _seed(raw, sub_id, name, cls):
        # Inject a stable manual id + display name into a RAW (pre-validation) sub-
        # dict, unless the device YAML already set them. Run BEFORE the entity-base
        # validator (which would otherwise reject a nameless, auto-id entity with
        # "At least one of 'id:' or 'name:' is required!"). A manually-declared ID
        # (is_declaration=True) keeps persistence keys stable across builds and makes
        # new_select/new_number emit a global named exactly sub_id; CONF_NAME is
        # honoured by setup_entity, mirroring the colour-light path. We never force
        # CONF_INTERNAL, so the auto-named control is exposed to Home Assistant.
        if CONF_ID not in raw:
            raw[CONF_ID] = ID(sub_id, is_declaration=True, type=cls)
        if CONF_NAME not in raw:
            raw[CONF_NAME] = name
        return raw

    def _pre(raw):
        # raw is the user's state mapping (pre-validation). An absent/`null`/empty
        # state still gets the full control set: normalise null/{} to a dict, then
        # seed id/name for the flattened anim select and the speed-number entity so
        # the entity-base validators see a valid id+name. The user-facing
        # animation/variation/speed/accent keys stay OPTIONAL (resolved in _post).
        if raw is None:
            raw = {}
        if not isinstance(raw, dict):
            return raw
        _seed(raw, "%s_anim" % state, "%s animation" % disp, AvatarSelect)
        raw.setdefault(CONF_SPEED_ENTITY, {})
        _seed(raw[CONF_SPEED_ENTITY], "%s_speed" % state,
              "%s animation speed" % disp, AvatarNumber)
        return raw

    def _post(conf):
        # Resolve the EFFECTIVE animation (user override or manifest default) and
        # validate variation/speed/accent against THAT animation. Then inject one
        # validated RGB light per colour role (with the accent override applied).
        anim_id = conf.get(CONF_ANIMATION) or STATE_DEFAULT_ANIM[state]
        conf[CONF_ANIMATION] = anim_id
        home = _MOD_BY_ID[anim_id]

        # variation: must be a declared variation of the resolved animation.
        if CONF_VARIATION in conf and conf[CONF_VARIATION] not in home["variations"]:
            raise cv.Invalid(
                "'%s' is not a valid variation for animation '%s' (state '%s'). "
                "Valid variations: %s"
                % (conf[CONF_VARIATION], anim_id, state,
                   ", ".join(home["variations"]) or "(none)"),
                [CONF_VARIATION],
            )

        # speed: must be within the resolved animation's manifest range.
        if CONF_SPEED in conf:
            sp, val = home["speed"], float(conf[CONF_SPEED])
            if not (sp["min"] <= val <= sp["max"]):
                raise cv.Invalid(
                    "speed %.3g is out of range for animation '%s' (state '%s'); "
                    "must be between %g and %g."
                    % (val, anim_id, state, sp["min"], sp["max"]),
                    [CONF_SPEED],
                )

        # accent: only valid if the resolved animation declares a colour role; a
        # palette animation (colors: []) rejects it (mirrors the variation error).
        # Already normalised to '#RRGGBB' by _accent_hex.
        accent_hex = conf.get(CONF_ACCENT)
        if accent_hex is not None and not home["colors"]:
            raise cv.Invalid(
                "accent is not valid for animation '%s' (state '%s'): it is "
                "palette-driven (no colour role to recolour). Remove `accent:` or "
                "pick a colour-driven animation." % (anim_id, state),
                [CONF_ACCENT],
            )

        # Inject one validated RGB light per colour role (accent override applied
        # to every role). Done as a post-validator (not a static schema key)
        # because the role set depends on the RESOLVED animation.
        conf[CONF_COLORS] = [
            _color_light_config(state, c, i, len(home["colors"]), accent_hex)
            for i, c in enumerate(home["colors"])
        ]
        return conf

    return cv.All(_pre, base, _post)


def _build_schema():
    schema = {cv.GenerateID(): cv.declare_id(cg.Component)}  # hub placeholder
    for st in STATES:
        # Every state is OPTIONAL: an omitted state is minted with its manifest
        # default animation + the full control set (resolved in to_code).
        schema[cv.Optional(st)] = _state_schema(st)
    return cv.Schema(schema).extend(cv.COMPONENT_SCHEMA)


def _empty_ok(value):
    # `avatar:` with nothing under it parses as None; a single-device install
    # should be able to write just `avatar:` and get all 9 states defaulted.
    # Normalise null -> {} so the schema (all keys optional) accepts it.
    if value is None:
        value = {}
    conf = _BASE_SCHEMA(value)
    # Materialise EVERY state so to_code always mints the full 9-state control
    # set. An absent state runs its own sub-schema with an empty block, which
    # resolves the manifest default animation + seeds the auto ids/colours — i.e.
    # an omitted state is identical to `<state>: {}`.
    for st in STATES:
        if st not in conf:
            conf[st] = _state_schema(st)({})
    return conf


_BASE_SCHEMA = _build_schema()
CONFIG_SCHEMA = _empty_ok


EXPECTED_PAGES = [
    "idle_page", "listening_page", "thinking_page", "replying_page",
    "timer_finished_page", "error_page", "muted_page", "initializing_page",
    "no_wifi_page", "no_ha_page",
]


def _final_validate(config):
    try:
        fconf = fv.full_config.get()          # the merged Config (an OrderedDict) — NOT fconf.data
        displays = fconf.get("display", []) or []
        page_ids, has_render = set(), False
        for disp in displays:
            for pg in (disp.get("pages") or []):
                pid = pg.get("id")
                if pid is not None: page_ids.add(str(pid))
                lam = pg.get("lambda")
                if lam and "render_state" in str(lam): has_render = True
        missing = [p for p in EXPECTED_PAGES if p not in page_ids]
        if missing:
            raise cv.Invalid(
                "avatar: the voice-assistant pages are missing (%s). "
                "Include the esphome.voice-assistant package."
                % ", ".join(missing[:3])
            )
        if not has_render:
            raise cv.Invalid(
                "avatar: no page calls avatar::render_state — include the "
                "avatar-pages.yaml package after the voice-assistant package."
            )
    except cv.Invalid:
        raise
    except Exception:
        pass   # defensive: never crash validation on an unexpected config shape


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config):
    _ship_headers(_MODS)
    _write_dispatch(_MODS)
    # The anim select now lists every animation×variation permutation (16 flat
    # labels), NOT bare module names. Option index == flattened catalogue index
    # == the generated dispatch case index.
    options = list(_FLAT_LABELS)
    # Mint ALL 9 states. The schema materialises every state (an omitted one is
    # resolved to its manifest default animation + auto-seeded controls), so an
    # empty `avatar:` block still produces the full control set.
    for st in STATES:
        conf = config[st]
        # CONF_ANIMATION was RESOLVED to the effective animation (user override or
        # manifest default) in the per-state _post validator.
        home = _MOD_BY_ID[conf[CONF_ANIMATION]]
        # Mint the per-state per-role colour lights (configs pre-validated in the
        # schema so their ids resolve in lambdas). new_light wraps each output in a
        # LightState (the registered Component) and RETURNS the AvatarColorOutput
        # var (its .id is the CONF_OUTPUT_ID == "<state>_<role>"). CAPTURE it so the
        # render-state table can read each role's live colour; the lambda also reads
        # it via id(<state>_<role>)->get(). No-op for palette-only anims (empty
        # colors[], e.g. the orb). Manifest colors[] order == ColorSet slot order,
        # which is the order each module's render() consumes cs.c[i].
        color_vars = []
        for color_conf in conf.get(CONF_COLORS, []):
            cv_out = await light.new_light(color_conf)
            color_vars.append(cv_out)
        # Anim select: the full flattened catalogue (the device YAML's id/name
        # apply here). Seed to the flattened index of (resolved module, chosen
        # default variation) so a fresh flash lands on the right option without
        # manual HA action. The variation was already validated against the
        # resolved animation in _post; here we just resolve its index (absent =>
        # index 0, i.e. the first / single variation).
        sel = await select.new_select(conf, options=options)
        await cg.register_component(sel, conf)
        var_idx = 0
        if CONF_VARIATION in conf:
            var_idx = home["variations"].index(conf[CONF_VARIATION])
        initial_idx = _flat_index(home["id"], var_idx)
        cg.add(sel.set_initial_index(initial_idx))
        # Speed number: ALWAYS minted (entity config auto-seeded into
        # CONF_SPEED_ENTITY). min/max from the resolved animation's manifest;
        # initial value = the per-state `speed:` override when set, else the
        # manifest default.
        sp = home["speed"]
        spd = await number.new_number(
            conf[CONF_SPEED_ENTITY],
            min_value=sp["min"],
            max_value=sp["max"],
            step=0.1,
        )
        initial_speed = float(conf[CONF_SPEED]) if CONF_SPEED in conf \
            else float(sp["default"])
        cg.add(spd.set_initial_value(initial_speed))
        await cg.register_component(spd, conf[CONF_SPEED_ENTITY])
        # Register this phase in the runtime table (avatar_render_state.h) so the
        # page lambda's avatar::render_state(it, <PHASE>, millis()) resolves the
        # minted anim-select / speed-number / colour outputs. The variation is now
        # baked into the flattened anim-select index, so there is no var-select
        # argument. phase_id == STATES.index(st) == the avatar::Phase enum value
        # (IDLE..NO_HA = 0..8). A zero-length C++ array is illegal, so the
        # no-colour case uses the (…, nullptr, 0) overload form.
        #
        # The minted vars are emitted by ESPHome as GLOBAL pointers named exactly by
        # their id (e.g. `static avatar::AvatarSelect *const idle_anim = …`). This
        # RawStatement lands in setup() and is emitted VERBATIM — it is NOT run
        # through the lambda processor, so the `id(x)` macro is unavailable here;
        # the correct reference is the bare variable name, which equals the id.
        # new_select/new_number/new_light return MockObj wrappers whose str() is
        # exactly that bare name ("idle_anim"). We must NOT use `.id`: MockObj's
        # __getattr__ turns `var.id` into the expression `var->id` (a member
        # access), which is wrong here — str(var) is the name we want. For an
        # absent var emit the C++ literal "nullptr".
        phase_id = STATES.index(st)
        # Teach each per-state control its owning phase so a genuine HA change
        # (control()/write_state) can arm a 2s preview of that state (avatar_preview.h).
        # Normal C++ method calls (like set_initial_index) — NOT RawStatements.
        cg.add(sel.set_phase_id(phase_id))
        cg.add(spd.set_phase_id(phase_id))
        for cv_out in color_vars:
            cg.add(cv_out.set_phase_id(phase_id))
        sel_id = "%s" % sel
        spd_id = "%s" % spd
        if color_vars:
            col_ids = ", ".join("%s" % v for v in color_vars)
            cg.add(cg.RawStatement(
                "{ avatar::AvatarColorOutput* _c[] = {%s}; "
                "avatar::register_state(%d, %s, %s, _c, %d); }"
                % (col_ids, phase_id, sel_id, spd_id, len(color_vars))))
        else:
            cg.add(cg.RawStatement(
                "avatar::register_state(%d, %s, %s, nullptr, 0);"
                % (phase_id, sel_id, spd_id)))
