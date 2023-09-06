from dataclasses import dataclass
from typing import Union

JsonType = Union[str, list[str]]


@dataclass
class TypeReference:
    name: str
    unit: str

    @property
    def namespaces(self) -> list[str]:
        return self.unit.split('/')


@dataclass
class TypeDefinition:
    name: str
    type: JsonType


def create_type_reference_from_url(url: str):
    if not url.startswith('/'):
        raise Exception(f'Type reference url needs to start with /: {url}')

    if '#/' not in url:
        raise Exception(f'Type reference url needs to refer to a specific type with #/TypeName: {url}')

    unit, name = url.lstrip('/').split('#/')
    return TypeReference(name=name, unit=unit)


@dataclass
class ConfigItem:
    description: str
    type: JsonType
    default: any


@dataclass
class Metadata:
    license: str
    authors: list[str]


@dataclass
class CommandArgument:
    name: str
    type: JsonType


@dataclass
class CommandResult:
    type: JsonType


@dataclass
class Command:
    arguments: list[CommandArgument]
    result: CommandResult


@dataclass
class Signal:
    type: JsonType


@dataclass
class Interface:
    name: str
    description: str
    commands: dict[str, Command]
    signals: dict[str, Signal]


@dataclass
class Implementation:
    description: str
    interface: str
    config: dict[str, ConfigItem]


@dataclass
class Requirement:
    interface: str
    min_connections: int
    max_connections: int


@dataclass
class Module:
    name: str
    description: str
    config: dict[str, ConfigItem]
    implements: dict[str, Implementation]
    requires: dict[str, Requirement]
    metadata: Metadata
    enable_external_mqtt: bool
    enable_telemetry: bool


def create_metadata_from_definition(d: dict) -> Metadata:
    return Metadata(
        license=d.get('license'),
        authors=[e for e in d.get('authors', [])]
    )


def create_config_item_from_definition(d: dict) -> ConfigItem:
    return ConfigItem(
        description=d.get('description'),
        type=d.get('type'),
        default=d.get('default', None)
    )


def create_implementation_from_definition(d: dict) -> Implementation:
    return Implementation(
        description=d.get('description'),
        interface=d.get('interface'),
        config={name: create_config_item_from_definition(e) for name, e in d.get('config', {}).items()}
    )


def create_requirement_from_definition(d: dict) -> Requirement:
    return Requirement(
        interface=d.get('interface'),
        min_connections=d.get('min_connections', 1),
        max_connections=d.get('max_connections', 1)
    )


def create_module_from_definition(d: dict, module_name: str) -> Module:
    return Module(
        name=module_name,
        description=d.get('description'),
        config={name: create_config_item_from_definition(e) for name, e in d.get('config', {}).items()},
        implements={name: create_implementation_from_definition(e) for name, e in d.get('provides').items()},
        requires={name: create_requirement_from_definition(e) for name, e in d.get('requires').items()},
        metadata=create_metadata_from_definition(d.get('metadata')),
        enable_external_mqtt=d.get('enable_external_mqtt', False),
        enable_telemetry=d.get('enable_telemetry', False)
    )


def create_command_from_definition(d: dict) -> Command:
    result: CommandResult = None
    result_d = d.get('result', None)
    if result_d:
        result = CommandResult(result_d.get('type'))

    return Command(
        arguments=[CommandArgument(name, e.get('type')) for name, e in d.get('arguments', {}).items()],
        result=result
    )


def create_signal_from_definition(d: dict) -> Signal:
    return Signal(d.get('type'))


def create_interface_from_definition(d: dict, interface_name: str) -> Interface:
    return Interface(
        name=interface_name,
        description=d.get('description'),
        commands={name: create_command_from_definition(e) for name, e in d.get('cmds', {}).items()},
        signals={name: create_signal_from_definition(e) for name, e in d.get('vars', {}).items()}
    )


def create_type_from_definition(d: dict, type_name: str):
    return TypeDefinition(
        name=type_name,
        type=d.get('types').get(type_name).get('type')
    )
