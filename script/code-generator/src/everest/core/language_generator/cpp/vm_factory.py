import everest.framework.model as model
import everest.framework.model.types as types

from . import view_models as vm
from .util import snake_case, get_implementation_file_paths


class ViewModelFactory:
    def __init__(self):
        pass

    def create_typed_item(self, name: str, type: types.Type):

        type_id_mapping: dict[str, str] = {
            'null': 'std::nullptr_t',
            'integer': 'int',
            'float': 'double',
            'string': 'std::string',
            'boolean': 'bool',
            'array': 'Array',
            'object': 'Object'        }

        fqtn: str = None

        if isinstance(type, types.VariantType):
            type_ids = [e.ID for e in type.items]
            fqtn = [type_id_mapping[e] for e in type_ids if e != 'null'].sort()
            if 'null' in type_ids:
                fqtn.insert(0, type_id_mapping['null'])
            
            fqtn = ', '.join(fqtn)
            fqtn = f'std::variant<{fqtn}>'
        elif isinstance(type, types.TypeReference):
            fqtn = 'types::' + '::'.join(type.namespaces) + f'::{type.name}'
        else:
            fqtn = type_id_mapping[type.ID]

        return vm.TypedItem(
            name=name,
            type=vm.TypeViewModel(type.ID, fqtn)
        )

    def create_implementation(self, impl: model.Implementation, impl_name: str):
        interface = impl.interface
        (impl_hpp_path, impl_cpp_path) = get_implementation_file_paths(impl.interface, impl_name)
        return vm.ImplementationViewModel(
            id=impl_name,
            type=interface,
            desc=impl.description,
            config=[self.create_typed_item(name, e.type) for name, e in impl.config.items()],
            class_name=f'{impl.interface}Impl',
            class_header=impl_hpp_path,
            cpp_file_rel_path=impl_cpp_path,
            base_class=f'{interface}ImplBase',
        )

    def create_requirement(self, req: model.Requirement, req_name: str):
        print(req_name, (req.min_connections != 1 or req.max_connections != 1))
        return vm.RequirementViewModel(
            id=req_name,
            is_vector=(req.min_connections != 1 or req.max_connections != 1),
            type=req.interface,
            class_name=f'{req.interface}Intf',
        )

    def create_module_meta(self, module: model.Module):
        return vm.ModuleMetaViewModel(
            name=module.name,
            enable_external_mqtt=module.enable_external_mqtt,
            enable_telemetry=module.enable_telemetry
        )

    def create_module(self, module: model.Module):
        return vm.ModuleViewModel(
            provides=[self.create_implementation(e, name) for name, e in module.implements.items()],
            requires=[self.create_requirement(e, name) for name, e in module.requires.items()],
            config=[self.create_typed_item(name, e.type) for name, e in module.config.items()],
            info=self.create_module_meta(module)
        )

    def create_command(self, command: model.Command, name: str):
        return vm.CommandViewModel(
            name=name,
            args=[self.create_typed_item(e.name, e.type) for e in command.arguments],
            result=self.create_typed_item('result', command.result.type) if command.result else None
        )

    # def create_implementation_meta(self, interface: Interface):
    #     return ImplementationMetaViewModel(
    #         base_class_header=f'generated/interfaces/{interface.name}/Implementation.hpp',
    #         desc=interface.description,
    #         interface=interface.name,
    #         type_headers=[],

    #     )

    def create_interface(self, interface: model.Interface):
        return vm.InterfaceViewModel(
            cmds=[self.create_command(e, name) for name, e in interface.commands.items()],
            vars=[self.create_typed_item(name, e.type) for name, e in interface.signals.items()],
            name=interface.name
        )
