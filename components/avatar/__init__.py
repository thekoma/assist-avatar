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
# Internal: list of validated RGB-light configs (one per home-anim colour role),
# injected by the per-state validator so their IDs are declared during config
# validation (lambda id() resolution only sees validation-phase declared IDs —
# minting in to_code alone is too late).
CONF_COLORS = "colors"

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

_DEFAULT_SPEED = {"min": 0.3, "max": 2.0, "default": 1.0}


def _hex_to_rgb01(hex_str):
    """'#00FFD9' -> (0.0, 1.0, 0.851) percentages for the light initial_state."""
    h = str(hex_str).lstrip("#")
    r = int(h[0:2], 16) / 255.0
    g = int(h[2:4], 16) / 255.0
    b = int(h[4:6], 16) / 255.0
    return r, g, b


def _discover():
    """Return modules sorted by id: list of dicts
    {id, name, header, function, src, variations, speed, colors}."""
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
        })
    return mods


# Discover once at import so CONFIG_SCHEMA can validate `animation` against the
# known catalogue. to_code re-discovers to pick up the full per-module dicts.
_MODS = _discover()
_MOD_BY_ID = {m["id"]: m for m in _MODS}
_MOD_IDS = [m["id"] for m in _MODS]


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
    includes = "".join('#include "%s"\n' % m["header"] for m in mods)
    cases = ""
    for i, m in enumerate(mods):
        cases += "    case %d: %s(it, now_ms, cs, speed, variation); break;\n" % (
            i, m["function"])
    body = (
        "#pragma once\n"
        # esphome.h auto-includes component headers alphabetically (avatar/ before
        # display/ and color/), so include the concrete deps explicitly here.
        '#include "esphome/core/color.h"\n'
        '#include "esphome/components/display/display.h"\n'
        + includes +
        "namespace avatar {\n"
        "template<typename D>\n"
        "void dispatch(D &it, int module_idx, uint32_t now_ms, const avatar::ColorSet &cs, "
        "float speed, uint8_t variation) {\n"
        "  it.fill(esphome::Color(0,0,0));\n"
        "  switch (module_idx) {\n"
        + cases +
        "    default: %s(it, now_ms, cs, speed, variation); break;\n" % mods[0]["function"] +
        "  }\n}\n}  // namespace avatar\n"
    )
    write_file_if_changed(_COMPONENT_DIR / "avatar_dispatch.h", body)


