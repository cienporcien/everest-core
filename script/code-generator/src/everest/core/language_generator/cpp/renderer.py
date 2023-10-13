from enum import Enum
from datetime import datetime
from pathlib import Path

import jinja2 as j2

from .util import snake_case


class TemplateID(Enum):
    MODULE_HPP = 'module.hpp.j2'
    MODULE_CPP = 'module.cpp.j2'
    LOADER_HPP = 'ld-ev.hpp.j2'
    LOADER_CPP = 'ld-ev.cpp.j2'
    INTF_IMPL_HPP = 'interface-Impl.hpp.j2'
    INTF_IMPL_CPP = 'interface-Impl.cpp.j2'
    CMAKEFILE = 'CMakeLists.txt.j2'


class Renderer:
    def __init__(self):
        env = j2.Environment(loader=j2.FileSystemLoader(Path(__file__).parent / 'templates'),
                             lstrip_blocks=True, trim_blocks=True, undefined=j2.StrictUndefined,
                             keep_trailing_newline=True)

        env.globals['timestamp'] = datetime.utcnow()
        # FIXME (aw): which repo to use? everest or everest-framework?
        env.filters['snake_case'] = snake_case

        self._templates = {e: env.get_template(e.value) for e in TemplateID}

    def __call__(self, id: TemplateID, data):
        return self._templates[id].render(data)
