from pathlib import Path
from dataclasses import dataclass

import yaml
import jsonschema

from .model import create_module_from_definition, create_interface_from_definition, create_type_from_definition, create_type_unit_from_definition


class Validator:
    interface: jsonschema.Draft7Validator
    module: jsonschema.Draft7Validator
    config: jsonschema.Draft7Validator
    type: jsonschema.Draft7Validator

    def __init__(self, schema_dir: Path):
        for validator_name, schema_name in zip(['interface', 'module', 'config', 'type'], ['interface', 'manifest', 'config', 'type']):
            try:
                schema = yaml.safe_load((schema_dir / f'{schema_name}.yaml').read_text())
                jsonschema.Draft7Validator.check_schema(schema)
                setattr(self, validator_name, jsonschema.Draft7Validator(schema))
            except OSError as err:
                print(f'Could not open schema file {err.filename}: {err.strerror}')
                exit(1)
            except jsonschema.SchemaError as err:
                print(f'Schema error in schema file {schema_name}.yaml')
                raise
            except yaml.YAMLError as err:
                raise Exception(f'Could not parse interface definition file {schema_dir}') from err


class DefinitionParser:
    def __init__(self, schema_dir: Path):
        # FIXME (aw): we should also patch the schemas like in everest-framework
        self._pack = Validator(schema_dir)
        self._file_cache: dict[Path, any] = {}

    def load_validated_module(self, module_manifest: Path, module_name: str):
        module_def = self._load_yaml(module_manifest, 'module')

        return create_module_from_definition(module_def, module_name)

    def load_validated_type(self, type_file: Path, type_name: str):
        type_def = self._load_yaml(type_file, 'type')

        return create_type_from_definition(type_def, type_name)
    
    def load_validated_type_unit(self, type_file: Path, unit_name: str):
        type_def = self._load_yaml(type_file, 'type')

        return create_type_unit_from_definition(type_def, unit_name)

    def load_validated_interface(self, interface_file: Path, interface_name: str):
        interface_def = self._load_yaml(interface_file, 'interface')

        try:
            for var_def in interface_def.get('vars', {}).values():
                jsonschema.Draft7Validator.check_schema(var_def)

            for cmd_def in interface_def.get('cmds', {}).values():
                for arg_def in cmd_def.get('arguments', {}).values():
                    jsonschema.Draft7Validator.check_schema(arg_def)
                result_def = cmd_def.get('result', None)
                if result_def:
                    jsonschema.Draft7Validator.check_schema(result_def)
        except jsonschema.ValidationError as err:
            raise Exception(f'Validation error in interface definition file {interface_file}: {err}') from err

        return create_interface_from_definition(interface_def, interface_name)

    def _load_yaml(self, file_path: Path, type: str):
        cache = self._file_cache.get(file_path, None)
        if cache:
            return cache

        try:
            entity_def = yaml.safe_load(file_path.read_text())
            validator: jsonschema.Draft7Validator = getattr(self._pack, type)
            validator.validate(entity_def)
        except OSError as err:
            raise Exception(f'Could not open {type} definition file {err.filename}: {err.strerror}') from err
        except yaml.YAMLError as err:
            raise Exception(f'Could not parse {type} definition file {file_path}') from err
        except jsonschema.ValidationError as err:
            raise Exception(f'Validation error in {type} definition file {file_path}') from err

        self._file_cache[file_path] = entity_def
        return entity_def
