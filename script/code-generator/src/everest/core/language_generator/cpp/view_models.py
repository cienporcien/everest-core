from dataclasses import dataclass

from everest.framework.model import RawJsonType


@dataclass
class TypeViewModel:
    id: str
    fully_qualified_type_name: str

    @property
    def fqtn(self):
        return self.fully_qualified_type_name

    @property
    def dummy_result(self):
        result_map = {
            'null': 'nullptr',
            'boolean': 'true',
            'integer': '42',
            'float': '3.14',
            'string': '"everest"',
            'variant': '{}',
            'object': '{}',
            'reference': '{}'
        }

        return result_map[self.id]


@dataclass
class TypedItem:
    name: str
    type: TypeViewModel


@dataclass
class ImplementationViewModel:
    id: str
    type: str
    desc: str
    config: list[TypedItem]
    class_name: str
    class_header: str
    cpp_file_rel_path: str
    base_class: str


@dataclass
class RequirementViewModel:
    id: str
    is_vector: bool
    type: str
    class_name: str


@dataclass
class CommandViewModel:
    name: str
    args: list[TypedItem]
    result: TypedItem


@dataclass
class InterfaceMetaViewModel:
    base_class_header: str
    interface: str
    desc: str
    type_headers: list[str]


@dataclass
class InterfaceViewModel:
    vars: list[TypedItem]
    cmds: list[CommandViewModel]
    name: str
    # info: InterfaceMetaViewModel


@dataclass
class ModuleMetaViewModel:
    name: str
    enable_external_mqtt: bool
    enable_telemetry: bool


@dataclass
class ModuleViewModel:
    provides: list[ImplementationViewModel]
    requires: list[RequirementViewModel]
    config: list[TypedItem]
    info: ModuleMetaViewModel
