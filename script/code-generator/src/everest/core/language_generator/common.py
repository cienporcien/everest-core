from pathlib import Path


def resolve_everest_dir_path(everest_tree: list[Path], postfix: str):
    for everest_dir in everest_tree:
        path = everest_dir / postfix
        if path.exists():
            return path

    raise Exception(f'Could not resolve "{postfix}" in any of the provided everest-dir ({everest_tree}).')

def get_module_manifest(work_dir: Path, rel_mod_dir: str) -> Path:
    return work_dir / f'modules/{rel_mod_dir}/manifest.yaml'
