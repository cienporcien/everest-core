import everest.framework.model as model

from . import view_models as vm
from .util import snake_case, get_implementation_file_paths


class ViewModelFactory:
    def __init__(self):
        pass

    def create_typed_item(self, name: str, json_type):
        def _get_cpp_type(json_type):
            _map: dict[str, str] = {
                "null": "std::nullptr_t",
                "integer": "int",
                "number": "double",
                "string": "std::string",
                "boolean": "bool",
                "array": "Array",
                "object": "Object",
            }

            if not isinstance(json_type, list):
                return _map[json_type]

            cpp_type = [_map[e] for e in json_type if e != 'null'].sort()
            if 'null' in json_type:
                cpp_type.insert(0, _map['null'])

            return cpp_type

        return vm.TypedItem(
            name=name,
            is_variant=isinstance(json_type, list),
            json_type=json_type,
            cpp_type=_get_cpp_type(json_type)
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
            base_class_header=f'generated/interfaces/{interface}/Implementation.hpp'
        )

    def create_requirement(self, req: model.Requirement, req_name: str):
        return vm.RequirementViewModel(
            id=req_name,
            is_vector=(req.min_connections != 1 or req.max_connections != 1),
            type=req.interface,
            class_name=f'{req.interface}Intf',
            exports_header=f'generated/interfaces/{req.interface}/Interface.hpp'
        )

    def create_module_meta(self, module: model.Module):
        return vm.ModuleMetaViewModel(
            name=module.name,
            class_name=module.name,
            desc=module.description,
            hpp_guard=snake_case(module.name).upper() + '_HPP',
            module_header=f'{module.name}.hpp',
            module_config=[self.create_typed_item(name, e.type) for name, e in module.config.items()],
            ld_ev_header='ld-ev.hpp',
            enable_external_mqtt=module.enable_external_mqtt,
            enable_telemetry=module.enable_telemetry
        )

    def create_module(self, module: model.Module):
        return vm.ModuleViewModel(
            provides=[self.create_implementation(e, name) for name, e in module.implements.items()],
            requires=[self.create_requirement(e, name) for name, e in module.requires.items()],
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
            info=vm.InterfaceMetaViewModel(
                base_class_header=f'generated/interfaces/{interface.name}/Implementation.hpp',
                interface=interface.name,
                desc=interface.description,
                type_headers=[]
            )
        )
