from dataclasses import dataclass
from pathlib import Path
import shutil
import subprocess


@dataclass
class FileCreationInfo:
    abbreviation: str
    printable_name: str
    content: str
    path: Path
    last_mtime: float


FileCreationMap = dict[str, list[FileCreationInfo]]


def print_possible_files(files: FileCreationMap):
    print('Possible files')
    for section, file_list in files.items():
        print(f'section: {section}')
        for file in file_list:
            print(f' - {file.abbreviation}')


def filter_files(files: FileCreationMap, filter: list[str]) -> FileCreationMap:
    if not filter or len(filter) == 0:
        return files

    if len(filter) == 1 and 'which' in filter:
        print_possible_files(files)
        return None

    filtered_files: FileCreationMap = {}
    file_names_left = [e for e in filter]

    for section, file_list in files.items():
        for file in file_list:
            if not file.abbreviation in file_names_left:
                continue

            filtered_files.setdefault(section, []).append(file)
            file_names_left.remove(file.abbreviation)

    if len(file_names_left):
        raise Exception(f'Unknown files: ' + ', '.join(file_names_left))

    return filtered_files


def print_diff(file: FileCreationInfo):
    diff_executable = shutil.which('diff')
    if diff_executable == None:
        raise Exception('Cannot produce difference view, because diff executable not found')

    file_path = file.path

    # NOTE (aw): this is language specific
    diff_ignore = ''
    if file_path.suffix in ('.hpp', '.cpp'):
        diff_ignore = '^//.*'
    elif file_path.name == 'CMakeLists.txt':
        diff_ignore = '^#.*'

    diff_ignore_args = ['-I', diff_ignore] if diff_ignore else []

    run_parms = {'input': file.content, 'capture_output': True, 'encoding': 'utf-8'}

    diff = subprocess.run([
        diff_executable,
        '-ruN',
        *diff_ignore_args,
        '--label', file.printable_name,
        '--color=always',
        file_path,
        '-'
    ], **run_parms).stdout

    if diff:
        print(diff)


class FileCreator:
    def __init__(self):
        pass

    def __call__(self, files: FileCreationMap, overwrite=False, show_diff=False):
        flat_files = [file for file_list in files.values() for file in file_list]

        if show_diff:
            for file in flat_files:
                print_diff(file)

            return

        for section, file_list in files.items():
            print(f'Section: {section}')
            for file in file_list:
                print(f' -> {file.abbreviation}')
