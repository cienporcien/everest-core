import re
from dataclasses import dataclass
from pathlib import Path


@dataclass
class BlockDefinition:
    id: str
    content: str


BlockDefinitions = dict[str, BlockDefinition]


@dataclass
class BlockContent:
    tag: str
    content: str
    first_use: bool


BlockContentMap = dict[str, BlockContent]


@dataclass
class BlockMatch:
    tag: str
    name: str


class BlockCalculator:
    def __init__(self, format: str, regex: str):
        self._format = format
        self._regex = regex

    def calculate(self, definitions: BlockDefinitions, version: str, file_path: Path, update: bool) -> BlockContentMap:
        if update and file_path.exists():
            return self._generate_tmpl_blocks(definitions, version, file_path)
        else:
            return self._generate_tmpl_blocks(definitions, version)

    # FIXME (aw): refactor this, don't need to pass the things for error handling!
    def _check_for_match(self, definitions: BlockDefinitions, version: str, line, line_no, file_path) -> BlockMatch:
        match = re.search(self._regex, line)
        if not match:
            return None

        matched_uuid = match.group('uuid')
        matched_version = match.group('version')

        # check if uuid and version exists
        if version != matched_version:
            raise ValueError(
                f'Error while parsing {file_path}:\n'
                f'  matched line {line_no}: {line}\n'
                f'  contains version "{matched_version}", which is different from the blocks definition version "{version}"'
            )

        for name, definition in definitions.items():
            if definition.id != matched_uuid:
                continue

            return BlockMatch(
                tag=self._format.format(uuid=matched_uuid, version=matched_version),
                name=name
            )

        raise ValueError(
            f'Error while parsing {file_path}:\n'
            f'  matched line {line_no}: {line}\n'
            f'  contains uuid "{matched_uuid}", which doesn\'t exist in the block definition'
        )

    def _generate_tmpl_blocks(self, definitions: BlockDefinitions, version: str, file_path=None) -> BlockContentMap:
        def init_block_content(definition: BlockDefinition) -> BlockContent:
            return BlockContent(
                tag=self._format.format(uuid=definition.id, version=version),
                content=definition.content,
                first_use=True
            )

        blocks = {name: init_block_content(e) for name, e in definitions.items()}

        if not file_path:
            return blocks

        try:
            file_data = file_path.read_text()
        except OSError as err:
            print(f'Could not open file {err.filename} for parsing blocks: {err.strerror}')
            exit(1)  # FIXME (aw): exit strategy?

        line_no = 0
        match: BlockMatch = None
        content = None

        for line in file_data.splitlines(True):
            line_no += 1

            if not match:
                match = self._check_for_match(definitions, version, line.rstrip(), line_no, file_path)
                content = None
                continue

            if (line.strip() == match.tag):
                if (content):
                    block = blocks[match.name]
                    block.content = content.rstrip()
                    block.first_use = False

                match = None
            else:
                content = (content + line) if content else line

        if match:
            raise ValueError(
                f'Error while parsing {file_path}:\n'
                f'  matched tag line {match.tag}\n'
                f'  could not find closing tag'
            )

        return blocks
