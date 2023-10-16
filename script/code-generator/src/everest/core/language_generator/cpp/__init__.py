from pathlib import Path

from everest.framework.model import Module, Interface

from everest.core.language_generator.interface import ILanguageGenerator, GeneratorConfig
from everest.core.language_generator.file_management import FileCreationInfo, FileCreationStrategy, FileCreationMap


from .renderer import Renderer, TemplateID
from .vm_factory import ViewModelFactory
from .util import get_implementation_file_paths, snake_case, clang_format
from .blocks import Blocks
from .structuring import IncludeBatch

blocks = Blocks()


def dataclass_to_shallow_dict(d: any):
    return {name: getattr(d, name) for name in vars(d)}


class CppGenerator(ILanguageGenerator):
    def __init__(self, config: GeneratorConfig, disable_clang_format: bool = True, clang_format_base_path: Path = None):
        super().__init__(config)

        self._disable_clang_format = disable_clang_format
        self._clang_format_base_path = clang_format_base_path

        self._render = Renderer()
        self._vm_factory = ViewModelFactory()

    def _clang_format(self, files: list[FileCreationInfo]):
        if self._disable_clang_format:
            return

        for file in files:
            file.content = clang_format(self._clang_format_base_path, file.path.suffix, file.content)

    def _generate_module_core_files(self, module: Module, module_path: Path, update_only: bool):
        module_vm = self._vm_factory.create_module(module)
        hpp_file_path = module_path / f'{module.name}.hpp'

        last_mtime = (module_path / 'manifest.yaml').stat().st_mtime  # FIXME

        # FIXME (aw): do we need to sort these?
        implementation_headers = [f'<generated/interfaces/{e.type}/Implementation.hpp>' for e in module_vm.provides]
        requirement_headers = [f'<generated/interfaces/{e.type}/Interface.hpp>' for e in module_vm.requires]

        files_to_create: list[FileCreationInfo] = []

        template_data = {
            'blocks': blocks.get_module_hpp_blocks(hpp_file_path, update_only),
            'module': module_vm,
            'hpp_guard': snake_case(module.name).upper() + '_HPP',
            'config': module_vm.config,
            'class_name': module.name,
            'includes': [
                IncludeBatch(None, ['"ld-ev.hpp"']),
                IncludeBatch('implementations', implementation_headers),
                IncludeBatch('requirements', requirement_headers),
            ]
        }

        files_to_create.append(
            FileCreationInfo(
                abbreviation='module.hpp',
                printable_name=f'{module.name}.hpp',
                content=self._render(TemplateID.MODULE_HPP, template_data),
                path=hpp_file_path,
                last_mtime=last_mtime,
                strategy=FileCreationStrategy.update if update_only else FileCreationStrategy.create
            )
        )

        cmakelists_file_path = module_path / 'CMakeLists.txt'

        template_data = {
            'blocks': blocks.get_cmakelist_blocks(cmakelists_file_path, update_only),
            'impl_files': [e.cpp_file_rel_path for e in module_vm.provides]
        }

        files_to_create.append(
            FileCreationInfo(
                abbreviation='cmakelists',
                printable_name='CMakeLists.txt',
                content=self._render(TemplateID.CMAKEFILE, template_data),
                path=cmakelists_file_path,
                last_mtime=last_mtime,
                strategy=FileCreationStrategy.update if update_only else FileCreationStrategy.create
            )
        )

        if not update_only:
            template_data = {
                'module': module_vm,
                'includes': [
                    IncludeBatch(None, [f'"{module.name}.hpp"'])
                ],
                'class_name': module.name,
            }

            files_to_create.append(
                FileCreationInfo(
                    abbreviation='module.cpp',
                    printable_name=f'{module.name}.cpp',
                    content=self._render(TemplateID.MODULE_CPP, template_data),
                    path=module_path / f'{module.name}.cpp',
                    last_mtime=last_mtime
                )
            )

        return files_to_create

    def _generate_module_implementation_files(self, module: Module, module_path: Path, update_only: bool):
        impl_files: list[FileCreationInfo] = []
        for impl_name, impl in module.implements.items():
            interface_name = impl.interface

            interface, last_mtime = self._get_interface_model(interface_name)  # FIXME (aw): last_mtime!

            interface_vm = self._vm_factory.create_interface(interface)

            hpp_file_name, cpp_file_name = get_implementation_file_paths(impl.interface, impl_name)

            implementation_class_name = f'{interface_name}Impl'
            implementation_base_class_name = f'{interface_name}ImplBase'

            template_data = {
                'interface': interface_vm,
                'blocks': blocks.get_implementation_hpp_blocks(module_path / hpp_file_name, update_only),
                'config': [self._vm_factory.create_typed_item(name, e.type) for name, e in impl.config.items()],
                'class_name': implementation_class_name,
                'base_class_name': implementation_base_class_name,
                'module_class_name': module.name,
                'id': impl_name,
                'includes': [
                    IncludeBatch(None, [f'<generated/interfaces/{interface.name}/Implementation.hpp>']),
                    IncludeBatch(None, [f'"../{module.name}.hpp"'])
                ],
                'hpp_guard': snake_case(f'{impl_name}_{interface_name}').upper() + '_IMPL_HPP',
                'namespace': impl_name,
            }

            impl_files.append(FileCreationInfo(
                abbreviation=f'{impl_name}.hpp',
                printable_name=hpp_file_name,
                content=self._render(TemplateID.INTF_IMPL_HPP, template_data),
                path=module_path / hpp_file_name,
                last_mtime=last_mtime,
                strategy=FileCreationStrategy.update if update_only else FileCreationStrategy.create

            ))

            if not update_only:
                template_data = {
                    'interface': interface_vm,
                    'includes': [
                        IncludeBatch(None, [f'"{implementation_class_name}.hpp"'])
                    ],
                    'class_name': implementation_class_name,
                    'namespace': impl_name,
                }

                impl_files.append(FileCreationInfo(
                    abbreviation=f'{impl_name}.cpp',
                    printable_name=cpp_file_name,
                    content=self._render(TemplateID.INTF_IMPL_CPP, template_data),
                    path=module_path / cpp_file_name,
                    last_mtime=last_mtime
                ))

        return impl_files

    def _generate_module_files(self, module: Module, module_path: Path, update_only: bool):
        core_files = self._generate_module_core_files(module, module_path, update_only)
        implementation_files = self._generate_module_implementation_files(module, module_path, update_only)

        self._clang_format(core_files + implementation_files)

        return {
            'core': core_files,
            'implementation': implementation_files
        }

    def _generate_loader_files(self, module: Module, module_path: Path):
        loader_files: list[FileCreationInfo] = []
        module_vm = self._vm_factory.create_module(module)

        last_mtime = (module_path / 'manifest.yaml').stat().st_mtime  # FIXME
        out_dir = self._config.generated_output_directory / 'modules' / module.name

        template_data = {
            'hpp_guard': 'LD_EV_HPP',
            'includes': [
                IncludeBatch('framework', ['<framework/ModuleAdapter.hpp>', '<framework/everest.hpp>']),
                IncludeBatch('logging', ['<everest/logging.hpp>'])
            ]
        }

        loader_files.append(FileCreationInfo(
            abbreviation='ld-ev.hpp',
            printable_name=f'{module.name}/ld-ev.hpp',
            content=self._render(TemplateID.LOADER_HPP, template_data),
            path=out_dir / 'ld-ev.hpp',
            last_mtime=last_mtime
        ))

        implementation_headers = [f'"{e.id}/{e.type}Impl.hpp"' for e in module_vm.provides]

        template_data = {
            'module': module_vm,
            'includes': [
                IncludeBatch(None, ['"ld-ev.hpp"']),
                IncludeBatch('framework', ['<framework/runtime.hpp>', '<utils/types.hpp>']),
                IncludeBatch('module header', [f'"{module.name}.hpp"']),
                IncludeBatch('implementations', implementation_headers)
            ],
            'module_class_name': module.name,
            'module_name': module.name,
        }

        loader_files.append(FileCreationInfo(
            abbreviation='ld-ev.cpp',
            printable_name=f'{module.name}/ld-ev.cpp',
            content=self._render(TemplateID.LOADER_CPP, template_data),
            path=out_dir / 'ld-ev.cpp',
            last_mtime=last_mtime
        ))

        self._clang_format(loader_files)

        return {
            'loader': loader_files
        }

    def _generate_interface_files(self, interface_model: Interface) -> FileCreationMap:
        interface_vm = self._vm_factory.create_interface(interface_model)

        parent_output_dir = self._config.generated_output_directory / 'include/generated/interfaces'
        interface_output_name = f'{interface_model.name}/Interface.hpp'

        template_data = {
            'interface': interface_vm,
            'hpp_guard': snake_case(interface_model.name).upper() + '_INTERFACE_HPP',
            'class_name': f'{interface_model.name}Intf',
        }

        export_file = FileCreationInfo(
            abbreviation='interface-export',
            printable_name=interface_output_name,
            path=parent_output_dir / interface_output_name,
            content=self._render(TemplateID.INTF_EXPORT_HPP, template_data),
            last_mtime=None,
        )

        # loader_files.append(FileCreationInfo(
        #     abbreviation='ld-ev.cpp',
        #     printable_name=f'{module.name}/ld-ev.cpp',
        #     content=self._render(TemplateID.LOADER_CPP, template_data),
        #     path=out_dir / 'ld-ev.cpp',
        #     last_mtime=last_mtime
        # ))

        return {
            'export': [export_file]
        }
