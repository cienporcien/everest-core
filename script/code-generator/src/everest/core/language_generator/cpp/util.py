from pathlib import Path
import subprocess
import shutil

def snake_case(word: str) -> str:
    """Convert capital case to snake case
    Only alphanumerical characters are allowed.  Only inserts camelcase
    between a consecutive lower and upper alphabetical character and
    lowers first letter
    """

    out = ''
    if len(word) == 0:
        return out
    cur_char: str = ''
    for i in range(len(word)):
        if i == 0:
            cur_char = word[i]
            if not cur_char.isalnum():
                raise Exception('Non legal character in: ' + word)
            out += cur_char.lower()
            continue
        last_char: str = cur_char
        cur_char = word[i]
        if (last_char.islower() and last_char.isalpha() and cur_char.isupper() and cur_char.isalpha):
            out += '_'
        if not cur_char.isalnum():
            out += '_'
        else:
            out += cur_char.lower()

    return out


def create_dummy_result(json_type) -> str:
    def primitive_to_sample_value(type):
        if type == 'boolean':
            return 'true'
        elif type == 'integer':
            return '42'
        elif type == 'number':
            return '3.14'
        elif type == 'string':
            return '"everest"'
        elif type == 'object':
            return '{}'
        else:
            raise Exception(f'This json type "{type}" is not known or not implemented')

    if isinstance(json_type, list):
        return '{}'  # default initialization for variant
    else:
        return primitive_to_sample_value(json_type)
    
def get_implementation_file_paths(interface: str, impl_name: str) -> tuple[str, str]:
    common_part = f'{impl_name}/{interface}'
    return (
        f'{common_part}Impl.hpp',
        f'{common_part}Impl.cpp'
    )

def clang_format(clang_format_config_dir: Path, file_suffix: str, content: str) -> str:
    # check if we handle cpp and hpp files
    if file_suffix not in ('.hpp', '.cpp'):
        return content

    clang_format_executable = shutil.which('clang-format')
    if clang_format_executable is None:
        raise RuntimeError('Could not find clang-format executable - needed when passing clang-format config file')

    if not clang_format_config_dir.is_dir():
        raise RuntimeError(f'Supplied directory for the clang-format file ({clang_format_config_dir}) does not exist')

    if not (clang_format_config_dir / '.clang-format').exists():
        raise RuntimeError(f'Supplied directory for the clang-format file '
                           f'({clang_format_config_dir}) does not contain a .clang-format file')

    run_parms = {'capture_output': True, 'cwd': clang_format_config_dir, 'encoding': 'utf-8', 'input': content}

    format_cmd = subprocess.run([clang_format_executable, '--style=file'], **run_parms)

    if format_cmd.returncode != 0:
        raise RuntimeError(f'clang-format failed with:\n{format_cmd.stderr}\n{content}')

    return format_cmd.stdout
