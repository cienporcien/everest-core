from abc import ABC, abstractmethod
from enum import Enum
from dataclasses import dataclass, field
from pathlib import Path

from everest.framework.schema import DefinitionParser
from everest.framework.model import TypeDefinition, create_type_reference_from_url, Module

from .common import resolve_everest_dir_path, get_module_manifest
from .file_management import FileCreator, FileCreationMap, filter_files

GeneratorMethod = Enum('GeneratorMethod', ['create_module', 'update_module',
                       'generate_interface', 'generate_loader', 'generate_type'])


@dataclass
class GeneratorConfig:
    working_directory: Path
    everest_tree: list[Path]
    schema_directory: Path

    # FIXME (aw): should do some validation here!


@dataclass
class CreateModuleOptions:
    overwrite_if_exists: bool
    show_diff: bool
    file_filter: list[str]


class ILanguageGenerator(ABC):
    def __init__(self, config: GeneratorConfig):
        self._config = config
        self._validator = DefinitionParser(config.schema_directory)
        self._type_cache: dict[str, tuple[TypeDefinition, float]] = {}
        self._file_creator = FileCreator()
        self._update_mode = False  # NOTE (aw): questionable, if this really belongs here

    def create_module(self, options: CreateModuleOptions, full_qualified_module_name: str):
        # FIXME (aw): remove me from here
        self._update_mode = True

        (_, _, module_name) = full_qualified_module_name.rpartition('/')

        manifest_path = get_module_manifest(self._config.working_directory, full_qualified_module_name)
        module_model = self._validator.load_validated_module_def(manifest_path, module_name)

        # here we shall add the doc files
        module_files = self._generate_module_files(module_model, manifest_path.parent)

        module_files = filter_files(module_files, options.file_filter)

        if not module_files:
            return

        self._file_creator(module_files, show_diff=options.show_diff)

    @abstractmethod
    def _generate_module_files(self, module_model: Module, module_path: Path) -> FileCreationMap:
        pass

    @abstractmethod
    def update_module(self):
        pass

    @abstractmethod
    def generate_interface(self):
        pass

    @abstractmethod
    def generate_loader(self):
        pass

    @abstractmethod
    def generate_type(self):
        pass

    def _get_interface_model(self, interface_name: str):
        interface_path = resolve_everest_dir_path(self._config.everest_tree, f'interfaces/{interface_name}.yaml')

        interface_model = self._validator.load_validated_interface_def(interface_path, interface_name)

        last_mtime = interface_path.stat().st_mtime

        return interface_model, last_mtime

    def _get_type_model(self, type_reference_url: str):
        found = self._type_cache.get(type_reference_url, None)
        if found:
            return found

        # not found
        ref = create_type_reference_from_url(type_reference_url)

        postfix = f'types/{ref.unit}.yaml'
        type_unit_path = resolve_everest_dir_path(self._config.everest_tree, postfix)

        type_model = self._validator.load_validated_type_def(type_unit_path, ref.name)

        last_mtime = type_unit_path.stat().st_mtime

        self._type_cache[type_reference_url] = type_model, last_mtime

        return type_model, last_mtime
