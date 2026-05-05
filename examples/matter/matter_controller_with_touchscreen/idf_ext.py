import importlib.util
import sys
from pathlib import Path


def _bmgr_config_callback(target_name: str, ctx, args, **kwargs) -> None:
    raise Exception('should not be called')


_FAKE_ACTIONS = {
    'actions': {
        'gen-bmgr-config': {
            'callback': _bmgr_config_callback,
            'options': [],
            'short_help': 'Generate ESP Board Manager configuration files',
        },
        'bmgr': {
            'callback': _bmgr_config_callback,
            'options': [],
            'short_help': 'Run ESP Board Manager',
        },
    }
}


def _load_module(file_path: Path, module_name: str | None = None):
    module_name = module_name or file_path.stem
    spec = importlib.util.spec_from_file_location(module_name, file_path)
    if spec is None or spec.loader is None:
        raise ImportError(f"Failed to create spec for {file_path}")
    mod = importlib.util.module_from_spec(spec)
    sys.modules[module_name] = mod
    spec.loader.exec_module(mod)
    return mod


def _inject_psram_defaults(project_path: str) -> None:
    defaults_path = (
        Path(project_path) / "components" / "gen_bmgr_codes" / "board_manager.defaults"
    )
    if not defaults_path.exists():
        return

    board_name = None
    for line in defaults_path.read_text(encoding="utf-8").splitlines():
        if line.startswith('CONFIG_ESP_BOARD_NAME="') and line.endswith('"'):
            board_name = line[len('CONFIG_ESP_BOARD_NAME="') : -1]
            break

    if board_name in {"esp_box_lite", "esp_box_3"}:
        extra = "CONFIG_SPIRAM_MODE_OCT=y\n"
    elif board_name == "m5stack_cores3":
        extra = "CONFIG_SPIRAM_MODE_QUAD=y\n"
    else:
        return

    content = defaults_path.read_text(encoding="utf-8")
    if extra.strip() in content:
        return

    with defaults_path.open("a", encoding="utf-8") as f:
        if content and not content.endswith("\n"):
            f.write("\n")
        f.write(extra)


def action_extensions(base_actions: dict, project_path: str) -> dict:
    bmgr_path = (
        Path(project_path) / "managed_components" / "espressif__esp_board_manager"
    )
    if not (bmgr_path / 'idf_ext.py').exists():
        return _FAKE_ACTIONS

    bmgr_config = _load_module(
        bmgr_path / "idf_ext.py", "matter_controller_with_touchscreen_bmgr_idf_ext"
    )
    actions = bmgr_config.action_extensions(base_actions, project_path)

    for name in ("bmgr", "gen-bmgr-config"):
        if name not in actions.get("actions", {}):
            continue
        original = actions["actions"][name]["callback"]

        def _wrapped_callback(target_name, ctx, args, _original=original, **kwargs):
            _original(target_name, ctx, args, **kwargs)
            _inject_psram_defaults(project_path)

        actions["actions"][name]["callback"] = _wrapped_callback

    return actions
