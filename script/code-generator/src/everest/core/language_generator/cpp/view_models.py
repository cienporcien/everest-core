from dataclasses import dataclass

from everest.framework.model import JsonType


@dataclass
class TypedItem:
    name: str
    is_variant: bool
    json_type: JsonType
    cpp_type: any


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
    base_class_header: str


@dataclass
class RequirementViewModel:
    id: str
    is_vector: bool
    type: str
    class_name: str
    exports_header: str


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
    info: InterfaceMetaViewModel


@dataclass
class ModuleMetaViewModel:
    name: str
    class_name: str
    desc: str
    module_header: str
    hpp_guard: str
    module_config: list[TypedItem]
    ld_ev_header: str
    enable_external_mqtt: bool
    enable_telemetry: bool


@dataclass
class ModuleViewModel:
    provides: dict[str, ImplementationViewModel]
    requires: dict[str, RequirementViewModel]
    info: ModuleMetaViewModel
