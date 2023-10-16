from abc import ABC, abstractmethod
from enum import Enum
from dataclasses import dataclass, field, asdict
from pathlib import Path

from everest.framework.schema import DefinitionParser
from everest.framework.model import Module, Interface
from everest.framework.model.types import get_topological_sorted_type_list_from_unit

from .common import resolve_everest_dir_path, get_module_manifest
from .file_management import FileCreator, FileCreationMap, filter_files

GeneratorMethod = Enum('GeneratorMethod', ['create_module', 'update_module',
                       'generate_interface', 'generate_loader', 'generate_type'])


@dataclass
class GeneratorConfig:
    working_directory: Path
    everest_tree: list[Path]
    schema_directory: Path
    generated_output_directory: Path

    # FIXME (aw): should do some validation here!


@dataclass
class GenerateModuleOptions:
    overwrite_if_exists: bool
    update_only: bool
    show_diff: bool
    file_filter: list[str]

@dataclass
class GenerateInterfaceOptions:
    show_diff: bool

@dataclass
class GenerateRuntimeOptions:
    output_dir: Path


@dataclass
class GenerateTypeOptions:
    overwrite_if_exists: bool
    show_diff: bool


class ILanguageGenerator(ABC):
    def __init__(self, config: GeneratorConfig):
        self._config = config
        self._validator = DefinitionParser(config.schema_directory)
        self._type_cache: dict[str, tuple[TypeDefinition, float]] = {}
        self._file_creator = FileCreator()

    def generate_module(self, options: GenerateModuleOptions, full_qualified_module_name: str):

        (_, _, module_name) = full_qualified_module_name.rpartition('/')

        manifest_path = get_module_manifest(self._config.working_directory, full_qualified_module_name)
        module_model = self._validator.load_validated_module(manifest_path, module_name)

        # FIXME (aw): where to place the doc files?
        module_files = self._generate_module_files(module_model, manifest_path.parent, options.update_only)

        module_files = filter_files(module_files, options.file_filter)

        self._file_creator(module_files, overwrite=options.overwrite_if_exists, show_diff=options.show_diff)

    @abstractmethod
    def _generate_module_files(self, module_model: Module, module_path: Path, update_only: bool) -> FileCreationMap:
        pass

    def generate_interface(self, options: GenerateInterfaceOptions, interface_name: str):
        print(f'I shall create the interface {interface_name}')
        interface_model, _ = self._get_interface_model(interface_name[0])

        interface_files = self._generate_interface_files(interface_model)
        self._file_creator(interface_files, show_diff=options.show_diff)

    @abstractmethod
    def _generate_interface_files(self, interface_model: Interface) -> FileCreationMap:
        pass

    def generate_loader(self, full_qualified_module_name: str):
        (_, _, module_name) = full_qualified_module_name.rpartition('/')

        manifest_path = get_module_manifest(self._config.working_directory, full_qualified_module_name)
        module_model = self._validator.load_validated_module(manifest_path, module_name)

        # FIXME (aw): this loader might already be cpp specific, so how to deal with that?
        loader_files = self._generate_loader_files(module_model, manifest_path.parent)

        if not loader_files:
            return

        # FIXME (aw): overwrite / force-update handling
        self._file_creator(loader_files, overwrite=True)

    def generate_type(self, options: GenerateTypeOptions, types: list[str]):
        sample_type = types[0]
        postfix = f'types/{sample_type}.yaml'
        type_unit_path = resolve_everest_dir_path(self._config.everest_tree, postfix)

        unit_model = self._validator.load_validated_type_unit(type_unit_path, sample_type)

        print(unit_model)
        get_topological_sorted_type_list_from_unit(unit_model)

    def _get_interface_model(self, interface_name: str):
        interface_path = resolve_everest_dir_path(self._config.everest_tree, f'interfaces/{interface_name}.yaml')

        interface_model = self._validator.load_validated_interface(interface_path, interface_name)

        last_mtime = interface_path.stat().st_mtime

        return interface_model, last_mtime

    # def _get_type_model(self, type_reference_url: str):
    #     found = self._type_cache.get(type_reference_url, None)
    #     if found:
    #         return found

    #     # not found
    #     ref = create_type_reference_from_url(type_reference_url)

    #     postfix = f'types/{ref.unit}.yaml'
    #     type_unit_path = resolve_everest_dir_path(self._config.everest_tree, postfix)

    #     type_model = self._validator.load_validated_type_def(type_unit_path, ref.name)

    #     last_mtime = type_unit_path.stat().st_mtime

    #     self._type_cache[type_reference_url] = type_model, last_mtime

    #     return type_model, last_mtime