def _color_light_config(state, role_color, idx, total):
    """A validated RGB_LIGHT_SCHEMA dict for one colour role. Output id is the
    stable <state>_<role> the lambda reads; LightState id is <state>_<role>_light.
    Declared HERE (validation phase) so lambda id() resolution finds them; minted
    in to_code. Seeds RESTORE_AND_ON; initial_state is the first-boot default.
    The HA NAME is generic ("<State> accent"[ N]), NOT the role name: the animation
    (hence its role) is user-changeable per state, so a role-based name would
    mislead once a different animation is picked."""
    role = role_color["role"]
    out_id = "%s_%s" % (state, role)
    r, g, b = _hex_to_rgb01(role_color["default"])
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
    # name ("<DisplayName> animation") are auto-generated. variation/speed are
    # nested sub-schemas; their id/name are likewise optional and auto-derived. A
    # device YAML may still set any of them to override.
    base = select.select_schema(AvatarSelect).extend({
        cv.Required(CONF_ANIMATION): cv.one_of(*_MOD_IDS),
        # The variation select (home anim's variations). Minted only when the
        # home anim HAS variations AND a `variation:` block is present (use an
        # empty block `variation: {}` to request it); id/name auto-derived.
        cv.Optional(CONF_VARIATION): select.select_schema(AvatarSelect),
        # The speed number (home anim's speed min/max/default). Always present
        # (defaulted below in _pre to an empty block) so a speed control exists
        # for every state with no per-state boilerplate.
        cv.Optional(CONF_SPEED): number.number_schema(AvatarNumber),
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
        # raw is the user's state mapping (pre-validation). Seed id/name for the
        # flattened anim select and the variation/speed sub-blocks here so the
        # entity-base validator inside `base` sees a valid id+name. Default the
        # speed block to {} so a speed number is always minted.
        if not isinstance(raw, dict):
            return raw
        _seed(raw, "%s_anim" % state, "%s animation" % disp, AvatarSelect)
        raw.setdefault(CONF_SPEED, {})
        _seed(raw[CONF_SPEED], "%s_speed" % state,
              "%s animation speed" % disp, AvatarNumber)
        if CONF_VARIATION in raw and raw[CONF_VARIATION] is not None:
            _seed(raw[CONF_VARIATION], "%s_variation" % state,
                  "%s animation variation" % disp, AvatarSelect)
        return raw

    def _post(conf):
        # Inject one validated RGB light per home-anim colour role. Done as a
        # post-validator (not a static schema key) because the role set depends
        # on the chosen `animation`; the device YAML never declares colour blocks.
        home = _MOD_BY_ID[conf[CONF_ANIMATION]]
        conf[CONF_COLORS] = [_color_light_config(state, c, i, len(home["colors"]))
                             for i, c in enumerate(home["colors"])]
        return conf

    return cv.All(_pre, base, _post)


def _build_schema():
    schema = {cv.GenerateID(): cv.declare_id(cg.Component)}  # hub placeholder
    for st in STATES:
        schema[cv.Optional(st)] = _state_schema(st)
    return cv.Schema(schema).extend(cv.COMPONENT_SCHEMA)


CONFIG_SCHEMA = _build_schema()


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
    options = [m["name"] for m in _MODS]
    for st in STATES:
        if st not in config:
            continue
        conf = config[st]
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
        # Anim select: full catalogue (the device YAML's id/name apply here).
        # Seed to the configured animation's index so a fresh flash lands on the
        # right option without manual HA action (spec §4).
        sel = await select.new_select(conf, options=options)
        await cg.register_component(sel, conf)
        initial_idx = options.index(home["name"])
        cg.add(sel.set_initial_index(initial_idx))
        # Variation select: only mint when the home anim declares variations AND
        # the device YAML provided a `variation:` block. When absent, the device
        # lambda passes 0 literally. The select's option order is the manifest
        # `variations` list order, which EQUALS the module render()'s variation
        # index (verified: orb's switch maps 0=siri,1=calm,…,5=happy, matching its
        # manifest variations: [siri, calm, sleeping, agitated, spike, happy]).
        varsel = None
        if CONF_VARIATION in conf and home["variations"]:
            varsel = await select.new_select(
                conf[CONF_VARIATION], options=home["variations"]
            )
            await cg.register_component(varsel, conf[CONF_VARIATION])
            cg.add(varsel.set_initial_index(0))
        # Speed number: home anim's range/default.
        spd = None
        if CONF_SPEED in conf:
            sp = home["speed"]
            spd = await number.new_number(
                conf[CONF_SPEED],
                min_value=sp["min"],
                max_value=sp["max"],
                step=0.1,
            )
            cg.add(spd.set_initial_value(float(sp["default"])))
            await cg.register_component(spd, conf[CONF_SPEED])
        # Register this phase in the runtime table (avatar_render_state.h) so the
        # page lambda's avatar::render_state(it, <PHASE>, millis()) resolves the
        # minted anim-select / variation-select / speed-number / colour outputs.
        # phase_id == STATES.index(st) == the avatar::Phase enum value (IDLE..NO_HA
        # = 0..8). A zero-length C++ array is illegal, so the no-colour case uses
        # the (…, nullptr, 0) overload form.
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
        sel_id = "%s" % sel
        var_id = "%s" % varsel if varsel is not None else "nullptr"
        spd_id = "%s" % spd if spd is not None else "nullptr"
        if color_vars:
            col_ids = ", ".join("%s" % v for v in color_vars)
            cg.add(cg.RawStatement(
                "{ avatar::AvatarColorOutput* _c[] = {%s}; "
                "avatar::register_state(%d, %s, %s, %s, _c, %d); }"
                % (col_ids, phase_id, sel_id, var_id, spd_id, len(color_vars))))
        else:
            cg.add(cg.RawStatement(
                "avatar::register_state(%d, %s, %s, %s, nullptr, 0);"
                % (phase_id, sel_id, var_id, spd_id)))
